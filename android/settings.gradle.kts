// Resolve build plugins centrally from their official distribution repositories.
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

// Keep library resolution consistent; subprojects may not silently add repositories.
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "BalancingRobotAndroid"
// Single application module; there are no independent firmware/transport Gradle modules.
include(":app")
