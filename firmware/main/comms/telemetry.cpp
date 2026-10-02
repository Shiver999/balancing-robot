#include "robot/comms/telemetry.hpp"
#include <cstring>
namespace robot::protocol {
namespace {
// Explicit byte assembly keeps the wire layout independent of structure padding.
void integer(TelemetryPacket& out, unsigned offset, std::uint64_t value, unsigned length) {
    for (unsigned i = 0; i < length; ++i) out[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
void scalar(TelemetryPacket& out, unsigned offset, float value) {
    std::uint32_t bits;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    integer(out, offset, bits, 4);
}
}
TelemetryPacket encodeTelemetry(std::uint16_t sequence, std::int64_t timestamp_us,
                                const ImuSample& imu, const WheelMeasurement& wheels) {
    TelemetryPacket out{};
    out[0] = 1; out[1] = 2;
    integer(out, 2, sequence, 2);
    // Bit 3 is an explicit bench-only promise: motor output is disabled.
    out[4] = 8 | (imu.valid ? 1 : 0) | (wheels.valid ? 2 : 0) |
             (wheels.valid && wheels.velocity_valid ? 4 : 0);
    out[5] = imu.device_id;
    integer(out, 8, timestamp_us >= 0 ? static_cast<std::uint64_t>(timestamp_us) : 0, 8);
    if (imu.valid) {
        scalar(out, 16, imu.acceleration_m_s2.x); scalar(out, 20, imu.acceleration_m_s2.y);
        scalar(out, 24, imu.acceleration_m_s2.z); scalar(out, 28, imu.angular_velocity_rad_s.x);
        scalar(out, 32, imu.angular_velocity_rad_s.y); scalar(out, 36, imu.angular_velocity_rad_s.z);
    }
    if (wheels.valid) {
        scalar(out, 40, wheels.left_angle_rad); scalar(out, 44, wheels.right_angle_rad);
        if (wheels.velocity_valid) {
            scalar(out, 48, wheels.left_speed_rad_s); scalar(out, 52, wheels.right_speed_rad_s);
        }
        integer(out, 56, wheels.left_raw_angle, 2); integer(out, 58, wheels.right_raw_angle, 2);
    }
    return out;
}
} // namespace robot::protocol
