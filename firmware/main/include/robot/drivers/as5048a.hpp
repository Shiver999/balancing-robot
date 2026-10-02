#pragma once

#include "robot/drivers/sensor_io.hpp"
#include "robot/interfaces.hpp"

namespace robot {

struct EncoderReading {
    std::uint16_t angle_ticks{0};
    std::uint16_t diagnostic{0};
};

class As5048a {
public:
    explicit As5048a(EncoderFrameIo& io) : io_(io) {}
    esp_err_t initialize();
    esp_err_t read(EncoderReading* reading);
    static std::uint16_t readCommand(std::uint16_t address);
    static bool evenParity(std::uint16_t frame);

private:
    esp_err_t clearErrors();
    esp_err_t readRegister(std::uint16_t address, std::uint16_t* value);
    EncoderFrameIo& io_;
    bool initialized_{false};
};

struct WheelEncoderConfig {
    int left_direction{1};
    int right_direction{1};
    std::uint16_t left_zero_ticks{0};
    std::uint16_t right_zero_ticks{0};
    // Hand-turn bench assumptions, not verified motor operating limits.
    float max_speed_rad_s{100.0F};
    std::int64_t max_sample_gap_us{20000};
};

class As5048aEncoders final : public Encoders {
public:
    As5048aEncoders(As5048a& left, As5048a& right, SensorClock& clock,
                    WheelEncoderConfig config = {})
        : left_(left), right_(right), clock_(clock), config_(config) {}
    esp_err_t initialize() override;
    esp_err_t read(WheelMeasurement* measurement) override;

private:
    void resetHistory();
    As5048a& left_;
    As5048a& right_;
    SensorClock& clock_;
    WheelEncoderConfig config_;
    bool initialized_{false};
    bool has_previous_{false};
    std::uint16_t previous_left_{0}, previous_right_{0};
    std::int64_t previous_time_us_{0};
    double unwrapped_left_ticks_{0}, unwrapped_right_ticks_{0};
};

}  // namespace robot
