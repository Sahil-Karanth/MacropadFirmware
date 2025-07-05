import sys
import hid
import psutil
import time

vendor_id     = 0xFEED
product_id    = 0x9A25

usage_page    = 0xFF60
usage         = 0x61
report_length = 32

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

def send_raw_report(data):
    interface = get_raw_hid_interface()

    if interface is None:
        print("No device found")
        sys.exit(1)

    request_data = [0x00] * (report_length + 1)  # First byte is Report ID
    request_data[1:len(data) + 1] = data
    request_report = bytes(request_data)

    print("Request:")
    print(request_report)

    try:
        # Send the request
        bytes_written = interface.write(request_report)
        print(f"Bytes written: {bytes_written}")
        
        # Give QMK time to process
        time.sleep(0.1)
        
        # Try to read response with timeout/non-blocking
        max_attempts = 10
        for attempt in range(max_attempts):
            try:
                response_report = interface.read(report_length)
                if response_report:  # Check if we got data
                    print("Response:")
                    print(response_report)
                    break
                else:
                    print(f"No data available, attempt {attempt + 1}/{max_attempts}")
                    time.sleep(0.05)  # Short delay between attempts
            except Exception as e:
                print(f"Read error: {e}")
                break
        else:
            print("No response received after all attempts")

    except Exception as e:
        print(f"Communication error: {e}")
    finally:
        interface.close()


def send_raw_report_debug(data):
    interface = get_raw_hid_interface()

    if interface is None:
        print("No device found")
        sys.exit(1)

    request_data = [0x00] * (report_length + 1)
    request_data[1:len(data) + 1] = data
    request_report = bytes(request_data)

    print("Request:")
    print([hex(b) for b in request_report])

    try:
        bytes_written = interface.write(request_report)
        print(f"Bytes written: {bytes_written}")
        
        # Try multiple timeouts
        for timeout in [100, 500, 1000, 2000]:
            print(f"Trying timeout: {timeout}ms")
            response_report = interface.read(report_length, timeout_ms=timeout)
            
            if response_report:
                print("Response received!")
                print([hex(b) for b in response_report])
                return
            else:
                print(f"No response with {timeout}ms timeout")
        
        print("No response received with any timeout")

    except Exception as e:
        print(f"Communication error: {e}")
    finally:
        interface.close()




def get_report(data):
    request_data = [0x00] * (report_length + 1)
    request_data[1:len(data) + 1] = data
    return bytes(request_data)

# Alternative version using timeout instead of non-blocking:
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

            print("Response:")
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
    message = f"{str(ram_percent)}|{str(cpu_percent)}"

    return message.encode('utf-8')

def interpret_response(request_report):
    print(request_report)
    if request_report[0] == 1:
        return get_report(get_pc_stats())
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