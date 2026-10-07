# Compile real sensor adapters in every build; Kconfig gates bench execution, not source coverage.
idf_component_register(
    SRCS
        "app_main.cpp"
        "robot_app.cpp"
        "safety/safety_manager.cpp"
        "control/balance_controller.cpp"
        "comms/protocol.cpp"
        "comms/telemetry.cpp"
        "comms/ble_telemetry.cpp"
        "drivers/safe_stubs.cpp"
        "drivers/sensor_stubs.cpp"
        "drivers/mpu6xxx.cpp"
        "drivers/as5048a.cpp"
        "drivers/esp_sensor_spi.cpp"
        "drivers/sensor_bench.cpp"
        "drivers/imu_i2c_diagnostic.cpp"
        "drivers/esp_imu_i2c.cpp"
        "drivers/ble_status_led.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_rmt freertos log esp_timer esp_driver_spi esp_driver_gpio esp_driver_i2c bt nvs_flash
)

# Shared headers and driver code use the C++17 language contract.
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
