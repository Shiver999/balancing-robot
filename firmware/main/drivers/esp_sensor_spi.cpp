#include "robot/drivers/esp_sensor_spi.hpp"

#include <cstring>
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace robot {
namespace {
constexpr auto kHost = SPI2_HOST;
// Avoid flash/PSRAM, strapping, USB and revision-dependent RGB LED pins.
bool sensorPinAllowed(int pin) {
    return pin == 1 || pin == 2 || (pin >= 4 && pin <= 18) || pin == 21 ||
           (pin >= 39 && pin <= 44) || pin == 47;
}
}  // namespace

esp_err_t EspSensorSpi::initialize() {
    if (initialized_) return ESP_ERR_INVALID_STATE;
    const int pins[] = {pins_.sck, pins_.miso, pins_.mosi,
                        pins_.imu_cs, pins_.left_cs, pins_.right_cs};
    for (unsigned i = 0; i < 6; ++i) {
        if (!sensorPinAllowed(pins[i])) return ESP_ERR_INVALID_ARG;
        for (unsigned j = 0; j < i; ++j) {
            if (pins[i] == pins[j]) return ESP_ERR_INVALID_ARG;
        }
    }
    // Deselect every module before enabling the shared clock/data peripheral.
    gpio_config_t chip_selects{};
    chip_selects.pin_bit_mask = (1ULL << pins_.imu_cs) | (1ULL << pins_.left_cs) |
                                (1ULL << pins_.right_cs);
    chip_selects.mode = GPIO_MODE_OUTPUT;
    chip_selects.pull_up_en = GPIO_PULLUP_ENABLE;
    esp_err_t result = gpio_config(&chip_selects);
    if (result != ESP_OK) return result;
    const int cs[] = {pins_.imu_cs, pins_.left_cs, pins_.right_cs};
    for (int pin : cs) {
        result = gpio_set_level(static_cast<gpio_num_t>(pin), 1);
        if (result != ESP_OK) return result;
    }
    spi_bus_config_t bus{};
    bus.sclk_io_num = pins_.sck;
    bus.miso_io_num = pins_.miso;
    bus.mosi_io_num = pins_.mosi;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.data4_io_num = bus.data5_io_num = bus.data6_io_num = bus.data7_io_num = -1;
    bus.max_transfer_sz = 16;
    result = spi_bus_initialize(kHost, &bus, SPI_DMA_DISABLED);
    if (result != ESP_OK) return result;
    // Device modes are switched while acquiring the bus, before manual CS goes low.
    for (unsigned i = 0; i < 3; ++i) {
        spi_device_interface_config_t device{};
        device.clock_speed_hz = 1000000; // Conservative register/SPI bench rate.
        device.mode = i == 0 ? 3 : 1; // MPU vs AS5048A
        device.spics_io_num = -1; // Software CS guarantees AS5048A setup/high time.
        device.queue_size = 1;
        result = spi_bus_add_device(kHost, &device, &devices_[i]);
        if (result != ESP_OK) {
            for (unsigned j = 0; j < i; ++j) {
                (void)spi_bus_remove_device(devices_[j]);
                devices_[j] = nullptr;
            }
            (void)spi_bus_free(kHost);
            return result;
        }
    }
    initialized_ = true;
    return ESP_OK;
}

esp_err_t EspSensorSpi::exchange(Device device, const std::uint8_t* tx,
                                 std::uint8_t* rx, std::size_t length) {
    const auto index = static_cast<unsigned>(device);
    if (!initialized_) return ESP_ERR_INVALID_STATE;
    if (index >= 3 || tx == nullptr || rx == nullptr || length == 0 || length > 16) {
        return ESP_ERR_INVALID_ARG;
    }
    const int cs[] = {pins_.imu_cs, pins_.left_cs, pins_.right_cs};
    // IDF v5.4 requires portMAX_DELAY. This bus has one bench-task owner;
    // control-loop integration must add an independently enforced deadline.
    esp_err_t result = spi_device_acquire_bus(devices_[index], portMAX_DELAY);
    if (result != ESP_OK) return result;
    result = gpio_set_level(static_cast<gpio_num_t>(cs[index]), 0);
    if (result == ESP_OK) {
        esp_rom_delay_us(1); // AS5048A CS setup >=350 ns.
        spi_transaction_t transaction{};
        transaction.length = length * 8;
        transaction.tx_buffer = tx;
        transaction.rx_buffer = rx;
        result = spi_device_polling_transmit(devices_[index], &transaction);
        esp_rom_delay_us(1); // Hold after the last clock.
    }
    // Always attempt deselection and release, including transfer-error paths.
    const esp_err_t deselect = gpio_set_level(static_cast<gpio_num_t>(cs[index]), 1);
    esp_rom_delay_us(1); // AS5048A inter-frame CS high >=350 ns.
    spi_device_release_bus(devices_[index]);
    return result != ESP_OK ? result : deselect;
}

esp_err_t EspImuSpi::readRegisters(std::uint8_t address, std::uint8_t* bytes,
                                  std::size_t length) {
    if (bytes == nullptr || length == 0 || length > 15 || address > 0x7F) {
        return ESP_ERR_INVALID_ARG;
    }
    std::uint8_t tx[16]{}, rx[16]{};
    tx[0] = address | 0x80U;
    const esp_err_t result = bus_.exchange(EspSensorSpi::Device::kImu, tx, rx, length + 1);
    if (result == ESP_OK) std::memcpy(bytes, &rx[1], length);
    return result;
}

esp_err_t EspImuSpi::writeRegister(std::uint8_t address, std::uint8_t value) {
    if (address > 0x7F) return ESP_ERR_INVALID_ARG;
    const std::uint8_t tx[2]{address, value};
    std::uint8_t rx[2]{};
    return bus_.exchange(EspSensorSpi::Device::kImu, tx, rx, 2);
}

esp_err_t EspEncoderSpi::transfer(std::uint16_t tx, std::uint16_t* rx) {
    if (rx == nullptr || device_ == EspSensorSpi::Device::kImu) return ESP_ERR_INVALID_ARG;
    const std::uint8_t outgoing[2]{static_cast<std::uint8_t>(tx >> 8U), static_cast<std::uint8_t>(tx)};
    std::uint8_t incoming[2]{};
    const esp_err_t result = bus_.exchange(device_, outgoing, incoming, 2);
    if (result == ESP_OK) {
        *rx = static_cast<std::uint16_t>((static_cast<std::uint16_t>(incoming[0]) << 8U) | incoming[1]);
    }
    return result;
}

std::int64_t EspSensorClock::nowUs() const { return esp_timer_get_time(); }

// Round startup waits up to a scheduler tick; measurement reads do not call this.
void EspSensorClock::delayMs(std::uint32_t milliseconds) {
    vTaskDelay((milliseconds + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS);
}

}  // namespace robot
