# Balancing Robot

Hardware planning, firmware scaffold, and Android remote-control scaffold for an ESP32-S3 two-wheel balancing robot.

## Project areas

- `docs/hardware/` — bill of materials and power/signal architecture
- `docs/software/architecture.md` — firmware, safety, and Android software design
- `firmware/` — ESP-IDF C++ scaffold for ESP32-S3
- `android/` — native Kotlin app scaffold for a future BLE controller

## Safety status

The firmware motor driver is intentionally a nonfunctional safe stub. Real SPI drivers for MPU-6500/9250 and both AS5048A encoders are available in an opt-in, motor-disabled [sensor bench](docs/software/sensor-bringup.md). There is no balance loop, BLE GATT service, or active motor output. The Android UI also cannot connect or send commands yet. Do not connect motors expecting the current firmware to balance the robot.

Resolve the TBD electrical details in the hardware BOM/architecture before replacing the stubs. Keep the physical motor-disable path independent of firmware and Android connectivity.

Sensor adapters explicitly report unavailable hardware and invalidate samples.
Command admission defaults to zero motion, bounds leases to 200 ms, rejects
replayed sequences, and clears expired requests. It is not connected to BLE or
a control loop yet. The corrected provisional pin plan is in the hardware
architecture; it supersedes the earlier development-branch allocation.

## Builds and tests

- Firmware target: ESP-IDF v5.4, ESP32-S3; see [firmware build instructions](firmware/README.md).
- Firmware host tests: CMake/C++17 with sanitizers, no board required.
- Android: JDK 17, SDK Platform 35, checked-in Gradle 8.9 wrapper; see [Android instructions](android/README.md).
- GitHub Actions runs host tests, the target firmware build, and the Android APK build/protocol tests on pushes and pull requests.

See the individual `firmware/README.md`, `android/README.md`, and software architecture document for next steps.
