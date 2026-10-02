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
    if (bytes == nullptr || request == nullptr || length != kControlPacketSize ||
        received_at_us < 0) {
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
    const bool arm = (bytes[12] & kFlagArmRequest) != 0U;
    const bool deadman = (bytes[12] & kFlagDeadmanActive) != 0U;
    if (!std::isfinite(forward) || !std::isfinite(yaw) || lease_ms == 0U ||
        lease_ms > kMaxLeaseMs ||
        ((forward != 0.0F || yaw != 0.0F) && (!arm || !deadman))) {
        return ESP_ERR_INVALID_ARG;
    }

    request->sequence = readU16Le(&bytes[2]);
    request->forward_velocity_m_s = forward;
    request->yaw_rate_rad_s = yaw;
    request->arm_requested = arm;
    request->deadman_active = deadman;
    request->lease_ms = lease_ms;
    request->received_at_us = received_at_us;
    return ESP_OK;
}

esp_err_t CommandMailbox::accept(const std::uint8_t* bytes, std::size_t length,
                                 std::int64_t received_at_us) {
    MotionRequest candidate{};
    const esp_err_t result = decodeControlPacket(bytes, length, received_at_us,
                                                 &candidate);
    if (result != ESP_OK) {
        return result;
    }
    if (!std::isfinite(limits_.max_forward_velocity_m_s) ||
        !std::isfinite(limits_.max_yaw_rate_rad_s) ||
        limits_.max_forward_velocity_m_s < 0.0F ||
        limits_.max_yaw_rate_rad_s < 0.0F || limits_.max_lease_ms == 0U ||
        limits_.max_lease_ms > kMaxLeaseMs ||
        std::fabs(candidate.forward_velocity_m_s) > limits_.max_forward_velocity_m_s ||
        std::fabs(candidate.yaw_rate_rad_s) > limits_.max_yaw_rate_rad_s ||
        candidate.lease_ms > limits_.max_lease_ms) {
        return ESP_ERR_INVALID_ARG;
    }
    if (has_command_) {
        // uint16 serial arithmetic: accept forward distances 1..32767,
        // including wraparound; duplicates, older and ambiguous IDs fail.
        const auto delta = static_cast<std::uint16_t>(candidate.sequence - accepted_.sequence);
        if (delta == 0U || delta >= 0x8000U ||
            received_at_us < accepted_.received_at_us) {
            return ESP_ERR_INVALID_STATE;
        }
    }
    accepted_ = candidate;
    has_command_ = true;
    return ESP_OK;
}

MotionRequest CommandMailbox::current(std::int64_t now_us) const {
    if (!has_command_ || now_us < accepted_.received_at_us ||
        now_us - accepted_.received_at_us >=
            static_cast<std::int64_t>(accepted_.lease_ms) * 1000) {
        return MotionRequest{};
    }
    return accepted_;
}

void CommandMailbox::resetSession() {
    accepted_ = MotionRequest{};
    has_command_ = false;
}

}  // namespace robot::protocol
