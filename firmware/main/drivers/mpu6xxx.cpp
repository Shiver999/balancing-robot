#include "robot/drivers/mpu6xxx.hpp"

#include <limits>

namespace robot {
namespace {
constexpr std::uint8_t kWhoAmI = 0x75;
constexpr std::uint8_t kPowerManagement = 0x6B;
constexpr std::uint8_t kInterruptStatus = 0x3A;
constexpr float kAccelerationScale = 9.80665F / 8192.0F; // +/-4 g
constexpr float kGyroScale = 0.017453292519943295F / 65.5F; // +/-500 deg/s -> rad/s

// MPU register words are signed big-endian, unlike the little-endian phone protocol.
std::int16_t signedBe(const std::uint8_t* bytes) {
    const auto bits = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[0]) << 8U) | bytes[1]);
    // Avoid implementation-defined unsigned-to-signed conversion.
    return static_cast<std::int16_t>(bits < 0x8000U ? static_cast<int>(bits)
                                                  : static_cast<int>(bits) - 65536);
}
}  // namespace

esp_err_t Mpu6xxx::initialize() {
    initialized_ = false;
    device_id_ = 0;
    last_timestamp_us_ = -1;
    std::uint8_t id = 0;
    esp_err_t result = io_.readRegisters(kWhoAmI, &id, 1);
    if (result != ESP_OK) return result;
    // Similar module labels are insufficient: only the documented chips are accepted.
    if (id != 0x70 && id != 0x71) return ESP_ERR_NOT_SUPPORTED;
    result = io_.writeRegister(kPowerManagement, 0x80); // Device reset
    if (result != ESP_OK) return result;
    clock_.delayMs(100);
    result = io_.readRegisters(kWhoAmI, &device_id_, 1);
    if (result != ESP_OK) return result;
    if (device_id_ != id) return ESP_ERR_INVALID_RESPONSE;

    // Fixed bench profile: PLL clock, all six axes, no FIFO/DMP, SPI only,
    // +/-500 deg/s, +/-4 g, DLPF=3, 200 Hz output, polled data-ready.
    constexpr std::uint8_t settings[][2] = {
        {0x6B, 0x01}, {0x6C, 0x00}, {0x6A, 0x10}, {0x23, 0x00},
        {0x1A, 0x03}, {0x1B, 0x08}, {0x1C, 0x08}, {0x1D, 0x03},
        {0x19, 0x04}, {0x37, 0x00}, {0x38, 0x01},
    };
    for (const auto& setting : settings) {
        result = io_.writeRegister(setting[0], setting[1]);
        if (result != ESP_OK) return result;
    }
    clock_.delayMs(100); // Gyro startup and filter settling; never in read().
    // Verify writes rather than assuming an electrically responsive chip accepted them.
    for (const auto& setting : settings) {
        std::uint8_t actual = 0;
        result = io_.readRegisters(setting[0], &actual, 1);
        if (result != ESP_OK) return result;
        if (actual != setting[1]) return ESP_ERR_INVALID_RESPONSE;
    }
    // Discard the startup data-ready latch so the first successful read is new.
    std::uint8_t status = 0;
    result = io_.readRegisters(kInterruptStatus, &status, 1);
    if (result != ESP_OK) return result;
    initialized_ = true;
    return ESP_OK;
}

esp_err_t Mpu6xxx::read(ImuSample* sample) {
    if (sample == nullptr) return ESP_ERR_INVALID_ARG;
    *sample = ImuSample{};
    if (!initialized_) return ESP_ERR_INVALID_STATE;
    // Status + accel XYZ + temperature + gyro XYZ in one coherent burst.
    std::uint8_t bytes[15]{};
    const esp_err_t result = io_.readRegisters(kInterruptStatus, bytes, sizeof(bytes));
    if (result != ESP_OK) return result;
    if ((bytes[0] & 0x01U) == 0U) return ESP_ERR_NOT_FINISHED;
    bool all_ones = true;
    for (auto byte : bytes) all_ones = all_ones && byte == 0xFF;
    if (all_ones) return ESP_ERR_INVALID_RESPONSE;
    std::int16_t raw[7]{};
    for (int i = 0; i < 7; ++i) raw[i] = signedBe(&bytes[1 + i * 2]);
    for (int i = 0; i < 7; ++i) {
        if (i == 3) continue; // Temperature is not an inertial measurement.
        if (raw[i] == std::numeric_limits<std::int16_t>::min() ||
            raw[i] == std::numeric_limits<std::int16_t>::max()) {
            return ESP_ERR_INVALID_RESPONSE; // Clipped measurements are not valid.
        }
    }
    // Read completion is a local timestamp, not a compensated acquisition timestamp.
    const auto timestamp = clock_.nowUs();
    if (timestamp < 0 || timestamp <= last_timestamp_us_) return ESP_ERR_INVALID_STATE;
    sample->timestamp_us = timestamp;
    sample->acceleration_m_s2 = {raw[0] * kAccelerationScale, raw[1] * kAccelerationScale,
                                 raw[2] * kAccelerationScale};
    sample->temperature_raw = raw[3];
    sample->angular_velocity_rad_s = {raw[4] * kGyroScale, raw[5] * kGyroScale,
                                      raw[6] * kGyroScale};
    sample->device_id = device_id_;
    sample->valid = true;
    // pitch/pitch_rate remain unavailable until a calibrated estimator exists.
    last_timestamp_us_ = timestamp;
    return ESP_OK;
}

}  // namespace robot
