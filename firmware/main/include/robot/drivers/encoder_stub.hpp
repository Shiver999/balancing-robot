#pragma once

#include "robot/interfaces.hpp"

namespace robot {

class EncoderStub final : public Encoders {
public:
    esp_err_t initialize() override;
    esp_err_t read(WheelMeasurement* measurement) override;
};

Encoders& defaultEncoders();

}  // namespace robot
