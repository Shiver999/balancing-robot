#pragma once

#include "robot/types.hpp"

namespace robot {

class SafetyManager {
public:
    SafetyManager() = default;

    void enterDisarmed();
    void latchFault(FaultCode fault);
    bool requestArm(bool sensors_valid, bool drivers_ready, bool explicit_arm_request);
    void onRemoteLeaseExpired();
    bool outputsAllowed() const;
    RobotState state() const { return state_; }
    FaultCode fault() const { return fault_; }

private:
    RobotState state_{RobotState::kBoot};
    FaultCode fault_{FaultCode::kNone};
};

}  // namespace robot
