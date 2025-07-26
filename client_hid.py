import sys
from dotenv import load_dotenv
import os
import hid
import psutil
import speedtest
import spotipy
from spotipy.oauth2 import SpotifyOAuth
import time
import threading
from threading import Lock

load_dotenv()

macropad_vendor_id     = 0xFEED
macropad_product_id    = 0x9A25

loki65_keyboard_vendor_id = 0x8968
loki65_keyboard_product_id = 0x4C37

usage_page    = 0xFF60
usage         = 0x61

report_length = 32

PC_PERFORMANCE = 1
NETWORK_SPEED = 2
CURRENT_SONG = 3
RESET_NETWORK_TEST = 4
RGB_SEND = 5
COULD_NOT_CONNECT = -1

SERVICE_INTERVAL = 1
SONG_NAME_TRUNCATE = 20

SPOTIFY_CLIENT_ID = os.getenv("SPOTIFY_CLIENT_ID")
SPOTIFY_CLIENT_SECRET = os.getenv("SPOTIFY_CLIENT_SECRET")
SPOTIFY_REDIRECT_URI = "http://127.0.0.1:8888/callback/"


class KeyboardManager:
    def __init__(self):
        self.keyboard_device = None
        self.keyboard_lock = Lock()
        self.connection_attempts = 0
        self.max_connection_attempts = 5
        self.last_connection_attempt = 0
        self.connection_retry_delay = 2.0  # seconds
        
    def _find_keyboard_interface(self):
        """Find the Loki65 keyboard raw HID interface"""
        try:
            keyboard_interfaces = hid.enumerate(loki65_keyboard_vendor_id, loki65_keyboard_product_id)
            raw_hid_interfaces = [
                i for i in keyboard_interfaces 
                if i['usage_page'] == usage_page and i['usage'] == usage
            ]
            
            if raw_hid_interfaces:
                return raw_hid_interfaces[0]['path']
            return None
            
        except Exception as e:
            print(f"Error finding keyboard interface: {e}")
            return None
    
    def _connect_keyboard(self):
        """Establish connection to keyboard"""
        current_time = time.time()
        
        # Rate limit connection attempts
        if (current_time - self.last_connection_attempt) < self.connection_retry_delay:
            return False
            
        self.last_connection_attempt = current_time
        
        if self.connection_attempts >= self.max_connection_attempts:
            return False
        
        try:
            keyboard_path = self._find_keyboard_interface()
            if not keyboard_path:
                print("Loki65 raw HID interface not found.")
                self.connection_attempts += 1
                return False
            
            self.keyboard_device = hid.device()
            self.keyboard_device.open_path(keyboard_path)
            self.keyboard_device.set_nonblocking(1)  # Set non-blocking mode
            
            print("Successfully connected to Loki65 keyboard.")
            self.connection_attempts = 0  # Reset on successful connection
            return True
            
        except Exception as e:
            print(f"Failed to connect to keyboard: {e}")
            self.connection_attempts += 1
            if self.keyboard_device:
                try:
                    self.keyboard_device.close()
                except:
                    pass
                self.keyboard_device = None
            return False
    
    def _is_keyboard_connected(self):
        """Check if keyboard is still connected"""
        if not self.keyboard_device:
            return False
        
        try:
            # Try a simple read operation to test connection
            # This should return immediately due to non-blocking mode
            self.keyboard_device.read(1, timeout_ms=0)
            return True
        except Exception:
            return False
    
    def send_layer_data(self, layer_data):
        """Send layer data to keyboard with persistent connection"""
        with self.keyboard_lock:
            # Check if we need to establish/re-establish connection
            if not self.keyboard_device or not self._is_keyboard_connected():
                if not self._connect_keyboard():
                    return False
            
            try:
                # Prepare the report
                report = [0x00] * (report_length + 1)
                report[1] = layer_data
                
                # Send the data
                bytes_written = self.keyboard_device.write(bytes(report))
                
                if bytes_written > 0:
                    print(f"Successfully sent layer '{layer_data}' to Loki65 keyboard.")
                    return True
                else:
                    print("Failed to write data to keyboard")
                    # Connection might be broken, mark for reconnection
                    self._disconnect_keyboard()
                    return False
                    
            except Exception as e:
                print(f"Error sending data to keyboard: {e}")
                # Connection might be broken, mark for reconnection
                self._disconnect_keyboard()
                return False
    
    def _disconnect_keyboard(self):
        """Safely disconnect from keyboard"""
        if self.keyboard_device:
            try:
                self.keyboard_device.close()
            except:
                pass
            self.keyboard_device = None
    
    def cleanup(self):
        """Cleanup keyboard connection"""
        with self.keyboard_lock:
            self._disconnect_keyboard()
            print("Keyboard connection cleaned up.")


class NetworkSpeedTester:
    def __init__(self):
        self.lock = Lock()
        self.is_testing = False
        self.last_result = None
        self.test_start_time = None
        self.test_completion_time = None
        self.auto_reset_delay = 30  # Auto-reset after 30 seconds
        
    def start_test(self):
        """Start network speed test in background thread"""
        with self.lock:
            if self.is_testing:
                return  # Test already in progress
            
            self.is_testing = True
            self.test_start_time = time.time()
            self.last_result = None  # Clear previous results
            
        # Start test in background thread
        thread = threading.Thread(target=self._run_test)
        thread.daemon = True
        thread.start()
    
    def reset_test(self):
        """Reset the network test state"""
        with self.lock:
            if not self.is_testing:  # Only reset if not currently testing
                self.last_result = None
                self.test_completion_time = None
                print("Network test results cleared")
                return True
            return False
    
    def _run_test(self):
        """Run the actual speed test"""
        try:
            print("Starting network speed test...")
            st = speedtest.Speedtest()
            
            # Perform the download speed test
            download_speed = round(st.download() / 1000000, 1)
            
            # Perform the upload speed test
            upload_speed = round(st.upload() / 1000000, 1)
            
            with self.lock:
                self.last_result = (download_speed, upload_speed)
                self.is_testing = False
                self.test_completion_time = time.time()
                
            print(f"Speed test completed: {download_speed}↓ {upload_speed}↑ Mbps")
            
        except Exception as e:
            print(f"Speed test failed: {e}")
            with self.lock:
                self.last_result = None
                self.is_testing = False
                self.test_completion_time = None
    
    def get_status(self):
        """Get current status of speed test"""
        with self.lock:
            if self.is_testing:
                elapsed = time.time() - self.test_start_time
                return "testing", elapsed, None
            elif self.last_result:
                return "completed", 0, self.last_result
            else:
                return "idle", 0, None          

class SpotifyManager:
    def __init__(self):
        self.sp = None
        self.last_song_info = None
        self.last_update_time = 0
        self.init_spotify()
    
    def init_spotify(self):
        """Initialize Spotify client"""
        try:
            if SPOTIFY_CLIENT_ID and SPOTIFY_CLIENT_SECRET:
                self.sp = spotipy.Spotify(auth_manager=SpotifyOAuth(
                    client_id=SPOTIFY_CLIENT_ID,
                    client_secret=SPOTIFY_CLIENT_SECRET,
                    redirect_uri=SPOTIFY_REDIRECT_URI,
                    scope="user-read-playback-state user-read-currently-playing"
                ))
                print("Spotify client initialized successfully")
            else:
                print("Spotify credentials not provided - add your client ID and secret")
        except Exception as e:
            print(f"Failed to initialize Spotify client: {e}")
            self.sp = None
    
    def get_current_song(self):
        """Get current playing song with caching"""
        current_time = time.time()
        
        # Update every 10 seconds to avoid rate limiting
        if current_time - self.last_update_time < 10 and self.last_song_info:
            return self.last_song_info
        
        try:
            if not self.sp:
                return None
                
            current = self.sp.current_playback()
            
            if current and current.get('is_playing'):
                item = current['item']
                song_name = item['name'][:40]  # Limit length for display
                artists = ', '.join([artist['name'] for artist in item['artists']])[:40]
                
                self.last_song_info = (song_name, artists)
                self.last_update_time = current_time
                return self.last_song_info
            else:
                self.last_song_info = None
                self.last_update_time = current_time
                return None
                
        except Exception as e:
            print(f"Error getting current song: {e}")
            return None

# Global instances
speed_tester = NetworkSpeedTester()
spotify_manager = SpotifyManager()
keyboard_manager = KeyboardManager()

def get_raw_hid_interface():
    device_interfaces = hid.enumerate(macropad_vendor_id, macropad_product_id)
    raw_hid_interfaces = [i for i in device_interfaces if i['usage_page'] == usage_page and i['usage'] == usage]

    if len(raw_hid_interfaces) == 0:
        return None

    interface = hid.device()
    interface.open_path(raw_hid_interfaces[0]['path'])
    
    # Set non-blocking mode or use a timeout
    interface.set_nonblocking(1)
    
    return interface

def get_report(data):
    request_data = [0x00] * (report_length + 1)
    request_data[1:len(data) + 1] = data
    return bytes(request_data)

def send_report_with_timeout(interface, request_report):
    if interface is None:
        print("No device found")
        return COULD_NOT_CONNECT

    print("Request:")
    print(request_report)

    try:
        interface.write(request_report)

        response_report = interface.read(report_length, timeout_ms=1000)

        if response_report:
            print("Next service:")
            print(response_report)
        else:
            print("No response received within timeout")

    except Exception as e:
        print(f"Communication error: {e}")
        return COULD_NOT_CONNECT

    return response_report

def send_raw_hid_to_keyboard(data_to_send):
    """
    Send data to keyboard using the persistent keyboard manager
    """
    success = keyboard_manager.send_layer_data(data_to_send)
    if not success:
        print(f"Failed to send layer '{data_to_send}' to keyboard")

def zero_pad(integer):
    if integer <= 9 and integer >= 0:
        return f"0{integer}"
    return str(integer)

psutil.cpu_percent(interval=None) 
def get_pc_stats():
    ram_percent = round(psutil.virtual_memory().percent)
    cpu_percent = round(psutil.cpu_percent(interval=None))

    battery = psutil.sensors_battery()
    bat_percent = 0
    if battery is not None:
        bat_percent = battery.percent

    message = f"{PC_PERFORMANCE}{zero_pad(ram_percent)}|{zero_pad(cpu_percent)}|{zero_pad(bat_percent)}"

    return message.encode('utf-8')

def get_song_info():
    """Get current song information formatted for QMK"""
    song_info = spotify_manager.get_current_song()
    
    if song_info:
        song_name, artists = song_info
    
        first_artist = artists.split(",")[0]

        message = f"{CURRENT_SONG}{song_name[:SONG_NAME_TRUNCATE]}|{first_artist}"
    else:
        message = f"{CURRENT_SONG}--|--"
    
    return message.encode('utf-8')

def get_network_status():
    """Get current network test status and format for QMK"""
    status, elapsed, result = speed_tester.get_status()
    
    if status == "testing":
        # Show progress during test
        elapsed_str = f"{int(elapsed)}s"
        message = f"{NETWORK_SPEED}testing|{elapsed_str}"
    elif status == "completed" and result:
        # Show results
        download, upload = result
        message = f"{NETWORK_SPEED}{download}|{upload}"
    else:
        # No test data available
        message = f"{NETWORK_SPEED}--|--"
    
    return message.encode('utf-8')

def interpret_response(request_report):
    if not request_report or len(request_report) == 0:
        return get_report(get_pc_stats())
    
    request_type = request_report[0]
    
    if request_type == PC_PERFORMANCE:
        return get_report(get_pc_stats())
    elif request_type == NETWORK_SPEED:
        status, _, _ = speed_tester.get_status()
        
        # Only start test if idle (no results yet)
        if status == "idle":
            speed_tester.start_test()
        # If completed or testing, just return current status (don't restart)
        
        return get_report(get_network_status())
    elif request_type == RESET_NETWORK_TEST:
        # Only reset on explicit request
        speed_tester.reset_test()
        speed_tester.start_test()
        return get_report(get_network_status())
    elif request_type == CURRENT_SONG:
        return get_report(get_song_info())
    else:
        # Unknown request, default to PC stats
        return get_report(get_pc_stats())

# rgb to keyboard is an interrupt/separate thread
def hid_read_thread(interface):
    while True:
        report = interface.read(report_length, timeout_ms=100)
        if report:
            if report[0] == RGB_SEND:
                layer = report[1]
                print(f"Received RGB layer interrupt: {layer}")
                send_raw_hid_to_keyboard(layer)

        time.sleep(0.01)


def interface_connect():
    interface = None
    while interface is None:
        interface = get_raw_hid_interface()
        if interface is None:
            print("No device found. Retrying in 5 seconds...")
            time.sleep(5)
    return interface

def main():

    interface = interface_connect()

    # Start thread for RGB interrupts
    threading.Thread(target=hid_read_thread, args=(interface,), daemon=True).start()

    request_report = get_report(get_pc_stats())

    try:
        while True:
            try:
                response_report = send_report_with_timeout(interface, request_report)

                if response_report == COULD_NOT_CONNECT:
                    print("Lost connection. Attempting to reconnect...")
                    interface = interface_connect()                    
                    threading.Thread(target=hid_read_thread, args=(interface,), daemon=True).start()
                    request_report = get_report(get_pc_stats())
                    continue

                request_report = interpret_response(response_report)
                time.sleep(SERVICE_INTERVAL)

            except KeyboardInterrupt:
                print("\nShutting down...")
                break
            except Exception as e:
                print(f"Error in main loop: {e}")
                time.sleep(5)

    finally:
        # Cleanup connections
        print("Cleaning up connections...")
        keyboard_manager.cleanup()
        interface.close()
        print("Cleanup complete.")

if __name__ == '__main__':
    main()