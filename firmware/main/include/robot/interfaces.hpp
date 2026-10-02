#pragma once

#include "esp_err.h"
#include "robot/types.hpp"

namespace robot {

// Single-owner sensor contract: initialize first, then read into a non-null output.
// Implementations clear failed outputs; raw validity does not imply estimated attitude.
class Imu {
public:
    virtual ~Imu() = default;
    virtual esp_err_t initialize() = 0;
    virtual esp_err_t read(ImuSample* sample) = 0;
};

// Reads a coherent software pair; callers must inspect velocity_valid independently.
class Encoders {
public:
    virtual ~Encoders() = default;
    virtual esp_err_t initialize() = 0;
    virtual esp_err_t read(WheelMeasurement* measurement) = 0;
};

// Hardware boundary for actuation. disable() must be safe before initialization
// in a real adapter; healthy() alone never grants permission to enable outputs.
class MotorDriver {
public:
    virtual ~MotorDriver() = default;
    virtual esp_err_t initialize() = 0;
    virtual void disable() = 0;
    virtual esp_err_t apply(const MotorCommand& command) = 0;
    virtual bool healthy() const = 0;
};

}  // namespace robot
