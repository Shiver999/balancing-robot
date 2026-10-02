#pragma once

#include "robot/types.hpp"

namespace robot {

// Placeholder controller API. update() always returns a disabled zero command;
// no gains, calibrated feedback or controller state have been implemented.
class BalanceController {
public:
    MotorCommand update(const ImuSample& imu, const WheelMeasurement& wheels,
                        const MotionRequest& request, float dt_seconds);
    void reset();
};

}  // namespace robot
