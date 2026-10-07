# Sensor bring-up (motor-disabled bench)

The real drivers support the six-axis portion of MPU-6500 (`WHO_AM_I=0x70`)
and MPU-9250 (`0x71`), plus two AS5048A SPI encoders. The IMU now uses a dedicated I²C bus
after hardware testing confirmed WHO_AM_I=0x70 over I²C. Other IMU identities are
rejected. No magnetometer, DMP, bias calibration, body-frame transform, attitude
estimator, BLE streaming or motor actuation is implemented here.

## Current wiring: I²C IMU and SPI encoders

| Signal | ESP32-S3 GPIO |
| --- | --- |
| Encoder SPI SCK | 7 |
| Encoder SPI MISO | 6 |
| Encoder SPI MOSI | 5 |
| IMU SDA/SDI | 17 |
| IMU SCL/SCLK | 4 |
| IMU NCS | 3V3, no GPIO connection |
| IMU AD0/SDO | GND, selects address 0x68 |
| IMU FSYNC | GND |
| Left encoder chip select | 15 |
| Right encoder chip select | 16 |

Use a common ground and verified 3.3 V supply/logic for these modules. Check the
actual breakout labels and schematic before connecting; IMU boards may label
SPI signals with their alternate I²C names. Each encoder needs its own chip select and must release MISO when deselected.
Leave IMU INT/EDA/ECL disconnected. Move the IMU SDA from diagnostic GPIO5
to GPIO17 and SCL from diagnostic GPIO7 to GPIO4 with power off; GPIO5/7
remain reserved for encoder SPI. Keep NCS at 3V3 and AD0/FSYNC grounded.
The temporary identity diagnostic still uses GPIO5/7 and is a different build.
Use suitable I²C pull-ups to 3V3 if the breakout lacks them (typically 4.7 kΩ). Keep motors disconnected and
motor drivers physically disabled throughout bring-up.

The I²C adapter runs at 100 kHz with 20 ms transaction timeouts. SPI2 runs
at 1 MHz, mode 1 for each encoder. Optional legacy IMU SPI mode 3 remains
available by disabling CONFIG_ROBOT_IMU_USE_I2C and restoring SPI wiring. Software chip select provides at least 1 µs setup, hold and inter-frame
high time. Configuration rejects duplicate GPIOs and reserved memory, native
USB, strapping and revision-dependent LED pins. Do not share this adapter
between tasks: its ownership is the single bench task, including each encoder's
command/response pair. IDF v5.4 acquisition/polling waits are indefinite; an
independently enforced deadline is required before control-loop integration.

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

## Build and configure

After activating ESP-IDF and finding the port as described above, build:

```sh
idf.py -B build-i2c-sensors -D SDKCONFIG=sdkconfig.i2c-sensors \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.sensor-bench.defaults' build
```

After confirming the wiring, replace `YOUR_PORT` with the actual port from the
setup steps, then flash and open the monitor:

```sh
idf.py -B build-i2c-sensors -D SDKCONFIG=sdkconfig.i2c-sensors \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.sensor-bench.defaults' \
  -p YOUR_PORT flash monitor
```

Exit the monitor with **Control + ]**. The board keeps running until power is
disconnected.

The opt-in defaults enable `CONFIG_ROBOT_SENSOR_BENCH` and
`CONFIG_ROBOT_IMU_USE_I2C`; ordinary builds leave
it disabled. To change pins, software encoder zeros or direction, use the same
arguments with `menuconfig` and open **Robot sensor bench**. Directions must be
+1 or -1; zero is rejected. These values change software interpretation only,
without programming encoder OTP. Retain the same build/config arguments when
building or, after checking the physical wiring, flashing and monitoring.
If an existing sdkconfig retains old settings, open menuconfig and ensure
**Use dedicated I2C bus for IMU** is enabled with SDA17, SCL4, address104.
Supplemental defaults do not override existing configuration values.
No hardware was flashed by the agent during implementation.

The startup motor stub reports unavailable and remains disabled. The bench then
prints IMU identity and initialization results, polls at nominally 100 Hz and
logs readings about 10 times per second. Failed initialization requires correcting
the cause and rebooting; it never substitutes valid fake readings.

## Reading interpretation and checks

The IMU profile uses ±4 g, ±500 degrees/s, DLPF setting 3 and 200 Hz sensor
output. A single status-plus-data burst checks data-ready and reads all axes.
Output is acceleration in m/s² and angular velocity in rad/s in the sensor's
own axes. At rest, acceleration magnitude should be near 9.81 m/s², and gyro
near zero subject to bias. Rotate each axis by hand to check sign and mounting.
Temperature is retained as raw counts. Clipped inertial axes, absent data-ready,
bad identity/configuration, transport errors and invalid timestamps are rejected.
`valid=true` means a raw sample; `attitude_valid` remains false and pitch fields
are unavailable. Timestamps record completion of the local read, not the exact
sensor acquisition time or compensated filter delay.

AS5048A reads use even parity and the one-frame command/response pipeline.
Error flags trigger a bounded error-register clear while the reading remains
invalid. Diagnostics require offset compensation complete and reject CORDIC
overflow and either magnetic warning. Check magnet alignment/air gap when
initialization or readings fail. Raw angle is 0–16383 counts per revolution.
Turn each wheel slowly through a full revolution, verify wraparound and adjust
software direction so the desired forward rotation has positive velocity.

Both angles must succeed for the wheel pair to be valid. Velocity requires two
fresh readings, a gap at most 20 ms and a shortest-angle difference consistent
with the bench assumption of at most 100 rad/s. The first reading and readings
after a fault or excessive gap have `velocity_valid=false`; the unwrapped origin
is reset. Invalid velocity's numeric zero must not be interpreted as standstill.
The 100 rad/s bound is a hand-turn bench assumption, not a validated motor limit;
undersampling can alias multiple revolutions and cannot be detected from absolute
angle alone. Pair read time above 5 ms is rejected. Verify timing and operating
bounds before any controller uses these measurements.

## Verification

Eight host suites use sanitizers and the real decoding logic, with fake register
and SPI-frame transports to exercise identity, configuration, signed scaling,
freshness, clipping, transport faults, parity, diagnostic faults, wraparound,
direction, gaps, speed limits and recovery. ESP32-S3 default and bench builds
check the actual ESP-IDF adapter. Electrical behavior, sample timing, accuracy,
magnet placement and axis direction require physical bench testing.

Register references: [MPU-6500 register map](https://www.ic-components.se/files/5b/MPU-6500.pdf),
[MPU-9250 register map](https://cdn.sparkfun.com/assets/learn_tutorials/5/5/0/MPU-9250-Register-Map.pdf),
[AS5048A manufacturer datasheet](https://www.mouser.com/datasheet/2/588/AS5048_DS000298_4_00-2324531.pdf),
[ESP-IDF v5.4 SPI API](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/api-reference/peripherals/spi_master.html).

For phone display, use the separate [BLE sensor bench](ble-sensor-bench.md).
The commands above continue to build the serial-only variant.

If SPI identity reads fail, use the separate [I²C identity diagnostic](imu-i2c-diagnostic.md)
with its alternate wiring. Do not use I²C wiring with the SPI bench.
