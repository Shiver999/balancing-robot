#pragma once

#include "robot/types.hpp"

namespace robot {

class BalanceController {
public:
    MotorCommand update(const ImuSample& imu, const WheelMeasurement& wheels,
                        const MotionRequest& request, float dt_seconds);
    void reset();
};

}  // namespace robot
