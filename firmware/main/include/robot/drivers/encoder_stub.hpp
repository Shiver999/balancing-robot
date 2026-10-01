#pragma once

#include "robot/interfaces.hpp"

namespace robot {

class EncoderStub final : public Encoders {
public:
    esp_err_t initialize() override;
    esp_err_t read(WheelMeasurement* measurement) override;

    bool initialized() const { return initialized_; }

private:
    bool initialized_{false};
};

Encoders& defaultEncoders();

}  // namespace robot
