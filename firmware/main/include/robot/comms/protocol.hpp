#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"
#include "robot/types.hpp"

namespace robot::protocol {

constexpr std::uint8_t kVersion = 1;
constexpr std::uint8_t kControlMessageType = 1;
constexpr std::size_t kControlPacketSize = 15;

// Wire layout (little-endian): version:u8, type:u8, sequence:u16,
// forward_velocity_m_s:f32, yaw_rate_rad_s:f32, flags:u8, lease_ms:u16.
constexpr std::uint8_t kFlagArmRequest = 1U << 0U;
constexpr std::uint8_t kFlagDeadmanActive = 1U << 1U;

esp_err_t decodeControlPacket(const std::uint8_t* bytes, std::size_t length,
                              std::int64_t received_at_us,
                              MotionRequest* request);

}  // namespace robot::protocol
