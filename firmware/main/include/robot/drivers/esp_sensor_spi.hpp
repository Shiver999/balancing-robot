#pragma once

#include "driver/spi_master.h"
#include "robot/drivers/sensor_io.hpp"

namespace robot {

struct SensorSpiPins {
    int sck{7}, miso{6}, mosi{5};
    int imu_cs{4}, left_cs{15}, right_cs{16};
};

// One task owns this bus and all sensor drivers. No DMA/heap buffers in reads.
class EspSensorSpi {
public:
    enum class Device : unsigned { kImu, kLeft, kRight };
    explicit EspSensorSpi(SensorSpiPins pins) : pins_(pins) {}
    esp_err_t initialize();
    esp_err_t exchange(Device device, const std::uint8_t* tx,
                       std::uint8_t* rx, std::size_t length);

private:
    SensorSpiPins pins_;
    spi_device_handle_t devices_[3]{};
    bool initialized_{false};
};

class EspImuSpi final : public ImuRegisterIo {
public:
    explicit EspImuSpi(EspSensorSpi& bus) : bus_(bus) {}
    esp_err_t readRegisters(std::uint8_t address, std::uint8_t* bytes,
                            std::size_t length) override;
    esp_err_t writeRegister(std::uint8_t address, std::uint8_t value) override;
private:
    EspSensorSpi& bus_;
};

class EspEncoderSpi final : public EncoderFrameIo {
public:
    EspEncoderSpi(EspSensorSpi& bus, EspSensorSpi::Device device)
        : bus_(bus), device_(device) {}
    esp_err_t transfer(std::uint16_t tx, std::uint16_t* rx) override;
private:
    EspSensorSpi& bus_;
    EspSensorSpi::Device device_;
};

class EspSensorClock final : public SensorClock {
public:
    std::int64_t nowUs() const override;
    void delayMs(std::uint32_t milliseconds) override;
};

void runSensorBench();

}  // namespace robot
