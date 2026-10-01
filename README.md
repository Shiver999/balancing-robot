# Balancing Robot

Hardware planning, firmware scaffold, and Android remote-control scaffold for an ESP32-S3 two-wheel balancing robot.

## Project areas

- `docs/hardware/` — bill of materials and power/signal architecture
- `docs/software/architecture.md` — firmware, safety, and Android software design
- `firmware/` — ESP-IDF C++ scaffold for ESP32-S3
- `android/` — native Kotlin app scaffold for a future BLE controller

## Safety status

The firmware motor driver is intentionally a nonfunctional safe stub. There is no real sensor driver, balance loop, BLE GATT service, or active motor output. The Android UI also cannot connect or send commands yet. Do not connect motors expecting the current firmware to balance the robot.

Resolve the TBD electrical details in the hardware BOM/architecture before replacing the stubs. Keep the physical motor-disable path independent of firmware and Android connectivity.

See the individual `firmware/README.md`, `android/README.md`, and software architecture document for next steps.
