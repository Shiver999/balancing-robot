#pragma once

#include <cstdint>

namespace robot {

enum class RobotState : std::uint8_t {
    kBoot,
    kDisarmed,
    kReady,
    kBalancing,
    kRemoteLost,
    kFault,
};

enum class FaultCode : std::uint8_t {
    kNone,
    kSensorUnavailable,
    kDriverUnavailable,
    kControlDeadlineMissed,
    kInvalidCommand,
    kInternal,
};

struct ImuSample {
    std::int64_t timestamp_us{0};
    float pitch_rad{0.0F};
    float pitch_rate_rad_s{0.0F};
    bool valid{false};
};

struct WheelMeasurement {
    std::int64_t timestamp_us{0};
    float left_angle_rad{0.0F};
    float right_angle_rad{0.0F};
    float left_speed_rad_s{0.0F};
    float right_speed_rad_s{0.0F};
    bool valid{false};
};

struct MotionRequest {
    std::uint16_t sequence{0};
    float forward_velocity_m_s{0.0F};
    float yaw_rate_rad_s{0.0F};
    std::uint16_t lease_ms{0};
    std::int64_t received_at_us{0};
    bool arm_requested{false};
    bool deadman_active{false};
};

struct MotorCommand {
    float left_output{0.0F};
    float right_output{0.0F};
    bool enable{false};
};

struct RobotStatus {
    RobotState state{RobotState::kBoot};
    FaultCode fault{FaultCode::kNone};
    float pitch_rad{0.0F};
    float pitch_rate_rad_s{0.0F};
    float left_wheel_speed_rad_s{0.0F};
    float right_wheel_speed_rad_s{0.0F};
    bool imu_valid{false};
    bool encoders_valid{false};
    bool drivers_ready{false};
};

}  // namespace robot
