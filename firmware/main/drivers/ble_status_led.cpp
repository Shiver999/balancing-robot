#include "sdkconfig.h"
#include "robot/drivers/ble_status_led.hpp"
#if CONFIG_ROBOT_BLE_STATUS_LED
#include <atomic>
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
namespace robot {
namespace {
std::atomic<bool> connected{false};
rmt_channel_handle_t channel = nullptr;
rmt_encoder_handle_t encoder = nullptr;
constexpr char tag[] = "ble_status_led";
// Only the worker touches RMT. Modest brightness avoids an overly bright status LED.
void worker(void*) {
    bool previous = false, displayed = false;
    while (true) {
        const bool desired = connected.load();
        if (!displayed || desired != previous) {
            const unsigned char grb[]{0, static_cast<unsigned char>(desired ? 0 : 24),
                                        static_cast<unsigned char>(desired ? 24 : 0)};
            rmt_symbol_word_t symbols[25]{};
            for (unsigned bit = 0; bit < 24; ++bit) {
                const bool one = (grb[bit / 8] & (0x80 >> (bit % 8))) != 0;
                // 10 MHz ticks: WS2812 zero=0.3/0.9 us, one=0.9/0.3 us.
                symbols[bit].level0 = 1; symbols[bit].duration0 = one ? 9 : 3;
                symbols[bit].level1 = 0; symbols[bit].duration1 = one ? 3 : 9;
            }
            // End with 100 us low to latch the pixel; payload lives until TX completes.
            symbols[24].duration0 = 500; symbols[24].duration1 = 500;
            rmt_transmit_config_t config{};
            auto result = rmt_transmit(channel, encoder, symbols, sizeof(symbols), &config);
            if (result == ESP_OK) result = rmt_tx_wait_all_done(channel, -1);
            if (result == ESP_OK) { displayed = true; previous = desired; }
            else { ESP_LOGW(tag, "LED update failed: %s", esp_err_to_name(result)); vTaskDelay(pdMS_TO_TICKS(1000)); }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
}
esp_err_t startBleStatusLed() {
    if (channel != nullptr) return ESP_ERR_INVALID_STATE;
    connected.store(false);
    rmt_tx_channel_config_t config{};
    config.clk_src = RMT_CLK_SRC_DEFAULT;
#if CONFIG_ROBOT_STATUS_LED_GPIO38
    config.gpio_num = GPIO_NUM_38;
#else
    config.gpio_num = GPIO_NUM_48;
#endif
    config.resolution_hz = 10000000; config.mem_block_symbols = 64;
    config.trans_queue_depth = 1;
    auto result = rmt_new_tx_channel(&config, &channel);
    if (result != ESP_OK) return result;
    rmt_copy_encoder_config_t copy{};
    result = rmt_new_copy_encoder(&copy, &encoder);
    if (result == ESP_OK) result = rmt_enable(channel);
    if (result == ESP_OK) {
        if (xTaskCreate(worker, "ble_led", 2048, nullptr, 1, nullptr) == pdPASS) {
            ESP_LOGI(tag, "RGB status GPIO%d: red disconnected, blue connected", config.gpio_num);
            return ESP_OK;
        }
        result = ESP_ERR_NO_MEM;
        (void)rmt_disable(channel);
    }
    if (encoder != nullptr) { (void)rmt_del_encoder(encoder); encoder = nullptr; }
    (void)rmt_del_channel(channel); channel = nullptr;
    return result;
}
void setBleStatusConnected(bool value) { connected.store(value); }
}
#else
namespace robot {
esp_err_t startBleStatusLed() { return ESP_OK; }
void setBleStatusConnected(bool) {}
}
#endif
