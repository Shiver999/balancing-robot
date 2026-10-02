plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

// SDK and identity settings for the scaffold application; no release signing is configured.
android {
    namespace = "org.example.balancingrobot"
    compileSdk = 35

    defaultConfig {
        applicationId = "org.example.balancingrobot"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"
    }

    // Kotlin/JVM and Java compilation agree on Java 17 bytecode.
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
}

// Protocol unit tests run on the host JVM without an emulator or Bluetooth hardware.
dependencies {
    testImplementation("junit:junit:4.13.2")
}
