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

    // The firmware is still intentionally inert until the board-level hardware
    // checks are verified, but we now model the proper local transition to the
    // READY state to support the eventual control loop and arm policy.
    state_ = RobotState::kReady;
    return true;
}

void SafetyManager::onRemoteLeaseExpired() {
    if (state_ == RobotState::kBalancing) {
        state_ = RobotState::kRemoteLost;
    }
}

bool SafetyManager::outputsAllowed() const {
    // Keep outputs disabled until verified hardware and board-level safety checks
    // are in place. The state machine still tracks readiness without permitting
    // actuation from the scaffold.
    return false;
}

}  // namespace robot
