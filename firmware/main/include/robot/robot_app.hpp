#pragma once

#include "esp_err.h"
#include "robot/interfaces.hpp"
#include "robot/safety/safety_manager.hpp"

namespace robot {

class RobotApp {
public:
    explicit RobotApp(MotorDriver& motor_driver);
    esp_err_t start();

private:
    MotorDriver& motor_driver_;
    SafetyManager safety_{};
};

}  // namespace robot
