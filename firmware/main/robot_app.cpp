#include "robot/robot_app.hpp"

#include "esp_log.h"

namespace robot {
namespace {
constexpr char kTag[] = "robot_app";
}

RobotApp::RobotApp(MotorDriver& motor_driver) : motor_driver_(motor_driver) {}

esp_err_t RobotApp::start() {
    // Keep the output path disabled before attempting any peripheral startup.
    motor_driver_.disable();
    const esp_err_t driver_result = motor_driver_.initialize();
    if (driver_result != ESP_OK || !motor_driver_.healthy()) {
        motor_driver_.disable();
        safety_.latchFault(FaultCode::kDriverUnavailable);
        ESP_LOGW(kTag, "Motor output is unavailable; remaining disarmed");
        return driver_result == ESP_OK ? ESP_FAIL : driver_result;
    }

    // Control-loop sensor composition and hardware arming checks are absent.
    // The independent opt-in sensor bench runs from app_main without actuation.
    safety_.enterDisarmed();
    ESP_LOGW(kTag, "Scaffold only: no motor outputs or balance loop are enabled");
    return ESP_OK;
}

}  // namespace robot
