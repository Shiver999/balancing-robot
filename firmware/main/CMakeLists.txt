idf_component_register(
    SRCS
        "app_main.cpp"
        "robot_app.cpp"
        "safety/safety_manager.cpp"
        "control/balance_controller.cpp"
        "comms/protocol.cpp"
        "drivers/safe_stubs.cpp"
        "drivers/sensor_stubs.cpp"
    INCLUDE_DIRS "include"
    REQUIRES freertos log esp_timer
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
