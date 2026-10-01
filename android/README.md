# Android controller scaffold

Native Kotlin Android app scaffold for the robot's future BLE GATT controller. It currently contains a placeholder UI, a versioned control-packet encoder, and a nonfunctional BLE adapter. It cannot connect to or command the robot yet; control buttons remain disabled.

## Build prerequisites

- Android Studio with Android SDK Platform 35 installed
- JDK 17
- Gradle 8.9 or a compatible Gradle installation

Open this `android` directory in Android Studio and sync/build the `app` module. No Gradle wrapper is checked in yet.

## Next implementation steps

1. Implement BLE permissions, scan/connect, pairing/bonding, GATT discovery, and encrypted characteristic access.
2. Match the provisional UUIDs in `RobotProtocol.kt` with the ESP32 GATT service.
3. Implement status parsing, connection/loss events, command sequence/lease handling, and protocol tests.
4. Keep drive controls disabled unless connected, authenticated, and the robot reports a safe state. Never make the phone part of the balance loop.

The 200 ms lease and UUIDs are placeholders, not final safety settings. Verify the firmware-side contract and test link loss before enabling UI controls.
