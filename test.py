import psutil
import time

def get_download_mbps(interval=1):
    net1 = psutil.net_io_counters()
    time.sleep(interval)
    net2 = psutil.net_io_counters()

    # Bytes received in interval
    bytes_recv = net2.bytes_recv - net1.bytes_recv

    # Convert to Megabits per second
    mbps = (bytes_recv * 8) / (interval * 1_000_000)
    return round(mbps, 2)

while True:
    dl_speed = get_download_mbps()
    print(f"Download: {dl_speed} Mbps")
