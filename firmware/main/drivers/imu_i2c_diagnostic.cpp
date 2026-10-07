#include "sdkconfig.h"
#if CONFIG_ROBOT_IMU_I2C_DIAGNOSTIC
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace robot {
// Identity-only test: never reset/configure the sensor, start SPI/BLE or drive chip select.
void runImuI2cDiagnostic() {
    constexpr char tag[] = "imu_i2c_diag";
    i2c_master_bus_config_t config{};
    config.i2c_port = I2C_NUM_0;
    config.sda_io_num = GPIO_NUM_5;
    config.scl_io_num = GPIO_NUM_7;
    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.glitch_ignore_cnt = 7;
    config.flags.enable_internal_pullup = true;
    i2c_master_bus_handle_t bus = nullptr;
    const auto result = i2c_new_master_bus(&config, &bus);
    if (result != ESP_OK) {
        ESP_LOGE(tag, "Bus initialization failed: %s", esp_err_to_name(result));
        return;
    }
    ESP_LOGI(tag, "Identity-only I2C test: SDA=5 SCL=7; NCS must be tied to 3V3, AD0 to GND; MOTOR OUTPUT DISABLED");
    // Retry periodically so results remain visible if the monitor opens after startup.
    while (true) {
        ESP_LOGI(tag, "Bus idle levels: SDA=%d SCL=%d (both should be high)",
                 gpio_get_level(GPIO_NUM_5), gpio_get_level(GPIO_NUM_7));
        for (unsigned address = 0x68; address <= 0x69; ++address) {
            const auto probe = i2c_master_probe(bus, address, 100);
            if (probe != ESP_OK) {
                ESP_LOGW(tag, "Address 0x%02x probe=%s", address, esp_err_to_name(probe));
                continue;
            }
            i2c_device_config_t device_config{};
            device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
            device_config.device_address = address;
            device_config.scl_speed_hz = 100000;
            i2c_master_dev_handle_t device = nullptr;
            auto read_result = i2c_master_bus_add_device(bus, &device_config, &device);
            if (read_result != ESP_OK) {
                ESP_LOGE(tag, "Address 0x%02x add device=%s", address, esp_err_to_name(read_result));
                continue;
            }
            // Write only the register pointer, then repeated START to read WHO_AM_I.
            const uint8_t reg = 0x75;
            uint8_t identity = 0;
            read_result = i2c_master_transmit_receive(device, &reg, 1, &identity, 1, 100);
            if (read_result == ESP_OK) {
                ESP_LOGI(tag, "Address 0x%02x ACK; WHO_AM_I=0x%02x (%s)", address, identity,
                         identity == 0x70 ? "MPU-6500" : identity == 0x71 ? "MPU-9250" : "unrecognized identity; do not assume compatibility");
            } else {
                ESP_LOGW(tag, "Address 0x%02x ACK; WHO_AM_I read=%s", address, esp_err_to_name(read_result));
            }
            const auto removed = i2c_master_bus_rm_device(device);
            if (removed != ESP_OK) {
                ESP_LOGE(tag, "Device cleanup failed: %s", esp_err_to_name(removed));
                return;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
} // namespace robot
#endif
