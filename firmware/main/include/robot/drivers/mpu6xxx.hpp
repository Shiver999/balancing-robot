#pragma once

#include "robot/drivers/sensor_io.hpp"
#include "robot/interfaces.hpp"

namespace robot {

// Transport-aware USER_CTRL preserves I2C access when that adapter is selected.
// Six-axis portion of MPU-6500 (WHO_AM_I=0x70) / MPU-9250 (0x71).
// No DMP, magnetometer, bias calibration or body-frame attitude estimator.
enum class ImuTransport { kSpi, kI2c };

class Mpu6xxx final : public Imu {
public:
    Mpu6xxx(ImuRegisterIo& io, SensorClock& clock, ImuTransport transport = ImuTransport::kSpi)
        : io_(io), clock_(clock), transport_(transport) {}
    // Identify/reset the chip, configure the fixed profile, verify it and discard startup status.
    esp_err_t initialize() override;
    // Clear the output first; only publish a new, unclipped, data-ready raw sample.
    esp_err_t read(ImuSample* sample) override;
    std::uint8_t deviceId() const { return device_id_; }

private:
    ImuRegisterIo& io_;
    SensorClock& clock_;
    ImuTransport transport_;
    std::uint8_t device_id_{0};
    bool initialized_{false};
    std::int64_t last_timestamp_us_{-1};
};

}  // namespace robot
