# ESP32-S3 firmware

ESP-IDF C++ scaffold for an ESP32-S3 target. It defines the modular data/interfaces for sensors, encoders, motor actuation, safety, control, and command decoding. The motor driver is deliberately a safe stub: initialization reports `ESP_ERR_NOT_SUPPORTED`, outputs remain disabled, and no balance loop or BLE transport is active. **Do not expect this firmware to balance or drive the robot.**

## Build

The repeatable target build uses **ESP-IDF v5.4** and the ESP32-S3 toolchain.
On this Mac, activate the installed environment in each new Terminal window:

```sh
source "/Users/rogercarrick/.espressif/esp-idf/export.sh"
idf.py --version
```

Then, from the repository root, run:

```sh
cd firmware
idf.py set-target esp32s3
idf.py build
```

To locate the board on macOS, compare `ls /dev/cu.*` before and after plugging
it in. Use the new port in `-p YOUR_PORT`. If it does not appear, try another
computer USB port and a known-good data cable. Detailed activation, port
discovery and flash commands are in the [serial bench guide](../docs/software/sensor-bringup.md)
and [BLE bench guide](../docs/software/ble-sensor-bench.md).

GitHub Actions runs this build without flashing any hardware. See
`.github/workflows/build.yml` for target, host, and Android checks.

## Host tests (no ESP-IDF or robot required)

Requires CMake 3.16+ and a C++17 compiler. From the repository root:

```sh
cmake -S firmware/test/host -B firmware/test/host/build -DCMAKE_BUILD_TYPE=Debug
cmake --build firmware/test/host/build --parallel 2
ctest --test-dir firmware/test/host/build --output-on-failure
```

Tests compile the actual protocol, mailbox, safety, controller, sensor stubs,
motor stub and startup code with host-only ESP-IDF error/log adapters. Address
and undefined-behavior sanitizers are enabled by default; use
`-DROBOT_TEST_SANITIZERS=OFF` only if the host compiler lacks them. They cover
packet compatibility, malformed commands, configured limits, dead-man policy,
lease expiry, replay/wraparound, fault latching, startup disable order, and
unavailable sensors. They do not validate physical pin levels, timing, or motors.

## Real sensor bench

See [sensor bring-up](../docs/software/sensor-bringup.md) for wiring, configuration,
build commands and expected readings. Real MPU-6500/9250 I²C/SPI and AS5048A SPI drivers
are compiled in every build; acquisition is enabled only by
`CONFIG_ROBOT_SENSOR_BENCH`. Default startup remains idle with motors disabled.
Host tests additionally cover SI conversion, device identity/configuration,
data-ready, clipping, parity, encoder diagnostics, angle wraparound, direction,
velocity validity and fault recovery. Physical hardware has not been tested.

## Before hardware is enabled

- Confirm board revisions, GPIO assignments, signal voltage levels, driver PWM mode, fault/enable behavior, current sensing, motor pole pairs, and safe output-disable path.
- Verify the real sensor bench on the physical modules. Replace the motor stub only after validating the hardware disable path. Default sensor factories remain unavailable; explicitly compose the verified real drivers into future control firmware.
- Add watchdog/deadline handling, sensor freshness checks, state estimator, balance/motion controllers, bounded setpoints, and protocol/transport integration.
- Keep the hardware motor-disable independent of firmware and phone connectivity. Test with motors disconnected first.

## BLE sensor bench

The separate `sdkconfig.ble-bench.defaults` adds a NimBLE read/notify status service
to the sensor bench. See [Android connection and live sensors](../docs/software/ble-sensor-bench.md)
for build/flash/install steps and the 60-byte shared contract. Ordinary builds
keep BLE disabled. There is no command-write characteristic or motor enable.

The bench defaults now use IMU I²C at SDA17/SCL4/address0x68 and encoder SPI
at GPIO5/6/7 with CS15/16. See the updated wiring table before flashing; the
identity-only diagnostic retains its separate GPIO5/7 I²C wiring.
