#include "robot/control/balance_controller.hpp"

namespace robot {

MotorCommand BalanceController::update(const ImuSample& imu,
                                       const WheelMeasurement& wheels,
                                       const MotionRequest& request,
                                       float dt_seconds) {
    (void)imu;
    (void)wheels;
    (void)request;
    (void)dt_seconds;

    // Safe scaffold only: no gains or actuator output are implemented until the
    // motor interface, sensor signs, and current limits are measured and tested.
    return MotorCommand{};
}

// There is no controller history to clear until the feedback implementation exists.
void BalanceController::reset() {}

}  // namespace robot
