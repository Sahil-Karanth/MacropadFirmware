import os
import sys

# dummy standard outputs for .exe file
sys.stderr = sys.stdout = open(os.devnull, "wb")
PRINT_ON = False

import threading
import time
from threading import Lock, RLock

import hid
import psutil
import speedtest
import spotipy
from dotenv import load_dotenv
from spotipy.oauth2 import SpotifyOAuth

load_dotenv()

macropad_vendor_id = 0xFEED
macropad_product_id = 0x9A25

keyboard_vendor_id = 0x8968
keyboard_product_id = 0x4C37

usage_page = 0xFF60
usage = 0x61

report_length = 32

PC_PERFORMANCE = 1
NETWORK_SPEED = 2
CURRENT_SONG = 3
RESET_NETWORK_TEST = 4
RGB_SEND = 5
TIMER_STATUS = 6
TIMER_PAUSE_REQ = 7
TIMER_RESTART_REQ = 8
TIMER_RESET_REQ = 9
COULD_NOT_CONNECT = -1

SERVICE_INTERVAL = 1
SONG_NAME_TRUNCATE = 20


SPOTIFY_CLIENT_ID = os.getenv("SPOTIFY_CLIENT_ID")
SPOTIFY_CLIENT_SECRET = os.getenv("SPOTIFY_CLIENT_SECRET")
SPOTIFY_REDIRECT_URI = "http://127.0.0.1:8888/callback/"


def debug_print(str):
    if PRINT_ON:
        print(str)


def load_pomodoro_time():
    try:
        with open("pomodoro_duration.txt", "r") as file:
            pomodoro_duration = int(file.read().strip())
        return pomodoro_duration
    except Exception as e:
        debug_print("failed to load the pomodoro_duration.txt file")


pomodoro_duration = load_pomodoro_time()


class KeyboardManager:
    def __init__(self):
        self.keyboard_device = None
        self.keyboard_lock = Lock()
        self.last_connection_attempt = 0
        self.connection_retry_delay = 2.0  # seconds

    def _find_keyboard_interface(self):
        """Find the keyboard raw HID interface"""
        try:
            keyboard_interfaces = hid.enumerate(keyboard_vendor_id, keyboard_product_id)
            raw_hid_interfaces = [
                i
                for i in keyboard_interfaces
                if i["usage_page"] == usage_page and i["usage"] == usage
            ]

            if raw_hid_interfaces:
                return raw_hid_interfaces[0]["path"]
            return None

        except Exception as e:
            debug_print(f"Error finding keyboard interface: {e}")
            return None

    def _connect_keyboard(self):
        """Establish connection to keyboard with persistent retry"""
        current_time = time.time()

        if (current_time - self.last_connection_attempt) < self.connection_retry_delay:
            return False

        self.last_connection_attempt = current_time

        try:
            keyboard_path = self._find_keyboard_interface()
            if not keyboard_path:
                debug_print("Your keyboard raw HID interface was not found.")
                return False

            self.keyboard_device = hid.device()
            self.keyboard_device.open_path(keyboard_path)
            self.keyboard_device.set_nonblocking(1)  # Set non-blocking mode

            debug_print("Successfully connected to your keyboard.")
            return True

        except Exception as e:
            debug_print(f"Failed to connect to keyboard: {e}")
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
            self.keyboard_device.read(1, timeout_ms=0)
            return True
        except Exception:
            return False

    def send_layer_data(self, layer_data):
        """Send layer data to keyboard with persistent connection and retry"""
        with self.keyboard_lock:
            max_retries = 3
            retry_count = 0

            while retry_count < max_retries:
                if not self.keyboard_device or not self._is_keyboard_connected():
                    if not self._connect_keyboard():
                        retry_count += 1
                        if retry_count < max_retries:
                            debug_print(
                                f"Keyboard connection attempt {retry_count} failed, retrying..."
                            )
                            time.sleep(1)
                        continue

                try:
                    report = [0x00] * (report_length + 1)
                    report[1] = layer_data

                    bytes_written = self.keyboard_device.write(bytes(report))

                    if bytes_written > 0:
                        debug_print(
                            f"Successfully sent layer '{layer_data}' to the keyboard."
                        )
                        return True
                    else:
                        debug_print("Failed to write data to keyboard")
                        self._disconnect_keyboard()
                        retry_count += 1

                except Exception as e:
                    debug_print(f"Error sending data to keyboard: {e}")
                    self._disconnect_keyboard()
                    retry_count += 1

            debug_print(f"Failed to send layer data after {max_retries} attempts")
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
            debug_print("Keyboard connection cleaned up.")


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
                return

            self.is_testing = True
            self.test_start_time = time.time()
            self.last_result = None

        thread = threading.Thread(target=self._run_test)
        thread.daemon = True
        thread.start()

    def reset_test(self):
        """Reset the network test state"""
        with self.lock:
            if not self.is_testing:
                self.last_result = None
                self.test_completion_time = None
                debug_print("Network test results cleared")
                return True
            return False

    def _run_test(self):
        """Run the actual speed test"""
        try:
            debug_print("Starting network speed test...")
            st = speedtest.Speedtest()

            download_speed = round(st.download() / 1000000, 1)

            upload_speed = round(st.upload() / 1000000, 1)

            with self.lock:
                self.last_result = (download_speed, upload_speed)
                self.is_testing = False
                self.test_completion_time = time.time()

            debug_print(f"Speed test completed: {download_speed}↓ {upload_speed}↑ Mbps")

        except Exception as e:
            debug_print(f"Speed test failed: {e}")
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


class PomodoroTimer:
    def __init__(self):
        self.lock = RLock()
        self.start_time = None
        self.paused_time = None
        self.total_paused_duration = 0
        self.is_running = False
        self.is_paused = False
        self.is_completed = False
        self.duration = load_pomodoro_time()

    def start(self):
        """Start or resume the timer"""
        with self.lock:
            current_time = time.time()

            if self.is_paused:
                # Resume from pause
                self.total_paused_duration += current_time - self.paused_time
                self.is_paused = False
                self.paused_time = None
                debug_print("Pomodoro timer resumed")
            else:
                # Start new timer
                self.start_time = current_time
                self.total_paused_duration = 0
                self.is_completed = False
                debug_print("Pomodoro timer started")

            self.is_running = True

    def pause(self):
        """Pause the timer"""
        with self.lock:
            if self.is_running and not self.is_paused:
                self.paused_time = time.time()
                self.is_paused = True
                self.is_running = False
                debug_print("Pomodoro timer paused")
                return True
            return False

    def toggle_pause(self):
        """Toggles the pause/resume state of the timer."""
        with self.lock:
            if self.is_completed or not self.start_time:
                return

            if self.is_paused:
                self.start()

            elif self.is_running:
                self.pause()

    def reset(self):
        """Reset the timer to initial state"""
        with self.lock:
            self.start_time = None
            self.paused_time = None
            self.total_paused_duration = 0
            self.is_running = False
            self.is_paused = False
            self.is_completed = False
            debug_print("Pomodoro timer reset")

    def get_status(self):
        """Get current timer status and remaining time"""
        self.duration = load_pomodoro_time()
        with self.lock:
            if not self.start_time:
                return "STOPPED", "00:00:00"

            current_time = time.time()

            if self.is_paused:
                elapsed_time = (
                    self.paused_time - self.start_time - self.total_paused_duration
                )
            else:
                elapsed_time = (
                    current_time - self.start_time - self.total_paused_duration
                )

            remaining_time = max(0, self.duration - elapsed_time)

            if remaining_time <= 0 and not self.is_completed:
                self.is_completed = True
                self.is_running = False
                debug_print("Pomodoro timer completed!")

            hours = int(remaining_time // 3600)
            minutes = int((remaining_time % 3600) // 60)
            seconds = int(remaining_time % 60)
            time_str = f"{hours:02d}:{minutes:02d}:{seconds:02d}"

            if self.is_completed:
                return "COMPLETED", "00:00:00"
            elif self.is_paused:
                return "PAUSED", time_str
            elif self.is_running:
                return "RUNNING", time_str
            else:
                return "STOPPED", time_str


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
                self.sp = spotipy.Spotify(
                    auth_manager=SpotifyOAuth(
                        client_id=SPOTIFY_CLIENT_ID,
                        client_secret=SPOTIFY_CLIENT_SECRET,
                        redirect_uri=SPOTIFY_REDIRECT_URI,
                        scope="user-read-playback-state user-read-currently-playing",
                    )
                )
                debug_print("Spotify client initialized successfully")
            else:
                debug_print(
                    "Spotify credentials not provided - add your client ID and secret"
                )
                pass
        except Exception as e:
            debug_print(f"Failed to initialize Spotify client: {e}")
            self.sp = None

    def get_current_song(self):
        """Get current playing song with caching"""
        current_time = time.time()

        if current_time - self.last_update_time < 10 and self.last_song_info:
            return self.last_song_info

        try:
            if not self.sp:
                return None

            current = self.sp.current_playback()

            if current and current.get("is_playing"):
                item = current["item"]
                song_name = item["name"][:40]
                artists = ", ".join([artist["name"] for artist in item["artists"]])[:40]

                self.last_song_info = (song_name, artists)
                self.last_update_time = current_time
                return self.last_song_info
            else:
                self.last_song_info = None
                self.last_update_time = current_time
                return None

        except Exception as e:
            debug_print(f"Error getting current song: {e}")
            return None


# Global instances
speed_tester = NetworkSpeedTester()
spotify_manager = SpotifyManager()
keyboard_manager = KeyboardManager()
pomodoro_timer = PomodoroTimer()


def get_raw_hid_interface():
    device_interfaces = hid.enumerate(macropad_vendor_id, macropad_product_id)
    raw_hid_interfaces = [
        i
        for i in device_interfaces
        if i["usage_page"] == usage_page and i["usage"] == usage
    ]

    if len(raw_hid_interfaces) == 0:
        return None

    interface = hid.device()
    interface.open_path(raw_hid_interfaces[0]["path"])

    interface.set_nonblocking(1)

    return interface


def get_report(data):
    request_data = [0x00] * (report_length + 1)
    request_data[1 : len(data) + 1] = data
    return bytes(request_data)


def send_report_with_timeout(interface, request_report):
    if interface is None:
        debug_print("No device found")
        return COULD_NOT_CONNECT

    debug_print("Request:")
    debug_print(request_report)

    try:
        interface.write(request_report)

        response_report = interface.read(report_length, timeout_ms=1000)

        debug_print(response_report)

    except Exception as e:
        debug_print(f"Communication error: {e}")
        return COULD_NOT_CONNECT

    return response_report


def send_raw_hid_to_keyboard(data_to_send):
    """
    Send data to keyboard using the persistent keyboard manager
    """
    success = keyboard_manager.send_layer_data(data_to_send)
    if not success:
        debug_print(f"Failed to send layer '{data_to_send}' to keyboard")
        pass


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

    return message.encode("utf-8")


def get_song_info():
    """Get current song information formatted for QMK"""
    song_info = spotify_manager.get_current_song()

    if song_info:
        song_name, artists = song_info

        first_artist = artists.split(",")[0]

        message = f"{CURRENT_SONG}{song_name[:SONG_NAME_TRUNCATE]}|{first_artist}"
    else:
        message = f"{CURRENT_SONG}--|--"

    return message.encode("utf-8")


def get_network_status():
    """Get current network test status and format for QMK"""
    status, elapsed, result = speed_tester.get_status()

    if status == "testing":
        elapsed_str = f"{int(elapsed)}s"
        message = f"{NETWORK_SPEED}testing|{elapsed_str}"
    elif status == "completed" and result:
        download, upload = result
        message = f"{NETWORK_SPEED}{download}|{upload}"
    else:
        message = f"{NETWORK_SPEED}--|--"

    return message.encode("utf-8")


def get_timer_status():
    """Get current timer status and format for QMK"""
    status, time_remaining = pomodoro_timer.get_status()
    message = f"{TIMER_STATUS}{status}|{time_remaining}"
    return message.encode("utf-8")


def interpret_response(request_report):
    if not request_report or len(request_report) == 0:
        return get_report(get_pc_stats())

    request_type = request_report[0]

    if request_type == PC_PERFORMANCE:
        return get_report(get_pc_stats())
    elif request_type == NETWORK_SPEED:
        status, _, _ = speed_tester.get_status()

        if status == "idle":
            speed_tester.start_test()

        return get_report(get_network_status())
    elif request_type == RESET_NETWORK_TEST:
        speed_tester.reset_test()
        speed_tester.start_test()
        return get_report(get_network_status())
    elif request_type == CURRENT_SONG:
        return get_report(get_song_info())
    elif request_type == TIMER_STATUS:
        return get_report(get_timer_status())
    elif request_type == TIMER_PAUSE_REQ:
        pomodoro_timer.toggle_pause()
        return get_report(get_timer_status())
    elif request_type == TIMER_RESTART_REQ:
        pomodoro_timer.start()
        return get_report(get_timer_status())
    elif request_type == TIMER_RESET_REQ:
        pomodoro_timer.reset()
        return get_report(get_timer_status())
    else:
        # Unknown request, default to PC stats
        return get_report(get_pc_stats())


def hid_read_thread(interface):
    while True:
        try:
            report = interface.read(report_length, timeout_ms=100)
            if report:
                if report[0] == RGB_SEND:
                    layer = report[1]
                    debug_print(f"Received RGB layer interrupt: {layer}")
                    send_raw_hid_to_keyboard(layer)
        except Exception:
            # Handle cases where the device might get disconnected
            return
        time.sleep(0.01)


def interface_connect():
    interface = None
    while interface is None:
        try:
            interface = get_raw_hid_interface()
            if interface is None:
                debug_print("No device found. Retrying in 5 seconds...")
                time.sleep(5)
        except Exception as e:
            debug_print(f"Error during connection attempt: {e}")
            time.sleep(5)
    return interface


def main():
    interface = interface_connect()

    read_thread = threading.Thread(
        target=hid_read_thread, args=(interface,), daemon=True
    )
    read_thread.start()

    request_report = get_report(get_pc_stats())

    try:
        while True:
            try:
                response_report = send_report_with_timeout(interface, request_report)

                if response_report == COULD_NOT_CONNECT:
                    debug_print("Lost connection. Attempting to reconnect...")
                    interface.close()
                    interface = interface_connect()

                    if not read_thread.is_alive():
                        read_thread = threading.Thread(
                            target=hid_read_thread, args=(interface,), daemon=True
                        )
                        read_thread.start()

                    request_report = get_report(get_pc_stats())
                    continue

                request_report = interpret_response(response_report)
                time.sleep(SERVICE_INTERVAL)

            except KeyboardInterrupt:
                debug_print("\nShutting down...")
                break
            except Exception as e:
                debug_print(f"Error in main loop: {e}")
                time.sleep(5)

    finally:
        debug_print("Cleaning up connections...")
        keyboard_manager.cleanup()
        if interface:
            interface.close()
        debug_print("Cleanup complete.")


if __name__ == "__main__":
    main()
