#include "sdkconfig.h"

#if CONFIG_ROBOT_SENSOR_BENCH
#include "robot/drivers/as5048a.hpp"
#include "robot/drivers/esp_sensor_spi.hpp"
#include "robot/drivers/mpu6xxx.hpp"
#include "robot/drivers/esp_imu_i2c.hpp"
#include "esp_log.h"
#include "robot/comms/ble_telemetry.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace robot {

// Bring-up with optional BLE snapshots: fixed lifetime drivers, one bus owner, no control task.
void runSensorBench() {
    constexpr char tag[] = "sensor_bench";
    static EspSensorSpi bus({CONFIG_ROBOT_SPI_SCK, CONFIG_ROBOT_SPI_MISO, CONFIG_ROBOT_SPI_MOSI,
#if CONFIG_ROBOT_IMU_USE_I2C
                            -1,
#else
                            CONFIG_ROBOT_IMU_CS,
#endif
                            CONFIG_ROBOT_LEFT_ENCODER_CS, CONFIG_ROBOT_RIGHT_ENCODER_CS});
#if CONFIG_ROBOT_IMU_USE_I2C
    // Reject cross-bus overlap before either adapter can reconfigure another bus's GPIOs.
    const int spi_pins[]{CONFIG_ROBOT_SPI_SCK, CONFIG_ROBOT_SPI_MISO, CONFIG_ROBOT_SPI_MOSI,
                         CONFIG_ROBOT_LEFT_ENCODER_CS, CONFIG_ROBOT_RIGHT_ENCODER_CS};
    for (int pin : spi_pins) {
        if (pin == CONFIG_ROBOT_IMU_I2C_SDA || pin == CONFIG_ROBOT_IMU_I2C_SCL) {
            ESP_LOGE(tag, "IMU I2C and encoder SPI pins overlap; fix menuconfig");
            return;
        }
    }
#endif
    // Advertising remains available even if sensor initialization fails.
#if CONFIG_ROBOT_BLE_TELEMETRY
    const auto ble_result = startBleTelemetry();
    ESP_LOGI(tag, "BLE startup=%s", esp_err_to_name(ble_result));
#endif
    const esp_err_t bus_result = bus.initialize();
    ESP_LOGI(tag, "Encoder SPI startup=%s", esp_err_to_name(bus_result));
    static EspSensorClock clock;
#if CONFIG_ROBOT_IMU_USE_I2C
    static EspImuI2c imu_io(CONFIG_ROBOT_IMU_I2C_SDA, CONFIG_ROBOT_IMU_I2C_SCL, CONFIG_ROBOT_IMU_I2C_ADDRESS);
    const auto imu_transport_result = imu_io.initialize();
    ESP_LOGI(tag, "IMU I2C SDA=%d SCL=%d address=0x%02x startup=%s",
             CONFIG_ROBOT_IMU_I2C_SDA, CONFIG_ROBOT_IMU_I2C_SCL, CONFIG_ROBOT_IMU_I2C_ADDRESS,
             esp_err_to_name(imu_transport_result));
    static Mpu6xxx imu(imu_io, clock, ImuTransport::kI2c);
#else
    static EspImuSpi imu_io(bus);
    const auto imu_transport_result = bus_result;
    static Mpu6xxx imu(imu_io, clock);
#endif
    static EspEncoderSpi left_io(bus, EspSensorSpi::Device::kLeft);
    static EspEncoderSpi right_io(bus, EspSensorSpi::Device::kRight);
    static As5048a left(left_io), right(right_io);
    static As5048aEncoders encoders(left, right, clock,
        {CONFIG_ROBOT_LEFT_ENCODER_DIRECTION, CONFIG_ROBOT_RIGHT_ENCODER_DIRECTION,
         CONFIG_ROBOT_LEFT_ENCODER_ZERO, CONFIG_ROBOT_RIGHT_ENCODER_ZERO, 100.0F, 20000});
    // Report independent initialization results; failures are not replaced with fake data.
    const auto imu_init = imu_transport_result == ESP_OK ? imu.initialize() : imu_transport_result;
    const auto encoder_init = bus_result == ESP_OK ? encoders.initialize() : bus_result;
    ESP_LOGI(tag, "IMU init=%s WHO_AM_I=0x%02x; encoders init=%s; MOTOR OUTPUT DISABLED",
             esp_err_to_name(imu_init), imu.deviceId(), esp_err_to_name(encoder_init));
    TickType_t period = pdMS_TO_TICKS(10); // Bench polling, not a balancing loop.
    if (period == 0) period = 1;
    auto wake = xTaskGetTickCount();
    unsigned count = 0;
    while (true) {
        ImuSample sample;
        WheelMeasurement wheels;
        const auto imu_result = imu.read(&sample);
        const auto wheel_result = encoders.read(&wheels);
        // Decimate logging to limit serial overhead without slowing normal sensor polling.
        if (count++ % 10 == 0) {
            publishBleTelemetry(clock.nowUs(), sample, wheels);
            if (sample.valid) {
                ESP_LOGI(tag, "imu t=%lld accel_m_s2=(%.3f,%.3f,%.3f) gyro_rad_s=(%.3f,%.3f,%.3f) attitude=unavailable",
                         static_cast<long long>(sample.timestamp_us), sample.acceleration_m_s2.x,
                         sample.acceleration_m_s2.y, sample.acceleration_m_s2.z,
                         sample.angular_velocity_rad_s.x, sample.angular_velocity_rad_s.y, sample.angular_velocity_rad_s.z);
            } else {
                ESP_LOGW(tag, "IMU reading unavailable: %s", esp_err_to_name(imu_result));
            }
            if (wheels.valid) {
                ESP_LOGI(tag, "enc raw=(%u,%u) angle_rad=(%.4f,%.4f) speed_rad_s=(%.3f,%.3f) velocity_valid=%d",
                         static_cast<unsigned>(wheels.left_raw_angle), static_cast<unsigned>(wheels.right_raw_angle),
                         wheels.left_angle_rad, wheels.right_angle_rad, wheels.left_speed_rad_s,
                         wheels.right_speed_rad_s, wheels.velocity_valid);
            } else {
                ESP_LOGW(tag, "Encoder reading unavailable: %s", esp_err_to_name(wheel_result));
            }
        }
        xTaskDelayUntil(&wake, period);
    }
}

}  // namespace robot
#endif
