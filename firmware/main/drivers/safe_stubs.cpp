#include "robot/interfaces.hpp"

namespace robot {

class SafeMotorDriver final : public MotorDriver {
public:
    esp_err_t initialize() override { return ESP_ERR_NOT_SUPPORTED; }
    void disable() override {}
    esp_err_t apply(const MotorCommand& command) override {
        (void)command;
        return ESP_ERR_NOT_SUPPORTED;
    }
    bool healthy() const override { return false; }
};

MotorDriver& safeMotorDriver() {
    static SafeMotorDriver driver;
    return driver;
}

}  // namespace robot
