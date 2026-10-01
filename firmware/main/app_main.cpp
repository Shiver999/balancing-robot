#include "robot/robot_app.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace robot {
MotorDriver& safeMotorDriver();
}

extern "C" void app_main() {
    static robot::RobotApp app(robot::safeMotorDriver());
    const esp_err_t result = app.start();
    if (result != ESP_OK) {
        ESP_LOGW("app_main", "Safe scaffold startup returned %s", esp_err_to_name(result));
    }

    // Idle safely. Do not attach motors expecting the scaffold to balance.
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
