#pragma once

#include "robot/interfaces.hpp"

namespace robot {

// Unavailable hardware adapter. Synthetic samples belong in host tests only.
class ImuStub final : public Imu {
public:
    esp_err_t initialize() override;
    esp_err_t read(ImuSample* sample) override;
};

Imu& defaultImu();

}  // namespace robot
