import sys
import hid
import psutil

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

    return interface

def send_raw_report(data):
    interface = get_raw_hid_interface()

    if interface is None:
        print("No device found")
        sys.exit(1)

    request_data = [0x00] * (report_length + 1) # First byte is Report ID
    request_data[1:len(data) + 1] = data
    request_report = bytes(request_data)

    print("Request:")
    print(request_report)

    try:

        interface.write(request_report)

    finally:
        interface.close()

if __name__ == '__main__':
    # Get the current system-wide RAM usage percentage
    ram_percent = psutil.virtual_memory().percent
    cpu_percent = psutil.cpu_percent(interval=1)

    # Convert the float value to a string
    message = f"{str(ram_percent)}|{str(cpu_percent)}"
    # Convert the string to a sequence of bytes and send it
    print(message)
    data_to_send = message.encode('utf-8')
    print(data_to_send)
    send_raw_report(data_to_send)