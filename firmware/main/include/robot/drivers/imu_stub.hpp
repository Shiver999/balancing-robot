#pragma once

#include "robot/interfaces.hpp"

namespace robot {

class ImuStub final : public Imu {
public:
    esp_err_t initialize() override;
    esp_err_t read(ImuSample* sample) override;

    bool initialized() const { return initialized_; }

private:
    bool initialized_{false};
};

Imu& defaultImu();

}  // namespace robot
