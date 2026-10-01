#pragma once

#include "esp_err.h"
#include "robot/types.hpp"

namespace robot {

class Imu {
public:
    virtual ~Imu() = default;
    virtual esp_err_t initialize() = 0;
    virtual esp_err_t read(ImuSample* sample) = 0;
};

class Encoders {
public:
    virtual ~Encoders() = default;
    virtual esp_err_t initialize() = 0;
    virtual esp_err_t read(WheelMeasurement* measurement) = 0;
};

class MotorDriver {
public:
    virtual ~MotorDriver() = default;
    virtual esp_err_t initialize() = 0;
    virtual void disable() = 0;
    virtual esp_err_t apply(const MotorCommand& command) = 0;
    virtual bool healthy() const = 0;
};

}  // namespace robot
