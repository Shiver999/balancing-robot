#pragma once
#include "driver/i2c_master.h"
#include "robot/drivers/sensor_io.hpp"
namespace robot {
// Dedicated IMU bus; SDA17/SCL4 leave encoder SPI5/6/7 free. One bench task owns it.
class EspImuI2c final : public ImuRegisterIo {
public:
    EspImuI2c(int sda, int scl, std::uint16_t address) : sda_(sda), scl_(scl), address_(address) {}
    esp_err_t initialize();
    esp_err_t readRegisters(std::uint8_t address, std::uint8_t* bytes, std::size_t length) override;
    esp_err_t writeRegister(std::uint8_t address, std::uint8_t value) override;
private:
    int sda_, scl_;
    std::uint16_t address_;
    i2c_master_bus_handle_t bus_{nullptr};
    i2c_master_dev_handle_t device_{nullptr};
};
}
