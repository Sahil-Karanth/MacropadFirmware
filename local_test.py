import time
import urllib.request
import subprocess
import statistics

def measure_download_speed(url, chunk_size=1024*1024, max_chunks=10):
    print("Testing download speed...")
    total_bytes = 0
    start = time.time()
    try:
        with urllib.request.urlopen(url, timeout=10) as response:
            for _ in range(max_chunks):
                chunk = response.read(chunk_size)
                if not chunk:
                    break
                total_bytes += len(chunk)
    except Exception as e:
        print(f"Download failed: {e}")
        return 0.0

    end = time.time()
    duration = end - start
    if duration == 0:
        return 0.0
    mbps = (total_bytes * 8) / (duration * 1_000_000)  # bits to megabits
    return round(mbps, 2)

def measure_ping(host="8.8.8.8", count=5):
    print(f"Pinging {host}...")
    ping_times = []
    for _ in range(count):
        try:
            output = subprocess.check_output(
                ["ping", "-c", "1", host],
                stderr=subprocess.DEVNULL,
                universal_newlines=True
            )
            time_ms = float(output.split("time=")[1].split(" ")[0])
            ping_times.append(time_ms)
        except Exception:
            ping_times.append(None)
    ping_times = [p for p in ping_times if p is not None]
    if not ping_times:
        return None
    return round(statistics.mean(ping_times), 2)

# --- Run Test ---
test_url = "http://ipv4.download.thinkbroadband.com/10MB.zip"  # Public test file (10MB)
download_speed = measure_download_speed(test_url)
ping_ms = measure_ping()

print("\n--- Speed Test Result ---")
print(f"Download Speed: {download_speed} Mbps")
print(f"Ping: {ping_ms} ms")
