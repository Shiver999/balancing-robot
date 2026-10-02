#include "robot/interfaces.hpp"

namespace robot {

// Inert adapter: it owns no GPIOs, so disable() has no physical effect.
// Real wiring must keep the motor hardware disabled independently of this stub.
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

// One process-lifetime fallback instance; no allocation or hardware initialization.
MotorDriver& safeMotorDriver() {
    static SafeMotorDriver driver;
    return driver;
}

}  // namespace robot
