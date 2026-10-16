#include "sdkconfig.h"
#if CONFIG_ROBOT_SINGLE_ENCODER_TEST
#include <cmath>
#include "robot/drivers/as5048a.hpp"
#include "robot/drivers/esp_sensor_spi.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace robot {
namespace {
// Bypass normal validity filtering only for troubleshooting. Never publish these
// unchecked words as measurements. Each read uses command then NOP response.
void logRawDiagnostics(EspEncoderSpi& io) {
    constexpr char tag[] = "encoder_diag";
    const std::uint16_t addresses[]{0x0001, 0x3ffd, 0x3fff};
    const char* names[]{"ERRFL", "DIAAGC", "ANGLE"};
    for (unsigned i = 0; i < 3; ++i) {
        std::uint16_t first = 0, response = 0;
        const auto command = As5048a::readCommand(addresses[i]);
        auto result = io.transfer(command, &first);
        if (result == ESP_OK) result = io.transfer(0, &response);
        if (result != ESP_OK) {
            ESP_LOGW(tag, "%s transport=%s", names[i], esp_err_to_name(result));
            continue;
        }
        const bool parity = As5048a::evenParity(response);
        const bool error = (response & 0x4000) != 0;
        ESP_LOGW(tag, "%s cmd=0x%04x first=0x%04x response=0x%04x parity_ok=%d EF=%d",
                 names[i], static_cast<unsigned>(command), static_cast<unsigned>(first),
                 static_cast<unsigned>(response), parity, error);
        if (!parity || error) continue; // Bit interpretations cannot be trusted.
        if (i == 0) {
            // Reading ERRFL clears stored framing/command/parity errors.
            ESP_LOGW(tag, "ERRFL framing=%d invalid_command=%d command_parity=%d",
                     (response & 1) != 0, (response & 2) != 0, (response & 4) != 0);
        } else if (i == 1) {
            ESP_LOGW(tag, "DIAAGC OCF=%d COF=%d COMP_LOW=%d COMP_HIGH=%d AGC=%u",
                     (response & 0x100) != 0, (response & 0x200) != 0,
                     (response & 0x400) != 0, (response & 0x800) != 0,
                     static_cast<unsigned>(response & 0xff));
        }
        if (response == 0 || response == 0xffff) {
            ESP_LOGW(tag, "Uniform response: check power, CS and MISO continuity; not proof of a working sensor");
        }
    }
}
}
// Dedicated bring-up loop: never initialize the IMU, BLE, or motor peripherals.
void runSingleEncoderTest() {
    constexpr char tag[] = "single_encoder";
    // Unused right CS remains high; only the left device is ever queried.
    static EspSensorSpi bus({7, 6, 5, -1, 15, 16});
    static EspSensorClock clock;
    static EspEncoderSpi io(bus, EspSensorSpi::Device::kLeft);
    static As5048a encoder(io);
    ESP_LOGI(tag, "One AS5048A: SCK7 MISO6 MOSI5 CS15; GPIO16 reserved high; MOTOR OUTPUT DISABLED");
    auto result = bus.initialize();
    if (result != ESP_OK) {
        ESP_LOGE(tag, "SPI startup=%s", esp_err_to_name(result));
        return;
    }
    // Retry startup so a corrected magnet position can recover without reflashing.
    bool initialized = false, have_previous = false;
    std::uint16_t previous = 0;
    std::int64_t previous_us = 0;
    unsigned count = 0;
    auto wake = xTaskGetTickCount();
    const auto period = pdMS_TO_TICKS(10) > 0 ? pdMS_TO_TICKS(10) : 1;
    while (true) {
        if (!initialized) {
            result = encoder.initialize();
            ESP_LOGI(tag, "Encoder init=%s", esp_err_to_name(result));
            if (result != ESP_OK) {
                logRawDiagnostics(io);
                ESP_LOGW(tag, "Check wiring and centered diametric magnet; retrying in 1 second");
                vTaskDelay(pdMS_TO_TICKS(1000));
                wake = xTaskGetTickCount();
                continue;
            }
            initialized = true;
        }
        EncoderReading reading{};
        result = encoder.read(&reading);
        const auto now = clock.nowUs();
        const auto elapsed = now - previous_us;
        bool velocity_valid = false;
        float speed = 0;
        if (result == ESP_OK && now >= 0) {
            // Shortest-angle difference handles crossing 16383 -> 0; hand-turn only.
            int difference = static_cast<int>(reading.angle_ticks) - previous;
            if (difference > 8192) difference -= 16384;
            if (difference < -8192) difference += 16384;
            if (have_previous && elapsed > 0 && elapsed <= 20000 && std::abs(difference) != 8192) {
                speed = static_cast<float>(difference * (6.283185307179586 / 16384.0) * 1000000.0 / elapsed);
                velocity_valid = std::fabs(speed) <= 100.0F;
            }
            previous = reading.angle_ticks; previous_us = now; have_previous = true;
        } else {
            have_previous = false;
            initialized = false; // Retry initialization on a read/magnet fault.
        }
        if (count++ % 10 == 0) {
            if (result == ESP_OK) {
                ESP_LOGI(tag, "raw=%u angle_deg=%.2f diag=0x%04x speed_rad_s=%.3f velocity_valid=%d",
                         static_cast<unsigned>(reading.angle_ticks), reading.angle_ticks * (360.0 / 16384.0),
                         static_cast<unsigned>(reading.diagnostic), velocity_valid ? speed : 0.0F, velocity_valid);
            } else ESP_LOGW(tag, "Encoder reading unavailable: %s", esp_err_to_name(result));
        }
        xTaskDelayUntil(&wake, period);
    }
}
}
#endif
