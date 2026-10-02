# Sensor bring-up (motor-disabled bench)

The real drivers support the six-axis portion of MPU-6500 (`WHO_AM_I=0x70`)
and MPU-9250 (`0x71`), plus two AS5048A SPI encoders. Other IMU identities are
rejected. No magnetometer, DMP, bias calibration, body-frame transform, attitude
estimator, BLE streaming or motor actuation is implemented here.

## Confirmed SPI wiring

| Signal | ESP32-S3 GPIO |
| --- | --- |
| Shared SCK | 7 |
| Shared MISO | 6 |
| Shared MOSI | 5 |
| IMU chip select | 4 |
| Left encoder chip select | 15 |
| Right encoder chip select | 16 |

Use a common ground and verified 3.3 V supply/logic for these modules. Check the
actual breakout labels and schematic before connecting; IMU boards may label
SPI signals with their alternate I²C names. Each module needs a separate chip
select and must release MISO when deselected. Keep motors disconnected and
motor drivers physically disabled throughout bring-up.

The ESP-IDF adapter uses SPI2 at 1 MHz, mode 3 for the IMU and mode 1 for each
encoder. Software chip select provides at least 1 µs setup, hold and inter-frame
high time. Configuration rejects duplicate GPIOs and reserved memory, native
USB, strapping and revision-dependent LED pins. Do not share this adapter
between tasks: its ownership is the single bench task, including each encoder's
command/response pair. IDF v5.4 acquisition/polling waits are indefinite; an
independently enforced deadline is required before control-loop integration.

## Build and configure

With the installed ESP-IDF v5.4 environment, from `firmware`:

```sh
idf.py -B build-sensor-bench -D SDKCONFIG=sdkconfig.sensor-bench \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.sensor-bench.defaults' build
```

The opt-in defaults enable `CONFIG_ROBOT_SENSOR_BENCH`; ordinary builds leave
it disabled. To change pins, software encoder zeros or direction, use the same
arguments with `menuconfig` and open **Robot sensor bench**. Directions must be
+1 or -1; zero is rejected. These values change software interpretation only,
without programming encoder OTP. Retain the same build/config arguments when
building or, after checking the physical wiring, flashing and monitoring.
No hardware was flashed during implementation.

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

Seven host suites use sanitizers and the real decoding logic, with fake register
and SPI-frame transports to exercise identity, configuration, signed scaling,
freshness, clipping, transport faults, parity, diagnostic faults, wraparound,
direction, gaps, speed limits and recovery. ESP32-S3 default and bench builds
check the actual ESP-IDF adapter. Electrical behavior, sample timing, accuracy,
magnet placement and axis direction require physical bench testing.

Register references: [MPU-6500 register map](https://www.ic-components.se/files/5b/MPU-6500.pdf),
[MPU-9250 register map](https://cdn.sparkfun.com/assets/learn_tutorials/5/5/0/MPU-9250-Register-Map.pdf),
[AS5048A manufacturer datasheet](https://www.mouser.com/datasheet/2/588/AS5048_DS000298_4_00-2324531.pdf),
[ESP-IDF v5.4 SPI API](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/api-reference/peripherals/spi_master.html).
