#pragma once

#include "robot/types.hpp"

namespace robot {

// Fail-closed lifecycle scaffold: faults latch and no arming request succeeds.
// State tracking is software bookkeeping, not a physical emergency-stop circuit.
class SafetyManager {
public:
    SafetyManager() = default;

    // Preserve a latched fault instead of clearing it through a normal transition.
    void enterDisarmed();
    void latchFault(FaultCode fault);
    bool requestArm(bool sensors_valid, bool drivers_ready, bool explicit_arm_request);
    // Reserved balancing-to-remote-lost transition; unreachable in the scaffold.
    void onRemoteLeaseExpired();
    bool outputsAllowed() const;
    RobotState state() const { return state_; }
    FaultCode fault() const { return fault_; }

private:
    RobotState state_{RobotState::kBoot};
    FaultCode fault_{FaultCode::kNone};
};

}  // namespace robot
