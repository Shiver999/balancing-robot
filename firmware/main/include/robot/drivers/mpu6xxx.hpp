#pragma once

#include "robot/drivers/sensor_io.hpp"
#include "robot/interfaces.hpp"

namespace robot {

// Six-axis portion of MPU-6500 (WHO_AM_I=0x70) / MPU-9250 (0x71).
// No DMP, magnetometer, bias calibration or body-frame attitude estimator.
class Mpu6xxx final : public Imu {
public:
    Mpu6xxx(ImuRegisterIo& io, SensorClock& clock) : io_(io), clock_(clock) {}
    // Identify/reset the chip, configure the fixed profile, verify it and discard startup status.
    esp_err_t initialize() override;
    // Clear the output first; only publish a new, unclipped, data-ready raw sample.
    esp_err_t read(ImuSample* sample) override;
    std::uint8_t deviceId() const { return device_id_; }

private:
    ImuRegisterIo& io_;
    SensorClock& clock_;
    std::uint8_t device_id_{0};
    bool initialized_{false};
    std::int64_t last_timestamp_us_{-1};
};

}  // namespace robot
