cmake_minimum_required(VERSION 3.16)
project(robot_host_tests LANGUAGES CXX)

enable_testing()
option(ROBOT_TEST_SANITIZERS "Enable address and undefined-behavior sanitizers" ON)

add_executable(robot_host_tests
    robot_tests.cpp
    ../../main/comms/protocol.cpp
    ../../main/safety/safety_manager.cpp
    ../../main/control/balance_controller.cpp
    ../../main/drivers/safe_stubs.cpp
    ../../main/drivers/sensor_stubs.cpp
    ../../main/robot_app.cpp
)
target_include_directories(robot_host_tests PRIVATE support ../../main/include)
target_compile_features(robot_host_tests PRIVATE cxx_std_17)
target_compile_options(robot_host_tests PRIVATE -Wall -Wextra -Werror -Wpedantic)
if(ROBOT_TEST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(robot_host_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(robot_host_tests PRIVATE -fsanitize=address,undefined)
endif()
foreach(suite protocol mailbox safety sensors)
    add_test(NAME ${suite} COMMAND robot_host_tests ${suite})
endforeach()

add_executable(sensor_driver_tests sensor_driver_tests.cpp
    ../../main/drivers/mpu6xxx.cpp ../../main/drivers/as5048a.cpp)
target_include_directories(sensor_driver_tests PRIVATE support ../../main/include)
target_compile_features(sensor_driver_tests PRIVATE cxx_std_17)
target_compile_options(sensor_driver_tests PRIVATE -Wall -Wextra -Werror -Wpedantic)
if(ROBOT_TEST_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(sensor_driver_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(sensor_driver_tests PRIVATE -fsanitize=address,undefined)
endif()
foreach(suite imu encoder wheels)
    add_test(NAME ${suite} COMMAND sensor_driver_tests ${suite})
endforeach()
