# QMK Macropad with Advanced Python Client

This project was originally built for the **ANAVI ARROWS** open-source macropad (by Leon Anavi), but is designed to be highly adaptable. It enhances a QMK-powered macropad with a powerful Python client script that runs on your PC. This allows for advanced features that go beyond standard keyboard firmware capabilities, such as interacting with web APIs, monitoring system stats, and more, all controlled from your macropad.

> **General Compatibility Note:** While this guide uses the ANAVI Arrows as a reference, the firmware and client script will work on **any QMK-compatible macropad** with an OLED screen, provided you follow the configuration steps (like updating Device IDs).

The Python script is designed to be fault-tolerant, automatically handling disconnections and reconnections if the macropad or keyboard is unplugged.

View images and videos of the project at: https://sahil-karanth.github.io/Portfolio/macropad.html

## Features

- **Multiple Macro Layers:** Quickly switch between different modes for various tasks, including layers for general use, programming, Git commands, and Markdown formatting.
- **Internet Speed Testing:** Trigger a network speed test directly from the macropad and view the results on the OLED screen. This runs in the background, so you can continue working.
- **Spotify Media Control:** Displays the currently playing song and artist from Spotify and allows for media control.
- **Pomodoro Timer:** A built-in Pomodoro timer to help you stay focused. The timer runs in the background, and the macropad's RGB lighting will flash when the timer is complete, regardless of the active layer.
- **System Monitoring:** Displays real-time PC stats which are CPU usage, RAM usage, and battery percentage.
- **Dynamic Arrows Layer:** A dedicated arrow key layer that can be toggled on and off from any other layer for quick navigation.
- **RGB Sync:** The macropad's RGB lighting syncs with your main keyboard for a cohesive desktop setup.

## Setup and Configuration

Getting everything running involves two main steps: first, configuring the Python client script with your specific details, and second, compiling and flashing the firmware to your macropad.

### Step 1: Configure the Python Client Script (`macropad_client_hid.py`)

Before flashing the firmware, you need to set up the Python script that runs on your PC.

#### A. Install Dependencies

First, you need to install the required Python libraries. It's recommended to create a `requirements.txt` file with the following content and install it.

```
psutil
speedtest-cli
spotipy
python-dotenv
hidapi
```

Install them using pip: `pip install -r requirements.txt`

#### B. Configure Device IDs

You must update the script with the Vendor ID (VID) and Product ID (PID) for your macropad and (optionally) your main keyboard.

**Find your Device IDs:**

- **On Windows (PowerShell):**
  ```powershell
  Get-PnpDevice -Class 'HIDClass' | Select-Object -Property FriendlyName, InstanceId
  ```
  Look for your keyboard and macropad in the list. The InstanceId will contain the VID and PID, like `VID_FEED&PID_9A25`.
- **On Linux:**
  ```bash
  lsusb
  ```
  This command will list connected USB devices, including their IDs.

**Update the Script:**
Open `macropad_client_hid.py` and modify these lines with your device's IDs:

```python
# Change these values to match your macropad
macropad_vendor_id     = 0xFEED
macropad_product_id    = 0x9A25

# Change these values to match your main keyboard for RGB sync
keyboard_vendor_id = 0x8968
keyboard_product_id = 0x4C37
```

**Note:** The main keyboard IDs are only required for the RGB sync feature. To enable this, you must also flash your QMK-compatible keyboard with the firmware provided in the `keyboard_firmware` folder. If you don't want to use this feature, you can skip this step and ignore the keyboard ID values; the rest of the script will function correctly.

#### C. Spotify API Credentials

For Spotify integration, you need API credentials.

1.  Go to the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard/) and create a new application.
2.  Once created, find your `Client ID` and `Client Secret`.
3.  In your project directory, create a new file named `.env`.
4.  Add your credentials to the `.env` file like this:
    ```
    SPOTIFY_CLIENT_ID=your_client_id_here
    SPOTIFY_CLIENT_SECRET=your_client_secret_here
    ```

#### D. Pomodoro Timer Duration

You can easily change the Pomodoro timer duration.

1.  In the project directory, find the `pomodoro_duration.txt` file (or create it if it doesn't exist).
2.  The file should contain a single number, which is the timer duration **in seconds**. For example, for a 25-minute timer, the content would be `1500`.
3.  You can change this value at any time, even while the client script is running. The new duration will be used the next time you start the timer.

### Step 2: Compile and Flash the QMK Firmware

Once the Python client is configured, you can compile and flash the firmware.

Use **QMK MSYS** to compile the firmware from the `keymap.c` file and its associated project files. This will generate a `.uf2` file which you can then flash to your QMK-compatible macropad.

#### Optional: Configure for a Different OLED Screen

The firmware is pre-configured for the OLED screen that comes with ANAVI macropads. If you are using a different screen, you may need to adjust the display dimensions in the firmware.

1.  Open the `keymap.c` file.
2.  Find and edit the following definitions to match your screen's character capacity:
    ```c
    #define NUM_SCREEN_LINES 8
    #define SCREEN_CHAR_WIDTH 20
    ```

## Usage

### Running the Client Script

For the macropad's advanced features to work, the `macropad_client_hid.py` script must be running on your PC.

```bash
python macropad_client_hid.py
```

### Run on Startup (Windows)

To ensure the script runs automatically and silently every time you start your PC:

1.  Press `Win + R` to open the Run dialog.
2.  Type `shell:startup` and press Enter. This will open the Startup folder.
3.  Right-click inside the folder, select `New` -> `Shortcut`.
4.  In the location field, you will specify the full path to `pythonw.exe` (the windowless version of Python), followed by a space, and then the full path to your `macropad_client_hid.py` script.
    The format is: `"<path_to_pythonw.exe>" "<path_to_your_script.py>"`
    **Example:**
    ```
    "C:\Users\YourUser\AppData\Local\Programs\Python\Python39\pythonw.exe" "C:\Users\YourUser\Documents\QMK-Project\macropad_client_hid.py"
    ```
    _Make sure to replace the paths with the actual locations on your computer._
5.  Click `Next`, give the shortcut a name (e.g., "Macropad Client"), and click `Finish`.

### Macropad Controls

- **Cycle Layers:**
  - **Single Tap Up Arrow:** Cycle forward through the layers.
  - **Double Tap Up Arrow:** Cycle backward through the layers.
- **Toggle Arrows Layer:**
  - Press **Left Arrow** and **Right Arrow** at the same time to switch to the Arrows Layer. Press them again to return to your previous layer.
- **Toggle Display & RGB:**
  - Press **Up Arrow** and **Down Arrow** at the same time to turn the OLED screen and all RGB lighting on or off.

## Advanced Customization

### Pomodoro Timer Notifications

When the Pomodoro timer is running in the background (i.e., you are not on the Pomodoro layer), the firmware checks if the timer is finished periodically. By default, this check occurs every **30 seconds**.
You can change this interval, but be aware of the trade-off:

- A **shorter** interval (e.g., 5 seconds) will give you a more immediate notification when the timer ends.
- However, this increases the frequency of communication between the macropad and the PC, which might introduce a tiny amount of input lag on other layers.

To change this, edit the following line in `keymap.c`:

```c
// in the matrix_scan_user function
if (timer_active && timer_elapsed32(conditional_timer_poll) > 30000) { // 30000 milliseconds = 30 seconds
    conditional_timer_poll = timer_read32();
    enqueue(&req_queue, TIMER_STATUS);
}
```

Change `30000` to your desired interval in milliseconds.
