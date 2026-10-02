# Android controller scaffold

Native Kotlin Android app scaffold for the robot's future BLE GATT controller. It currently contains a placeholder UI, a versioned control-packet encoder, and a nonfunctional BLE adapter. It cannot connect to or command the robot yet; control buttons remain disabled.

## Build prerequisites

- Android Studio with Android SDK Platform 35 installed
- JDK 17
- Gradle 8.9, downloaded automatically by the checked-in wrapper with SHA-256 verification

Open this `android` directory in Android Studio, or set `JAVA_HOME` to JDK 17
and `ANDROID_HOME` to your SDK directory and run:

```sh
cd android
./gradlew --no-daemon :app:assembleDebug :app:testDebugUnitTest
```

On this Mac the SDK is installed at `~/Library/Android/sdk`. Set Java 17 for
this shell before building (other Java installations can remain available):

```sh
export JAVA_HOME="$(/usr/libexec/java_home -v 17)"
export ANDROID_HOME="$HOME/Library/Android/sdk"
```

The ignored `local.properties` also records the SDK path for this checkout.
On Windows use `gradlew.bat`. CI installs SDK Platform 35 and Build Tools 34.0.0
and runs the same build/tests. The protocol tests cover the shared golden wire
packet plus invalid numbers, leases, sequences, limits and dead-man/arm flags.
Default motion limits are zero until hardware limits are verified explicitly;
firmware independently enforces its own limits.

## Next implementation steps

1. Implement BLE permissions, scan/connect, pairing/bonding, GATT discovery, and encrypted characteristic access.
2. Match the provisional UUIDs in `RobotProtocol.kt` with the ESP32 GATT service.
3. Implement status parsing, connection/loss events and periodic lease renewal; connect firmware transport to the command mailbox described in the software architecture.
4. Keep drive controls disabled unless connected, authenticated, and the robot reports a safe state. Never make the phone part of the balance loop.

The 200 ms maximum lease and UUIDs are scaffold settings, not measured safety settings. Verify the firmware-side contract and test link loss before enabling UI controls.
