#pragma once

#include <cstddef>
#include <cstdint>
#include "esp_err.h"

namespace robot {

// Startup delays are allowed here; sensor reads never request a delay.
class SensorClock {
public:
    virtual ~SensorClock() = default;
    virtual std::int64_t nowUs() const = 0;
    virtual void delayMs(std::uint32_t milliseconds) = 0;
};

class ImuRegisterIo {
public:
    virtual ~ImuRegisterIo() = default;
    virtual esp_err_t readRegisters(std::uint8_t address, std::uint8_t* bytes,
                                    std::size_t length) = 0;
    virtual esp_err_t writeRegister(std::uint8_t address, std::uint8_t value) = 0;
};

class EncoderFrameIo {
public:
    virtual ~EncoderFrameIo() = default;
    // One MSB-first 16-bit frame, with a distinct CS assertion for each call.
    virtual esp_err_t transfer(std::uint16_t tx, std::uint16_t* rx) = 0;
};

}  // namespace robot
