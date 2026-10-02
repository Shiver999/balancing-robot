#pragma once
#include "esp_err.h"
#include "robot/types.hpp"
namespace robot {
// Optional bench transport; no write characteristic or motor-command admission.
esp_err_t startBleTelemetry();
void publishBleTelemetry(std::int64_t timestamp_us, const ImuSample& imu,
                         const WheelMeasurement& wheels);
}
