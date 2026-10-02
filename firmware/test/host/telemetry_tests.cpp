#include "robot/comms/telemetry.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
// Fixed wire fixture shared with SensorTelemetryTest; catches byte order and offset drift.
int main() {
    robot::ImuSample imu; imu.valid = true; imu.device_id = 0x71;
    imu.acceleration_m_s2 = {1, -2, 3}; imu.angular_velocity_rad_s = {-4, 5, -6};
    robot::WheelMeasurement wheels; wheels.valid = wheels.velocity_valid = true;
    wheels.left_angle_rad = 1; wheels.right_angle_rad = 2;
    wheels.left_speed_rad_s = -3; wheels.right_speed_rad_s = 4;
    wheels.left_raw_angle = 123; wheels.right_raw_angle = 456;
    const auto packet = robot::protocol::encodeTelemetry(65535, 12345678, imu, wheels);
    char hex[121]{};
    for (unsigned i = 0; i < packet.size(); ++i) std::snprintf(hex + i * 2, 3, "%02x", packet[i]);
    CHECK(std::strcmp(hex, "0102ffff0f7100004e61bc00000000000000803f000000c000004040000080c00000a0400000c0c00000803f00000040000040c0000080407b00c801") == 0);
    imu.valid = false; wheels.valid = false;
    const auto invalid = robot::protocol::encodeTelemetry(0, 0, imu, wheels);
    CHECK(invalid[4] == 8);
    for (unsigned i = 16; i < invalid.size(); ++i) CHECK(invalid[i] == 0);
    wheels.valid = true; wheels.velocity_valid = false;
    const auto angles = robot::protocol::encodeTelemetry(1, 1, imu, wheels);
    CHECK(angles[4] == 10);
    for (unsigned i = 48; i < 56; ++i) CHECK(angles[i] == 0);
}
