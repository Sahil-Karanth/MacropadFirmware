import sys
import hid
import psutil
import speedtest
import time
import threading
from threading import Lock

vendor_id     = 0xFEED
product_id    = 0x9A25

usage_page    = 0xFF60
usage         = 0x61
report_length = 32

PC_PERFORMANCE = 1
NETWORK_SPEED = 2

class NetworkSpeedTester:
    def __init__(self):
        self.lock = Lock()
        self.is_testing = False
        self.last_result = None
        self.test_start_time = None
        
    def start_test(self):
        """Start network speed test in background thread"""
        with self.lock:
            if self.is_testing:
                return  # Test already in progress
            
            self.is_testing = True
            self.test_start_time = time.time()
            
        # Start test in background thread
        thread = threading.Thread(target=self._run_test)
        thread.daemon = True
        thread.start()
    
    def _run_test(self):
        """Run the actual speed test"""
        try:
            print("Starting network speed test...")
            st = speedtest.Speedtest()
            
            # Perform the download speed test
            download_speed = round(st.download() / 1000000, 1)  # Convert to Mbps
            
            # Perform the upload speed test
            upload_speed = round(st.upload() / 1000000, 1)  # Convert to Mbps
            
            with self.lock:
                self.last_result = (download_speed, upload_speed)
                self.is_testing = False
                
            print(f"Speed test completed: {download_speed}↓ {upload_speed}↑ Mbps")
            
        except Exception as e:
            print(f"Speed test failed: {e}")
            with self.lock:
                self.last_result = None
                self.is_testing = False
    
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

# Global speed tester instance
speed_tester = NetworkSpeedTester()

def get_raw_hid_interface():
    device_interfaces = hid.enumerate(vendor_id, product_id)
    raw_hid_interfaces = [i for i in device_interfaces if i['usage_page'] == usage_page and i['usage'] == usage]

    if len(raw_hid_interfaces) == 0:
        return None

    interface = hid.device()
    interface.open_path(raw_hid_interfaces[0]['path'])
    
    interface.set_nonblocking(1)
    
    return interface

def get_report(data):
    request_data = [0x00] * (report_length + 1)
    request_data[1:len(data) + 1] = data
    return bytes(request_data)

def send_report_with_timeout(request_report):
    interface = get_raw_hid_interface()

    if interface is None:
        print("No device found")
        sys.exit(1)

    print("Request:")
    print(request_report)

    try:
        interface.write(request_report)
        
        # Use read with timeout (timeout in milliseconds)
        response_report = interface.read(report_length, timeout_ms=2000)
        
        if response_report:
            print("Next service:")
            print(response_report)
        else:
            print("No response received within timeout")

    except Exception as e:
        print(f"Communication error: {e}")
    finally:
        interface.close()
        return response_report

def get_pc_stats():
    ram_percent = psutil.virtual_memory().percent
    cpu_percent = psutil.cpu_percent(interval=1)
    message = f"{PC_PERFORMANCE}{str(ram_percent)}|{str(cpu_percent)}"

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
        # Check if we need to start a new testefd
        status, _, _ = speed_tester.get_status()
        if status == "idle":
            speed_tester.start_test()
        
        return get_report(get_network_status())
    else:
        # Unknown request, default to PC stats
        return get_report(get_pc_stats())

if __name__ == '__main__':
    # initial report to send
    request_report = get_report(get_pc_stats())

    while True:
        try:
            # service request and get next one
            response_report = send_report_with_timeout(request_report)
            request_report = interpret_response(response_report)
            time.sleep(1)
        except KeyboardInterrupt:
            print("\nShutting down...")
            break
        except Exception as e:
            print(f"Error in main loop: {e}")
            time.sleep(1)