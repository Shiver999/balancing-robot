#include "robot/comms/protocol.hpp"

#include <cmath>
#include <cstring>

namespace robot::protocol {
namespace {

std::uint16_t readU16Le(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
           (static_cast<std::uint16_t>(bytes[1]) << 8U);
}

float readF32Le(const std::uint8_t* bytes) {
    const std::uint32_t bits = static_cast<std::uint32_t>(bytes[0]) |
                               (static_cast<std::uint32_t>(bytes[1]) << 8U) |
                               (static_cast<std::uint32_t>(bytes[2]) << 16U) |
                               (static_cast<std::uint32_t>(bytes[3]) << 24U);
    float value = 0.0F;
    static_assert(sizeof(value) == sizeof(bits), "Protocol requires 32-bit float");
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

}  // namespace

esp_err_t decodeControlPacket(const std::uint8_t* bytes, std::size_t length,
                              std::int64_t received_at_us,
                              MotionRequest* request) {
    if (bytes == nullptr || request == nullptr || length != kControlPacketSize) {
        return ESP_ERR_INVALID_ARG;
    }
    if (bytes[0] != kVersion || bytes[1] != kControlMessageType) {
        return ESP_ERR_INVALID_VERSION;
    }
    if ((bytes[12] & static_cast<std::uint8_t>(~(kFlagArmRequest | kFlagDeadmanActive))) != 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    const float forward = readF32Le(&bytes[4]);
    const float yaw = readF32Le(&bytes[8]);
    const std::uint16_t lease_ms = readU16Le(&bytes[13]);
    if (!std::isfinite(forward) || !std::isfinite(yaw) || lease_ms == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    request->sequence = readU16Le(&bytes[2]);
    request->forward_velocity_m_s = forward;
    request->yaw_rate_rad_s = yaw;
    request->arm_requested = (bytes[12] & kFlagArmRequest) != 0U;
    request->deadman_active = (bytes[12] & kFlagDeadmanActive) != 0U;
    request->lease_ms = lease_ms;
    request->received_at_us = received_at_us;
    return ESP_OK;
}

}  // namespace robot::protocol
