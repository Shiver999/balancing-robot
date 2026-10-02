idf_component_register(
    SRCS
        "app_main.cpp"
        "robot_app.cpp"
        "safety/safety_manager.cpp"
        "control/balance_controller.cpp"
        "comms/protocol.cpp"
        "drivers/safe_stubs.cpp"
        "drivers/sensor_stubs.cpp"
        "drivers/mpu6xxx.cpp"
        "drivers/as5048a.cpp"
        "drivers/esp_sensor_spi.cpp"
        "drivers/sensor_bench.cpp"
    INCLUDE_DIRS "include"
    REQUIRES freertos log esp_timer esp_driver_spi esp_driver_gpio
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
