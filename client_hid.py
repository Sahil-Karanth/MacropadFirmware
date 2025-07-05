import sys
import hid
import psutil
import speedtest
import time

vendor_id     = 0xFEED
product_id    = 0x9A25

usage_page    = 0xFF60
usage         = 0x61
report_length = 32

PC_PERFORMANCE = 1
NETWORK_SPEED = 2

def get_raw_hid_interface():
    device_interfaces = hid.enumerate(vendor_id, product_id)
    raw_hid_interfaces = [i for i in device_interfaces if i['usage_page'] == usage_page and i['usage'] == usage]

    if len(raw_hid_interfaces) == 0:
        return None

    interface = hid.device()
    interface.open_path(raw_hid_interfaces[0]['path'])
    
    # Set non-blocking mode or use a timeout
    interface.set_nonblocking(1)  # or use interface.read(report_length, timeout_ms=1000)
    
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

            # get next request to service

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

def test_internet_speed():
    try:
        st = speedtest.Speedtest()
        print("Testing internet speed...")

        # Perform the download speed test
        download_speed = round(st.download() / 1000000, 1)  # Convert to Mbps

        # Perform the upload speed test
        upload_speed = round(st.upload() / 1000000, 1)  # Convert to Mbps

        message = f"{NETWORK_SPEED}{str(download_speed)}|{str(upload_speed)}"

        return message.encode('utf-8')

    except speedtest.SpeedtestException as e:
        print("An error occurred during the speed test:", str(e))

def interpret_response(request_report):
    if request_report[0] == PC_PERFORMANCE:
        return get_report(get_pc_stats())
    elif request_report[1] == NETWORK_SPEED:
        return get_report(test_internet_speed())
    else:
        raise Exception("don't know how to service response")

if __name__ == '__main__':

    # initial report to send
    request_report = get_report(get_pc_stats())

    while True:

        # service request and get next one
        response_report = send_report_with_timeout(request_report)
        request_report = interpret_response(response_report)
        time.sleep(5)