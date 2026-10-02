#pragma once

#include "esp_err.h"
#include "robot/interfaces.hpp"
#include "robot/safety/safety_manager.hpp"

namespace robot {

// Startup coordinator owns safety state but borrows the motor adapter for its lifetime.
// It establishes the disabled output path; sensor bench composition lives in app_main.
class RobotApp {
public:
    explicit RobotApp(MotorDriver& motor_driver);
    esp_err_t start();

private:
    MotorDriver& motor_driver_;
    SafetyManager safety_{};
};

}  // namespace robot
