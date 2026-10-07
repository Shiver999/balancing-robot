#include "robot/drivers/esp_imu_i2c.hpp"
namespace robot {
esp_err_t EspImuI2c::initialize() {
    if (bus_ != nullptr || device_ != nullptr) return ESP_ERR_INVALID_STATE;
    const auto allowed = [](int pin) {
        return pin == 1 || pin == 2 || (pin >= 4 && pin <= 18) || pin == 21 ||
               (pin >= 39 && pin <= 44) || pin == 47;
    };
    if (!allowed(sda_) || !allowed(scl_) || sda_ == scl_ || (address_ != 0x68 && address_ != 0x69)) return ESP_ERR_INVALID_ARG;
    i2c_master_bus_config_t config{};
    config.i2c_port = I2C_NUM_0; config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.sda_io_num = static_cast<gpio_num_t>(sda_);
    config.scl_io_num = static_cast<gpio_num_t>(scl_);
    config.glitch_ignore_cnt = 7; config.flags.enable_internal_pullup = true;
    auto result = i2c_new_master_bus(&config, &bus_);
    if (result != ESP_OK) return result;
    i2c_device_config_t device{};
    device.dev_addr_length = I2C_ADDR_BIT_LEN_7; device.device_address = address_;
    device.scl_speed_hz = 100000;
    result = i2c_master_bus_add_device(bus_, &device, &device_);
    if (result != ESP_OK) { (void)i2c_del_master_bus(bus_); bus_ = nullptr; }
    return result;
}
esp_err_t EspImuI2c::readRegisters(std::uint8_t address, std::uint8_t* bytes, std::size_t length) {
    if (bytes == nullptr || length == 0 || length > 15 || address > 0x7f) return ESP_ERR_INVALID_ARG;
    if (device_ == nullptr) return ESP_ERR_INVALID_STATE;
    // Register pointer followed by repeated START; all sample bytes in one burst.
    return i2c_master_transmit_receive(device_, &address, 1, bytes, length, 20);
}
esp_err_t EspImuI2c::writeRegister(std::uint8_t address, std::uint8_t value) {
    if (address > 0x7f) return ESP_ERR_INVALID_ARG;
    if (device_ == nullptr) return ESP_ERR_INVALID_STATE;
    const std::uint8_t bytes[]{address, value};
    return i2c_master_transmit(device_, bytes, sizeof(bytes), 20);
}
}
