#include "robot/drivers/encoder_stub.hpp"
#include "robot/drivers/imu_stub.hpp"

#include "esp_timer.h"

namespace robot {
namespace {

ImuSample makeImuSample() {
    ImuSample sample{};
    sample.timestamp_us = esp_timer_get_time();
    sample.pitch_rad = 0.0F;
    sample.pitch_rate_rad_s = 0.0F;
    sample.valid = true;
    return sample;
}

WheelMeasurement makeWheelMeasurement() {
    WheelMeasurement measurement{};
    measurement.timestamp_us = esp_timer_get_time();
    measurement.left_angle_rad = 0.0F;
    measurement.right_angle_rad = 0.0F;
    measurement.left_speed_rad_s = 0.0F;
    measurement.right_speed_rad_s = 0.0F;
    measurement.valid = true;
    return measurement;
}

}  // namespace

esp_err_t ImuStub::initialize() {
    initialized_ = true;
    return ESP_OK;
}

esp_err_t ImuStub::read(ImuSample* sample) {
    if (sample == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    *sample = makeImuSample();
    return ESP_OK;
}

esp_err_t EncoderStub::initialize() {
    initialized_ = true;
    return ESP_OK;
}

esp_err_t EncoderStub::read(WheelMeasurement* measurement) {
    if (measurement == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    *measurement = makeWheelMeasurement();
    return ESP_OK;
}

Imu& defaultImu() {
    static ImuStub imu;
    return imu;
}

Encoders& defaultEncoders() {
    static EncoderStub encoders;
    return encoders;
}

}  // namespace robot
