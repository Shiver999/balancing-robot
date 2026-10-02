# Android connection and live sensors

The Android app now finds BLE sensor benches, lets the user select a robot,
negotiates ATT MTU, discovers the matching GATT service and subscribes to status
notifications. The foreground screen displays raw IMU acceleration/gyro,
wheel angles, raw counts and wheel speeds. Motors remain disabled; there is
no control-write characteristic in this firmware. Pitch remains unavailable.

## Prepare Terminal and find the ESP32 port (macOS)

1. Open Terminal and enter this checkout's firmware directory:

   ```sh
   cd "/Users/rogercarrick/Library/CloudStorage/OneDrive-Personal/Projects/balancing-robot/firmware"
   ```

2. Activate the installed ESP-IDF environment in that Terminal window:

   ```sh
   source "/Users/rogercarrick/.espressif/esp-idf/export.sh"
   idf.py --version
   ```

   Expect ESP-IDF v5.4. Repeat the activation in each new Terminal window.
   If `idf.py` is not found, run the `source` command again. On another computer,
   substitute that computer's project and ESP-IDF installation paths.

3. With the ESP32 unplugged, list the serial ports:

   ```sh
   ls /dev/cu.*
   ```

4. Connect the board using a USB data cable, preferably through its USB-to-UART
   connector, then run `ls /dev/cu.*` again. The new entry is the board's port.
   It may look like `/dev/cu.usbserial-…`, `/dev/cu.SLAB_USBtoUART`, or
   `/dev/cu.usbmodem…`. Ignore Bluetooth ports. Copy the full actual path;
   the examples are not literal port names.

5. Replace `YOUR_PORT` in the flash/monitor command below with that path, for
   example `-p /dev/cu.usbmodem123456`. Recheck after reconnecting or changing
   USB connectors because the port name may change.

If no new port appears, try another computer USB port, connect directly instead
of through a hub, and try another known-good data cable. Changing the computer
USB port restored detection during this project's bring-up. If the power LED
is off or flickering, unplug the board, disconnect sensors/external power, and
check the bare board on USB before reconnecting peripherals. If power is stable
but the port remains missing, hold **BOOT**, press and release **RESET**, then
release **BOOT** and list the ports again to check download mode. This button
sequence is documented in the [Espressif board guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html).

## Build the ESP32 BLE bench

Keep the existing SPI wiring and motors disconnected. After completing the
Terminal setup above, run:

```sh
idf.py -B build-ble-bench -D SDKCONFIG=sdkconfig.ble-bench \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ble-bench.defaults' build
```

After confirming the wiring, use the same arguments and your actual serial port:

```sh
idf.py -B build-ble-bench -D SDKCONFIG=sdkconfig.ble-bench \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ble-bench.defaults' \
  -p YOUR_PORT flash monitor
```

Exit the serial monitor with **Control + ]**; the board keeps running until power
is disconnected.

The new opt-in defaults enable NimBLE and `CONFIG_ROBOT_BLE_TELEMETRY`.
The ordinary sensor-bench defaults still produce serial-only firmware; the
ordinary default firmware remains idle. The BLE device advertises as
**Balancing Robot**. Advertising starts independently of sensor initialization;
a failed SPI bus still publishes unavailable-sensor heartbeats. Sensor startup
failures require correcting the wiring/magnets and rebooting.

## Build/install the Android app

From the repository root, with the installed JDK and SDK:

```sh
export JAVA_HOME="$(/usr/libexec/java_home -v 17)"
export ANDROID_HOME="$HOME/Library/Android/sdk"
cd android
./gradlew --no-daemon :app:assembleDebug :app:testDebugUnitTest
```

Enable developer options and USB debugging on the Android phone, connect it
with a data cable and approve that Mac on the phone. Verify `adb devices`
shows the intended phone, then install:

```sh
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

If several devices are attached, select the phone with `adb -s SERIAL install`.
A phone with BLE is required for radio validation; unit tests need no phone.

Open **Balancing Robot**, enable Bluetooth and tap **Find robot**. Grant Nearby
devices access on Android 12+, or Location access on Android 8–11. Older Android
versions may also require the phone's Location setting enabled for BLE scanning.
No background location or advertising permission is requested. Tap the discovered
robot (its address distinguishes multiple benches) and wait for live data.

Scanning stops after ten seconds or selection. Each GATT setup stage has a
15-second timeout. Leaving the screen disconnects; returning requires scanning
again. **Disconnect** closes the current session. Failed sessions do not reconnect
automatically to an arbitrary nearby device.

## Validity and connection behavior

The UI reports a connection only after notification subscription succeeds.
Unavailable IMU/encoder/speed fields appear as unavailable instead of fabricated
zero readings. At 1.5 seconds without an accepted snapshot, the app marks data
stale and clears displayed measurements. Replay/out-of-order packets, malformed
values or incompatible protocol packets fail the session and require reconnecting.
New sessions reset sequence and clock history so robot reboots are supported.

Telemetry is public read-only BLE: this bench does not authenticate/bond clients
or encrypt sensor data. No commands are accepted. Authentication and an enforced
firmware arming policy must be designed before adding remote actuation.

## Shared GATT and wire contract

- Service: `7f510001-1b15-4f6c-9a2a-5b2e6b0a0100`
- Status characteristic (read/notify): `7f510003-1b15-4f6c-9a2a-5b2e6b0a0100`
- Standard CCCD: `00002902-0000-1000-8000-00805f9b34fb`
- One complete status snapshot per notification, 60 bytes, little-endian.
- Android requests MTU 96 and requires negotiated MTU at least 63. A generic
  client must also negotiate a sufficient MTU before subscribing; it must not
  interpret a truncated notification as a complete snapshot.

| Byte offset | Field |
| --- | --- |
| 0 | Version 1, u8 |
| 1 | Status type 2, u8 |
| 2 | Sequence, u16, wrapping |
| 4 | Flags: IMU valid=1, wheel angles valid=2, wheel speed valid=4, motors disabled=8 |
| 5 | IMU WHO_AM_I, u8 |
| 6 | Reserved zero, u16 |
| 8 | Robot snapshot publication time, nonnegative i64 microseconds |
| 16 | Acceleration XYZ, three f32, m/s² |
| 28 | Gyro XYZ, three f32, rad/s |
| 40 | Left/right wheel angles, two f32, radians |
| 48 | Left/right wheel speeds, two f32, rad/s |
| 56 | Left/right raw encoder angles, two u16, 0–16383 |

Invalid sensor fields are zero-filled on the wire but ignored by the app according
to flags. The snapshot timestamp is local publication time, not an exact simultaneous
acquisition timestamp. Pitch, calibration, unwrapped odometry and commands are not
part of this status packet. BLE callbacks copy a protected snapshot and never read
the sensor bus. Publications are nominally 10 Hz and may coalesce under host load;
sequence gaps are allowed, duplicates and the ambiguous half-range are rejected.

## Validation limits

Host and JVM tests share a fixed golden packet and cover decoder compatibility,
malformed data, validity, sequence wrap, freshness and session reset. ESP-IDF builds
compile default, serial bench and BLE bench. Android APK/unit tests/lint check the
client code. No physical phone-to-ESP32 radio test or hardware flashing has been
performed during this implementation. Check permissions, discovery, streaming,
phone backgrounding, robot power loss, reconnect and invalid sensors on real hardware.

References: [Android BLE permissions](https://developer.android.com/develop/connectivity/bluetooth/bt-permissions),
[Android GATT connection](https://developer.android.com/develop/connectivity/bluetooth/ble/connect-gatt-server),
[ESP-IDF v5.4 NimBLE peripheral example](https://github.com/espressif/esp-idf/tree/v5.4/examples/bluetooth/nimble/bleprph).

## Connected joystick interface

After notification subscription, the app switches from discovery to a connected
screen with a Disconnect button, a circle centered in the usable screen and a
scrollable telemetry panel below. System bars/cutouts are excluded from the layout.
The circle resizes to fit the available space in portrait or landscape.

Touch inside the circle to preview analogue motion intent. Up/down mean forward/
reverse, and left/right mean turning, not sideways travel. Distance from center
sets intensity: a 5% radial dead zone is neutral, then intensity rises linearly
to 100% at the edge. Diagonals combine both axes while total intensity stays at
most 100%. A drag beyond the circle clamps to the edge. Only the finger that
started the gesture controls it; lifting that finger returns to neutral even
if another finger remains on screen. Release, cancellation, focus loss, resizing,
disconnection and stale telemetry reset the joystick.

This is an interactive preview only. The percentage readout is normalized intent,
not measured speed or a physical velocity limit. No motor commands are transmitted:
the firmware remains telemetry-only. Motor actuation requires a separate verified
firmware command/arming implementation and measured velocity/yaw limits.
