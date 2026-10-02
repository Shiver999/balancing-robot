#include "robot/drivers/as5048a.hpp"

#include <cmath>

namespace robot {
namespace {
constexpr std::uint16_t kMask = 0x3FFF;
constexpr std::uint16_t kError = 0x4000;
constexpr std::uint16_t kDiagnostics = 0x3FFD;
constexpr std::uint16_t kAngle = 0x3FFF;
constexpr double kRadiansPerTick = 6.2831853071795864769 / 16384.0;

// Apply software zero/sign then normalize negative modulo into one revolution.
std::uint16_t corrected(std::uint16_t ticks, std::uint16_t zero, int direction) {
    const int value = (direction * (static_cast<int>(ticks) - zero)) % 16384;
    return static_cast<std::uint16_t>(value < 0 ? value + 16384 : value);
}

// Choose the shortest signed displacement across the 14-bit angle boundary.
int delta(std::uint16_t current, std::uint16_t previous) {
    int value = static_cast<int>(current) - previous;
    if (value > 8192) value -= 16384;
    if (value < -8192) value += 16384;
    return value;
}
}  // namespace

// Toggle once per set bit; command/response parity covers data, flags and parity bit.
bool As5048a::evenParity(std::uint16_t frame) {
    bool odd = false;
    while (frame != 0) {
        odd = !odd;
        frame = static_cast<std::uint16_t>(frame & (frame - 1U));
    }
    return !odd;
}

std::uint16_t As5048a::readCommand(std::uint16_t address) {
    auto command = static_cast<std::uint16_t>((address & kMask) | 0x4000U);
    if (!evenParity(command)) command |= 0x8000U;
    return command;
}

esp_err_t As5048a::clearErrors() {
    std::uint16_t ignored = 0, response = 0;
    esp_err_t result = io_.transfer(readCommand(0x0001), &ignored);
    if (result != ESP_OK) return result;
    result = io_.transfer(0x0000, &response);
    if (result != ESP_OK) return result;
    // EF may describe the error being cleared. Only parity is required here.
    return evenParity(response) ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

esp_err_t As5048a::readRegister(std::uint16_t address, std::uint16_t* value) {
    std::uint16_t ignored = 0, response = 0;
    esp_err_t result = io_.transfer(readCommand(address), &ignored);
    if (result != ESP_OK) return result;
    result = io_.transfer(0x0000, &response); // Response to the previous frame.
    if (result != ESP_OK) return result;
    if (!evenParity(response) || (response & kError) != 0U) {
        (void)clearErrors(); // Bounded resynchronization; this reading still fails.
        return ESP_ERR_INVALID_RESPONSE;
    }
    *value = response & kMask;
    return ESP_OK;
}

// Clear prior bus errors and require one healthy diagnostic/angle read before use.
esp_err_t As5048a::initialize() {
    initialized_ = false;
    esp_err_t result = clearErrors();
    if (result != ESP_OK) return result;
    initialized_ = true;
    EncoderReading reading;
    result = read(&reading);
    if (result != ESP_OK) initialized_ = false;
    return result;
}

esp_err_t As5048a::read(EncoderReading* reading) {
    if (reading == nullptr) return ESP_ERR_INVALID_ARG;
    *reading = EncoderReading{};
    if (!initialized_) return ESP_ERR_INVALID_STATE;
    std::uint16_t diagnostic = 0, angle = 0;
    esp_err_t result = readRegister(kDiagnostics, &diagnostic);
    if (result != ESP_OK) return result;
    // OCF must be complete; reject CORDIC overflow and either magnetic warning.
    if ((diagnostic & 0x0100U) == 0U || (diagnostic & 0x0E00U) != 0U) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    result = readRegister(kAngle, &angle);
    if (result != ESP_OK) return result;
    *reading = {angle, diagnostic};
    return ESP_OK;
}

// Forget speed and multi-turn continuity whenever timestamps or sensor data fail.
void As5048aEncoders::resetHistory() {
    has_previous_ = false;
    previous_time_us_ = 0;
    unwrapped_left_ticks_ = unwrapped_right_ticks_ = 0;
}

esp_err_t As5048aEncoders::initialize() {
    initialized_ = false;
    resetHistory();
    if ((config_.left_direction != 1 && config_.left_direction != -1) ||
        (config_.right_direction != 1 && config_.right_direction != -1) ||
        config_.left_zero_ticks > kMask || config_.right_zero_ticks > kMask ||
        !std::isfinite(config_.max_speed_rad_s) || config_.max_speed_rad_s <= 0 ||
        config_.max_sample_gap_us <= 0 || config_.max_sample_gap_us > 1000000) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_err_t left_result = left_.initialize();
    const esp_err_t right_result = right_.initialize();
    if (left_result != ESP_OK) return left_result;
    if (right_result != ESP_OK) return right_result;
    initialized_ = true;
    return ESP_OK;
}

esp_err_t As5048aEncoders::read(WheelMeasurement* measurement) {
    if (measurement == nullptr) return ESP_ERR_INVALID_ARG;
    *measurement = WheelMeasurement{};
    if (!initialized_) return ESP_ERR_INVALID_STATE;
    EncoderReading left, right;
    // Both sensors must be healthy; reads are sequential, not simultaneous.
    // Reject excessive completed pair duration; transport waits have no hard deadline.
    const auto started = clock_.nowUs();
    const esp_err_t left_result = left_.read(&left);
    const esp_err_t right_result = right_.read(&right);
    const auto timestamp = clock_.nowUs();
    if (left_result != ESP_OK || right_result != ESP_OK) {
        resetHistory();
        return left_result != ESP_OK ? left_result : right_result;
    }
    if (started < 0 || timestamp < started || timestamp - started > 5000 ||
        (has_previous_ && timestamp <= previous_time_us_)) {
        resetHistory();
        return ESP_ERR_INVALID_STATE;
    }
    const auto current_left = corrected(left.angle_ticks, config_.left_zero_ticks, config_.left_direction);
    const auto current_right = corrected(right.angle_ticks, config_.right_zero_ticks, config_.right_direction);
    const double dt = has_previous_ ? (timestamp - previous_time_us_) / 1000000.0 : 0;
    // A unique shortest-angle interpretation needs less than half a turn between samples.
    // Faster/multiple turns can alias, so these assumptions require physical validation.
    bool velocity_valid = has_previous_ && timestamp - previous_time_us_ <= config_.max_sample_gap_us &&
                          config_.max_speed_rad_s * dt < 3.14159265358979323846;
    float left_speed = 0, right_speed = 0;
    if (velocity_valid) {
        const int left_delta = delta(current_left, previous_left_);
        const int right_delta = delta(current_right, previous_right_);
        left_speed = static_cast<float>(left_delta * kRadiansPerTick / dt);
        right_speed = static_cast<float>(right_delta * kRadiansPerTick / dt);
        if (std::abs(left_delta) == 8192 || std::abs(right_delta) == 8192 ||
            std::fabs(left_speed) > config_.max_speed_rad_s || std::fabs(right_speed) > config_.max_speed_rad_s) {
            resetHistory();
            return ESP_ERR_INVALID_RESPONSE;
        }
        unwrapped_left_ticks_ += left_delta;
        unwrapped_right_ticks_ += right_delta;
    } else {
        // Missing history/gap: establish a new origin, never invent zero speed.
        unwrapped_left_ticks_ = current_left;
        unwrapped_right_ticks_ = current_right;
    }
    previous_left_ = current_left;
    previous_right_ = current_right;
    previous_time_us_ = timestamp;
    has_previous_ = true;
    measurement->timestamp_us = timestamp;
    measurement->left_angle_rad = static_cast<float>(current_left * kRadiansPerTick);
    measurement->right_angle_rad = static_cast<float>(current_right * kRadiansPerTick);
    measurement->left_unwrapped_angle_rad = static_cast<float>(unwrapped_left_ticks_ * kRadiansPerTick);
    measurement->right_unwrapped_angle_rad = static_cast<float>(unwrapped_right_ticks_ * kRadiansPerTick);
    measurement->left_speed_rad_s = left_speed;
    measurement->right_speed_rad_s = right_speed;
    measurement->left_raw_angle = left.angle_ticks;
    measurement->right_raw_angle = right.angle_ticks;
    measurement->left_valid = measurement->right_valid = measurement->valid = true;
    measurement->velocity_valid = velocity_valid;
    return ESP_OK;
}

}  // namespace robot
