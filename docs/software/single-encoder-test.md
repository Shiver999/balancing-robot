# Single AS5048A encoder test

Serial-only firmware reads one encoder, independently of the IMU and second
encoder. Motor peripherals and BLE are not started. Keep motors disconnected.

## Wiring (power off first)

| Encoder pad | ESP32-S3 |
| --- | --- |
| Small pad 1 (− end), chip pin 13 / GND | GND |
| Small pad 2, chip pin 11 / VDD5V | 3V3 |
| Middle large pad, chip pin 12 / VDD3V | Same 3V3 |
| MISO | GPIO6 |
| MOSI | GPIO5 |
| CLK | GPIO7 |
| CSN | GPIO15 |

The large pad marked **P** connects to chip pin 14 (PWM): leave it disconnected.
Both chip supply pins 11 and 12 must receive 3.3 V in this setup. Supplying
small pad 2 alone is insufficient. Do not confuse the middle large supply pad
with P, and do not apply 5 V while pins 11/12 share the 3V3 rail.
The remaining small-pad order, starting from the − end, is GND, VCC, MISO,
MOSI, CLK, CSN. Verify by continuity to chip pins 13, 11, 3, 4, 2, 1 if unsure.

### Hardware confirmation — 2026-10-16

User testing confirmed successful acquisition after identifying and connecting
both supply pads. Logged raw counts 5595–5605 corresponded to approximately
122.94–123.16 degrees, with `diag=0x0100` and `velocity_valid=1`.
This indicates completed offset compensation and no CORDIC or magnetic warning
flags at that time. The user also confirmed the slow-rotation check. This is
single-encoder bench validation, not validation of a two-wheel or motor loop.
Small stationary speed fluctuations can arise from angle noise differentiated
over the short sampling interval; no speed smoothing is applied.

GPIO16 is configured high as an unused chip select; leave it unconnected.
The IMU can remain on SDA17/SCL4 but is not accessed. Use a centered diametric
magnet and rotate it slowly by hand. Sensor diagnostics must accept the magnet
before readings are published. No sensor OTP programming is performed.

## Build, flash and monitor

```sh
cd "/Users/rogercarrick/Library/CloudStorage/OneDrive-Personal/Projects/balancing-robot/firmware"
source "/Users/rogercarrick/.espressif/esp-idf/export.sh"
ls /dev/cu.*
idf.py -B build-single-encoder -D SDKCONFIG=sdkconfig.single-encoder \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.single-encoder.defaults' \
  -p YOUR_PORT build flash monitor
```

Replace YOUR_PORT with the ESP32 port that appears when connected. Exit the
monitor with Control+]. This separate build/config avoids changing an existing
sensor bench or motor diagnostic configuration. Ordinary sensor/BLE benches
still require two encoders.

Expect `Encoder init=ESP_OK` and lines such as:

```text
raw=4096 angle_deg=90.00 diag=0x0100 speed_rad_s=0.000 velocity_valid=1
```

The diagnostic register can have other valid low bits. Raw angle spans 0–16383;
angle degrees spans 0 up to (but excluding) 360. Increasing/decreasing values
depend on mounting; no direction or zero correction is applied in this test.
Speed uses consecutive samples and handles revolution wraparound. The first
sample, gaps over 20 ms, half-turn ambiguity and speeds above the hand-test
limit of 100 rad/s leave velocity_valid false; a displayed zero in that state
is not a measured zero speed. Nominal polling is 100 Hz and logging 10 Hz;
these are bench timings, not guaranteed motor-control deadlines.

Initialization is retried once a second on failure. Read errors clear velocity
history and trigger reinitialization, allowing recovery after magnet adjustment.
`ESP_ERR_INVALID_RESPONSE` can mean parity/bus errors or rejected magnet health;
check both wiring and magnet placement. Expected startup motor-unavailable
warnings originate from the safe scaffold and do not stop this test.

On initialization failure, `encoder_diag` now prints unchecked SPI command/response
words for ERRFL, DIAAGC and ANGLE, with parity and error flags. These are diagnostic
observations, not accepted measurements. With parity_ok=1 and EF=0, DIAAGC shows
OCF (startup complete), COF (calculation overflow), COMP_LOW/COMP_HIGH (magnetic
field warnings), and AGC. Uniform zero/one responses suggest checking wiring and
power first, but are not definitive by themselves. ERRFL reads clear stored SPI
errors. Share the complete `encoder_diag` lines when troubleshooting.
