#include "robot/robot_app.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "robot/drivers/esp_sensor_spi.hpp"

namespace robot {
MotorDriver& safeMotorDriver();
}

// ESP-IDF entry point has C linkage; static ownership outlives the bench/idle loops.
extern "C" void app_main() {
    static robot::RobotApp app(robot::safeMotorDriver());
    const esp_err_t result = app.start();
    if (result != ESP_OK) {
        ESP_LOGW("app_main", "Safe scaffold startup returned %s", esp_err_to_name(result));
    }

// The bench is independent of the unavailable motor adapter; its error is expected.
#if CONFIG_ROBOT_SENSOR_BENCH
    robot::runSensorBench();
#endif

    // Idle safely. Do not attach motors expecting the scaffold to balance.
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
