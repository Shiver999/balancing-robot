#pragma once
#include <array>
#include "robot/types.hpp"

namespace robot::protocol {
// Version 1 status: 60 bytes, little-endian. ATT MTU must be at least 63.
// Header(16), acceleration XYZ(12), gyro XYZ(12), wheel angle pair(8),
// wheel speed pair(8), raw encoder count pair(4). There is no attitude estimate.
constexpr std::size_t kTelemetrySize = 60;
using TelemetryPacket = std::array<std::uint8_t, kTelemetrySize>;
TelemetryPacket encodeTelemetry(std::uint16_t sequence, std::int64_t timestamp_us,
                                const ImuSample& imu, const WheelMeasurement& wheels);
} // namespace robot::protocol
