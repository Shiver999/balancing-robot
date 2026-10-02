#include "robot/drivers/encoder_stub.hpp"
#include "robot/drivers/imu_stub.hpp"

namespace robot {

esp_err_t ImuStub::initialize() { return ESP_ERR_NOT_SUPPORTED; }

esp_err_t ImuStub::read(ImuSample* sample) {
    if (sample == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    *sample = ImuSample{};  // Clear any previously valid sample on failure.
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t EncoderStub::initialize() { return ESP_ERR_NOT_SUPPORTED; }

esp_err_t EncoderStub::read(WheelMeasurement* measurement) {
    if (measurement == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    *measurement = WheelMeasurement{};
    return ESP_ERR_NOT_SUPPORTED;
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
