# Android sensor viewer

Native Kotlin foreground BLE client for the motor-disabled ESP32 sensor bench.
It scans by service UUID, supports device selection, subscribes to live status,
and displays IMU and wheel measurements with unavailable/stale handling.
The connected screen includes a centered circular analogue joystick preview,
a Disconnect button and scrollable telemetry. Motor commands remain disabled. See [connection and installation instructions](../docs/software/ble-sensor-bench.md).

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

Verify discovery, notification streaming, background disconnect and reconnect on a
real phone and ESP32. Before actuation, implement authenticated access, verified
firmware safety/arming and command admission integration. The command encoder
remains testable but is not sent by this telemetry-only client.
