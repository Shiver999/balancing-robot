# ESP32-S3 firmware scaffold

ESP-IDF C++ scaffold for an ESP32-S3 target. It defines the modular data/interfaces for sensors, encoders, motor actuation, safety, control, and command decoding. The motor driver is deliberately a safe stub: initialization reports `ESP_ERR_NOT_SUPPORTED`, outputs remain disabled, and no balance loop or BLE transport is active. **Do not expect this firmware to balance or drive the robot.**

## Build

Install ESP-IDF with the ESP32-S3 toolchain, export its environment, then from this directory run the standard ESP-IDF build/flash/monitor workflow. This workspace does not currently have `idf.py` installed, so the firmware has not been compiled here.

## Before hardware is enabled

- Confirm board revisions, GPIO assignments, signal voltage levels, driver PWM mode, fault/enable behavior, current sensing, motor pole pairs, and safe output-disable path.
- Replace `drivers/safe_stubs.cpp` with verified sensor and driver adapters.
- Add watchdog/deadline handling, sensor freshness checks, state estimator, balance/motion controllers, bounded setpoints, and protocol/transport integration.
- Keep the hardware motor-disable independent of firmware and phone connectivity. Test with motors disconnected first.
