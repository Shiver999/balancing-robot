#include "robot/safety/safety_manager.hpp"

namespace robot {

void SafetyManager::enterDisarmed() {
    if (state_ != RobotState::kFault) {
        state_ = RobotState::kDisarmed;
    }
}

void SafetyManager::latchFault(FaultCode fault) {
    fault_ = fault;
    state_ = RobotState::kFault;
}

bool SafetyManager::requestArm(bool sensors_valid, bool drivers_ready,
                               bool explicit_arm_request) {
    if (state_ != RobotState::kDisarmed || !sensors_valid || !drivers_ready ||
        !explicit_arm_request || fault_ != FaultCode::kNone) {
        return false;
    }
    // Deliberately do not arm yet. Tilt checks, local arm policy, and a verified
    // motor-disable path must be implemented before any arming transition.
    return false;
}

void SafetyManager::onRemoteLeaseExpired() {
    if (state_ == RobotState::kBalancing) {
        state_ = RobotState::kRemoteLost;
    }
}

bool SafetyManager::outputsAllowed() const {
    // No state currently permits outputs; hardware integration is not ready.
    return false;
}

}  // namespace robot
