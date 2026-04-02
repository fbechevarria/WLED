# Improv Wi-Fi BLE for WLED

This usermod allows you to configure your WLED device's Wi-Fi credentials over Bluetooth Low Energy (BLE) directly from a smartphone or tablet. 

Operating under the open **Improv Wi-Fi** standard, any supported app (like the WLED App or the [Improv Wi-Fi Web App](https://www.improv-wifi.com/)) can detect an un-provisioned WLED device and securely transmit your home network's SSID and Password without the need to connect to a temporary WLED Access Point (AP).

## Features
- Connects un-provisioned WLED devices directly to your home network via Bluetooth.
- Zero-overhead when connected: The Bluetooth radio is **completely shut off** to save power and memory when the device is successfully connected to Wi-Fi.

## Compatibility
This usermod targets **ESP32** devices (including variants like ESP32-C3, ESP32-S2, ESP32-S3).

> **Note**: Enabling Bluetooth requires a substantial amount of flash memory. A minimum of 4MB of flash is required, but you may need to use an OTA-less partition scheme depending on your build. Wait, ESP32-C3 typically has enough room, but watch the compile size!

## Requirements
- `NimBLE-Arduino` library.

## Installation

1. Copy the `usermod_v2_improv_ble` folder to your WLED `usermods` directory.
2. Edit your `platformio_override.ini` to add the usermod build flags and library dependencies:

```ini
[env:esp32c3dev]
# Use your target environment, e.g., esp32c3dev
build_flags = ${common.build_flags} ${esp32c3.build_flags}
  -D WLED_RELEASE_NAME=\"ESP32-C3\"
  -D USERMOD_IMPROV_BLE
  
lib_deps = ${esp32c3.lib_deps}
  h2zero/NimBLE-Arduino @ ^1.4.1
```

3. Ensure that `usermod_v2_improv_ble` is included in your active usermods (e.g. `wled00/usermods_list.cpp` or via `platformio_override.ini`).

## How to Use
1. Power cycle the WLED device. If the WLED device is NOT connected to your Wi-Fi network, it will begin broadcasting its Bluetooth Improv Wi-Fi signal.
2. Open the WLED app or navigate to `improv-wifi.com` on a BLE-enabled browser (like Chrome on Android or Desktop).
3. Search for the BLE device (it should be named `Improv_WLED`).
4. Select it, type your Wi-Fi name and password, and submit.
5. The WLED device will receive the credentials, establish a Wi-Fi connection, and immediately shut down its Bluetooth beacon to free resources.
