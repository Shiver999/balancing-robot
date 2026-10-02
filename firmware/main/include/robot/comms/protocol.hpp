#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"
#include "robot/types.hpp"

namespace robot::protocol {

constexpr std::uint8_t kVersion = 1;
constexpr std::uint8_t kControlMessageType = 1;
constexpr std::size_t kControlPacketSize = 15;
// Scaffold policy, not a measured BLE timing or hardware safety limit.
constexpr std::uint16_t kMaxLeaseMs = 200;

// Wire layout (little-endian): version:u8, type:u8, sequence:u16,
// forward_velocity_m_s:f32, yaw_rate_rad_s:f32, flags:u8, lease_ms:u16.
constexpr std::uint8_t kFlagArmRequest = 1U << 0U;
constexpr std::uint8_t kFlagDeadmanActive = 1U << 1U;

esp_err_t decodeControlPacket(const std::uint8_t* bytes, std::size_t length,
                              std::int64_t received_at_us,
                              MotionRequest* request);

struct CommandLimits {
    // Fail closed until measured hardware limits are supplied explicitly.
    float max_forward_velocity_m_s{0.0F};
    float max_yaw_rate_rad_s{0.0F};
    std::uint16_t max_lease_ms{kMaxLeaseMs};
};

// Owned by one task. A future BLE adapter must enqueue packets to that task;
// only current(now_us), never a decoded packet, may reach the control loop.
class CommandMailbox {
public:
    explicit CommandMailbox(CommandLimits limits = {}) : limits_(limits) {}
    esp_err_t accept(const std::uint8_t* bytes, std::size_t length,
                     std::int64_t received_at_us);
    MotionRequest current(std::int64_t now_us) const;
    // Only for a new authenticated session while disarmed; expiry does not
    // reset replay protection. Reset clears both the command and sequence.
    void resetSession();

private:
    CommandLimits limits_;
    MotionRequest accepted_{};
    bool has_command_{false};
};

}  // namespace robot::protocol
