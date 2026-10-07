# Temporary IMU I²C identity diagnostic

Use this when SPI WHO_AM_I fails despite confirmed wiring. It checks addresses
0x68 and 0x69 and reads register 0x75 without resetting or configuring the sensor.
This separate build never starts SPI, BLE or motor output. Bus transactions have
100 ms timeouts and the check repeats every two seconds.

## Alternate wiring (power off first)

Disconnect USB and battery, leave the encoders disconnected, and change only the
IMU wiring as follows. Disconnect the ESP32 ends of NCS/AD0 wires before tying
those module pins to power/ground. GPIO4 and GPIO6 must no longer connect to them.

| IMU pin | Diagnostic connection |
| --- | --- |
| VCC | Existing verified 3.3 V supply |
| GND | ESP32 GND |
| SCL/SCLK | GPIO7 |
| SDA/SDI | GPIO5 |
| NCS | 3V3 (disconnect GPIO4) |
| AD0/SDO | GND (disconnect GPIO6) |
| FSYNC | GND |
| INT, EDA, ECL | Disconnected |

The driver enables weak internal pull-ups. Check existing breakout pull-ups;
if absent, use an approximately 4.7 kΩ pull-up from SDA to 3V3 and another
from SCL to 3V3. Never pull these signals up to 5 V. Missing/weak pull-ups
can prevent communication and must not be mistaken for a dead sensor.

## Activate tools, build and flash

```sh
cd "/Users/rogercarrick/Library/CloudStorage/OneDrive-Personal/Projects/balancing-robot/firmware"
source "/Users/rogercarrick/.espressif/esp-idf/export.sh"
idf.py --version
ls /dev/cu.*
```

Find the board by comparing the port list before and after plugging it in. Use a
USB data cable, try a different computer USB port if necessary, and replace
`YOUR_PORT` below with the full actual port name. No firmware was flashed by the
agent while preparing this diagnostic.

```sh
idf.py -B build-imu-i2c -D SDKCONFIG=sdkconfig.imu-i2c \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.imu-i2c.defaults' build
idf.py -B build-imu-i2c -D SDKCONFIG=sdkconfig.imu-i2c \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.imu-i2c.defaults' \
  -p YOUR_PORT flash monitor
```

Fresh configuration paths prevent previous SPI/BLE settings carrying over.
Exit the monitor with Control + ]. Normal motor-stub warnings remain expected.

## Results

- `Bus idle levels: SDA=1 SCL=1`: neither line is stuck low at that instant.
- `Address 0x68 ACK; WHO_AM_I=0x70 (MPU-6500)` or `0x71 (MPU-9250)`:
  the expected chip responds over I²C; investigate the SPI-specific path next.
- With AD0 grounded, an absent 0x69 address is expected.
- Another identity: record the actual value; this is not permission to treat it
  as compatible with the existing driver.
- `ESP_ERR_NOT_FOUND`: no address acknowledgment, often absent/unpowered device,
  incorrect wiring or unsuitable pull-ups; does not by itself prove damage.
- `ESP_ERR_TIMEOUT` or an idle line at zero: check stuck lines, shorts, grounding
  and pull-ups before changing firmware.

Send the `imu_i2c_diag` lines from a full cycle for diagnosis.
For the current full bench, unplug power and move IMU SDA→GPIO17 and
SCL→GPIO4, retaining NCS→3V3 and AD0→GND. The diagnostic pins GPIO5/7
are used by the encoders in that build. If explicitly choosing the legacy
SPI IMU mode, restore NCS→GPIO4 and AD0→GPIO6 instead. The identity-only diagnostic does not produce motion or live telemetry.

References: [ESP-IDF v5.4 I²C master API](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/api-reference/peripherals/i2c.html),
[MPU-9250 product specification](https://cdn.sparkfun.com/assets/learn_tutorials/5/5/0/MPU9250REV1.0.pdf).
