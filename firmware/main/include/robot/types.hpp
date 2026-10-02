#pragma once

#include <cstdint>

namespace robot {

// Lifecycle vocabulary for future control integration; current firmware never balances.
enum class RobotState : std::uint8_t {
    kBoot,
    kDisarmed,
    kReady,
    kBalancing,
    kRemoteLost,
    kFault,
};

// Latched failure categories exposed to safety logic and future telemetry.
enum class FaultCode : std::uint8_t {
    kNone,
    kSensorUnavailable,
    kDriverUnavailable,
    kControlDeadlineMissed,
    kInvalidCommand,
    kInternal,
};

// Cartesian components in the sensor frame unless a consumer explicitly transforms them.
struct Vector3 {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
};

// Timestamped IMU snapshot. Check attitude_valid separately before using pitch fields.
struct ImuSample {
    std::int64_t timestamp_us{0};
    float pitch_rad{0.0F};
    float pitch_rate_rad_s{0.0F};
    bool valid{false};
    // Raw sensor-frame SI measurements. valid does not imply calibrated attitude.
    Vector3 acceleration_m_s2{};
    Vector3 angular_velocity_rad_s{};
    std::int16_t temperature_raw{0};
    std::uint8_t device_id{0};
    bool attitude_valid{false};
};

// A valid pair has two healthy angles; velocity_valid additionally requires usable history.
// Unwrapped angles restart after an outage and are not persistent odometry.
struct WheelMeasurement {
    std::int64_t timestamp_us{0};
    float left_angle_rad{0.0F};
    float right_angle_rad{0.0F};
    float left_speed_rad_s{0.0F};
    float right_speed_rad_s{0.0F};
    bool valid{false};
    bool velocity_valid{false};
    bool left_valid{false};
    bool right_valid{false};
    std::uint16_t left_raw_angle{0};
    std::uint16_t right_raw_angle{0};
    float left_unwrapped_angle_rad{0.0F};
    float right_unwrapped_angle_rad{0.0F};
};

// Admitted local command with SI setpoints and a lease measured from receive time.
// Arm/dead-man flags express intent; they do not authorize motor output.
struct MotionRequest {
    std::uint16_t sequence{0};
    float forward_velocity_m_s{0.0F};
    float yaw_rate_rad_s{0.0F};
    std::uint16_t lease_ms{0};
    std::int64_t received_at_us{0};
    bool arm_requested{false};
    bool deadman_active{false};
};

// Actuator request; output units remain unspecified until a real motor adapter exists.
// The default value disables actuation and requests zero output.
struct MotorCommand {
    float left_output{0.0F};
    float right_output{0.0F};
    bool enable{false};
};

// Compact future telemetry snapshot; no publisher currently populates/transmits it.
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
