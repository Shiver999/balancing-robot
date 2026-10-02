#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

#include "robot/comms/protocol.hpp"
#include "robot/control/balance_controller.hpp"
#include "robot/drivers/encoder_stub.hpp"
#include "robot/drivers/imu_stub.hpp"
#include "robot/robot_app.hpp"
#include "robot/safety/safety_manager.hpp"

namespace robot { MotorDriver& safeMotorDriver(); }

// Assertions remain active in all build types and terminate the current CTest suite.
#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    std::exit(1); \
} } while (false)

namespace {
using Packet = std::array<std::uint8_t, robot::protocol::kControlPacketSize>;
// Synthetic fixture limits; these are not measured/approved robot operating limits.
constexpr robot::protocol::CommandLimits kTestLimits{2.0F, 1.0F, 200};

// Independent fixture encoder keeps packet decoding tests independent of production helpers.
void putU16(Packet& packet, std::size_t offset, std::uint16_t value) {
    packet[offset] = static_cast<std::uint8_t>(value);
    packet[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

// Serialize float bits without pointer punning or host byte-order dependence.
void putFloat(Packet& packet, std::size_t offset, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (std::size_t i = 0; i < 4; ++i) {
        packet[offset + i] = static_cast<std::uint8_t>(bits >> (i * 8U));
    }
}

// Build a structurally valid packet that tests can mutate one field at a time.
Packet packet(std::uint16_t sequence = 1, float forward = 0.0F,
              float yaw = 0.0F, std::uint8_t flags = 3, std::uint16_t lease = 200) {
    Packet bytes{};
    bytes[0] = 1;
    bytes[1] = 1;
    putU16(bytes, 2, sequence);
    putFloat(bytes, 4, forward);
    putFloat(bytes, 8, yaw);
    bytes[12] = flags;
    putU16(bytes, 13, lease);
    return bytes;
}

// Check cross-platform bytes and reject malformed inputs without partial publication.
void protocolTests() {
    // Same golden vector as the Android encoder test, with explicit test limits.
    const Packet golden{1, 1, 0x34, 0x12, 0, 0, 0xC0, 0x3F,
                        0, 0, 0, 0xBF, 3, 0xC8, 0};
    robot::MotionRequest request{};
    CHECK(robot::protocol::decodeControlPacket(golden.data(), golden.size(), 1000, &request) == ESP_OK);
    CHECK(request.sequence == 0x1234 && request.forward_velocity_m_s == 1.5F);
    CHECK(request.yaw_rate_rad_s == -0.5F && request.lease_ms == 200);
    CHECK(request.arm_requested && request.deadman_active && request.received_at_us == 1000);
    CHECK(robot::protocol::decodeControlPacket(nullptr, golden.size(), 0, &request) == ESP_ERR_INVALID_ARG);
    CHECK(robot::protocol::decodeControlPacket(golden.data(), golden.size(), 0, nullptr) == ESP_ERR_INVALID_ARG);
    CHECK(robot::protocol::decodeControlPacket(golden.data(), golden.size() - 1, 0, &request) == ESP_ERR_INVALID_ARG);
    CHECK(robot::protocol::decodeControlPacket(golden.data(), golden.size() + 1, 0, &request) == ESP_ERR_INVALID_ARG);
    CHECK(robot::protocol::decodeControlPacket(golden.data(), golden.size(), -1, &request) == ESP_ERR_INVALID_ARG);

    auto rejected = [&](const Packet& bytes, esp_err_t expected = ESP_ERR_INVALID_ARG) {
        request.sequence = 4321;
        CHECK(robot::protocol::decodeControlPacket(bytes.data(), bytes.size(), 1000, &request) == expected);
        CHECK(request.sequence == 4321); // Failed decoding cannot publish partial data.
    };
    auto bytes = golden;
    bytes[0] = 2;
    rejected(bytes, ESP_ERR_INVALID_VERSION);
    bytes = golden;
    bytes[1] = 2;
    rejected(bytes, ESP_ERR_INVALID_VERSION);
    bytes = golden;
    bytes[12] = 0x80;
    rejected(bytes);
    for (float value : {std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity()}) {
        rejected(packet(1, value));
        rejected(packet(1, 0.0F, value));
    }
    rejected(packet(1, 0, 0, 3, 0));
    rejected(packet(1, 0, 0, 3, 201));
    rejected(packet(1, 0, 0, 3, 65535));
    for (std::uint8_t flags : {0, 1, 2}) {
        rejected(packet(1, 0.5F, 0, flags));
        rejected(packet(1, 0, -0.5F, flags));
    }
    bytes = packet(2, 0, 0, 0, 1); // Disarm/dead-man release remains admissible.
    CHECK(robot::protocol::decodeControlPacket(bytes.data(), bytes.size(), 0, &request) == ESP_OK);
}

// Verify limits, expiry boundaries and serial-number ordering including replay after expiry.
void mailboxTests() {
    auto accept = [](robot::protocol::CommandMailbox& box, const Packet& bytes, std::int64_t time = 0) {
        return box.accept(bytes.data(), bytes.size(), time);
    };
    robot::protocol::CommandMailbox defaults;
    CHECK(accept(defaults, packet(1, 0.01F)) == ESP_ERR_INVALID_ARG);
    CHECK(accept(defaults, packet(1, 0, -0.01F)) == ESP_ERR_INVALID_ARG);
    CHECK(accept(defaults, packet(1)) == ESP_OK);
    robot::protocol::CommandMailbox box(kTestLimits);
    CHECK(box.current(0).lease_ms == 0);
    CHECK(accept(box, packet(65535, 2.0F, -1.0F), 1000) == ESP_OK);
    CHECK(box.current(1000).forward_velocity_m_s == 2.0F);
    CHECK(box.current(200999).deadman_active);
    CHECK(box.current(201000).forward_velocity_m_s == 0.0F);
    CHECK(!box.current(201000).arm_requested && !box.current(201000).deadman_active);
    CHECK(box.current(999).lease_ms == 0);
    CHECK(accept(box, packet(65535), 201000) == ESP_ERR_INVALID_STATE); // Expiry does not permit replay.
    CHECK(accept(box, packet(65534), 201000) == ESP_ERR_INVALID_STATE);
    CHECK(accept(box, packet(32767), 201000) == ESP_ERR_INVALID_STATE); // Ambiguous half-range.
    CHECK(accept(box, packet(0, -2.0F, 1.0F), 201000) == ESP_OK); // Wraparound.
    CHECK(accept(box, packet(1, 2.01F), 202000) == ESP_ERR_INVALID_ARG);
    CHECK(accept(box, packet(1, 0, -1.01F), 202000) == ESP_ERR_INVALID_ARG);
    CHECK(accept(box, packet(1), 200999) == ESP_ERR_INVALID_STATE); // Clock goes backwards.
    CHECK(box.current(401000).lease_ms == 0); // Invalid packets did not renew the lease.
    CHECK(accept(box, packet(1, 0, 0, 0), 402000) == ESP_OK);
    CHECK(!box.current(402000).arm_requested && box.current(402000).forward_velocity_m_s == 0.0F);
    box.resetSession();
    CHECK(box.current(402000).lease_ms == 0);
    CHECK(accept(box, packet(0), 0) == ESP_OK);

    for (auto limits : {robot::protocol::CommandLimits{-1, 1, 200},
                        robot::protocol::CommandLimits{1, -1, 200},
                        robot::protocol::CommandLimits{1, 1, 0},
                        robot::protocol::CommandLimits{1, 1, 201},
                        robot::protocol::CommandLimits{std::numeric_limits<float>::quiet_NaN(), 1, 200},
                        robot::protocol::CommandLimits{1, std::numeric_limits<float>::infinity(), 200}}) {
        robot::protocol::CommandMailbox invalid(limits);
        CHECK(accept(invalid, packet()) == ESP_ERR_INVALID_ARG);
    }
    robot::protocol::CommandMailbox shorter({1, 1, 100});
    CHECK(accept(shorter, packet()) == ESP_ERR_INVALID_ARG);
    CHECK(accept(shorter, packet(1, 1, 1, 3, 100)) == ESP_OK);
    CHECK(shorter.current(100000).lease_ms == 0);
    box.resetSession();
    const auto near_max = std::numeric_limits<std::int64_t>::max() - 1;
    CHECK(accept(box, packet(), near_max) == ESP_OK);
    CHECK(box.current(near_max + 1).lease_ms == 200); // No overflowing deadline addition.
}

// Spy on startup ordering and writes; no physical hardware behavior is simulated.
class RecordingDriver final : public robot::MotorDriver {
public:
    esp_err_t initialize() override { CHECK(disables == 1); return result; }
    void disable() override { ++disables; }
    esp_err_t apply(const robot::MotorCommand&) override { ++applies; return ESP_OK; }
    bool healthy() const override { return ready; }
    esp_err_t result{ESP_OK};
    bool ready{true};
    int disables{0};
    int applies{0};
};

// Even healthy fixture hardware cannot make the scaffold arm or apply motor commands.
void safetyTests() {
    robot::SafetyManager safety;
    CHECK(safety.state() == robot::RobotState::kBoot && !safety.outputsAllowed());
    safety.enterDisarmed();
    CHECK(safety.state() == robot::RobotState::kDisarmed);
    CHECK(!safety.requestArm(true, true, true)); // Hardware arming remains unavailable.
    CHECK(!safety.requestArm(false, true, true));
    CHECK(!safety.requestArm(true, false, true));
    CHECK(!safety.requestArm(true, true, false));
    safety.latchFault(robot::FaultCode::kSensorUnavailable);
    safety.enterDisarmed();
    safety.onRemoteLeaseExpired();
    CHECK(safety.state() == robot::RobotState::kFault && !safety.outputsAllowed());
    CHECK(!safety.requestArm(true, true, true));
    CHECK(safety.fault() == robot::FaultCode::kSensorUnavailable);
    for (bool healthy : {false, true}) {
        for (esp_err_t result : {ESP_OK, ESP_ERR_NOT_SUPPORTED}) {
            RecordingDriver driver;
            driver.ready = healthy;
            driver.result = result;
            robot::RobotApp app(driver);
            const auto start_result = app.start();
            CHECK(start_result == (result != ESP_OK ? result : (healthy ? ESP_OK : ESP_FAIL)));
            CHECK(driver.disables == (result == ESP_OK && healthy ? 1 : 2));
            CHECK(driver.applies == 0);
        }
    }
    auto& inert = robot::safeMotorDriver();
    CHECK(inert.initialize() == ESP_ERR_NOT_SUPPORTED && !inert.healthy());
    CHECK(inert.apply({1, 1, true}) == ESP_ERR_NOT_SUPPORTED);
    robot::BalanceController controller;
    const auto command = controller.update({}, {}, {}, 0.001F);
    CHECK(!command.enable && command.left_output == 0 && command.right_output == 0);
}

// Seed stale valid outputs to prove unavailable adapters overwrite them on every failure.
void sensorTests() {
    auto& imu = robot::defaultImu();
    auto& encoders = robot::defaultEncoders();
    robot::ImuSample sample{123, 1.0F, 2.0F, true};
    robot::WheelMeasurement wheels{123, 1, 2, 3, 4, true};
    CHECK(imu.read(&sample) == ESP_ERR_NOT_SUPPORTED && !sample.valid && sample.timestamp_us == 0);
    CHECK(encoders.read(&wheels) == ESP_ERR_NOT_SUPPORTED && !wheels.valid && wheels.timestamp_us == 0);
    CHECK(imu.initialize() == ESP_ERR_NOT_SUPPORTED);
    CHECK(encoders.initialize() == ESP_ERR_NOT_SUPPORTED);
    sample.valid = true;
    wheels.valid = true;
    CHECK(imu.read(&sample) == ESP_ERR_NOT_SUPPORTED && !sample.valid);
    CHECK(encoders.read(&wheels) == ESP_ERR_NOT_SUPPORTED && !wheels.valid);
    CHECK(sample.pitch_rad == 0 && sample.pitch_rate_rad_s == 0);
    CHECK(wheels.left_angle_rad == 0 && wheels.right_speed_rad_s == 0);
    CHECK(imu.read(nullptr) == ESP_ERR_INVALID_ARG);
    CHECK(encoders.read(nullptr) == ESP_ERR_INVALID_ARG);
}
}  // namespace

// CTest selects one named suite per process for clear failure attribution.
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::string suite = argv[1];
    if (suite == "protocol") protocolTests();
    else if (suite == "mailbox") mailboxTests();
    else if (suite == "safety") safetyTests();
    else if (suite == "sensors") sensorTests();
    else return 2;
    std::printf("%s passed\n", argv[1]);
    return 0;
}
