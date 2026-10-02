#pragma once

#include "robot/interfaces.hpp"

namespace robot {

// Default unavailable adapter; failure clears samples rather than simulating wheels.
class EncoderStub final : public Encoders {
public:
    esp_err_t initialize() override;
    esp_err_t read(WheelMeasurement* measurement) override;
};

// Static fallback lifetime; the real bench explicitly composes its own drivers.
Encoders& defaultEncoders();

}  // namespace robot
