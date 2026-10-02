#include "robot/drivers/mpu6xxx.hpp"
#include "robot/drivers/as5048a.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
// Always-on assertion used by isolated CTest suites; no hardware or ESP-IDF is required.
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
using namespace robot;
// Deterministic local time: fixtures advance it to exercise freshness and timing boundaries.
struct Clock : SensorClock {
    std::int64_t time = 1000;
    std::int64_t nowUs() const override { return time; }
    void delayMs(std::uint32_t ms) override { time += ms * 1000; }
};
// MPU register bank emulates accepted writes, bursts and configurable transport/readback faults.
struct Registers : ImuRegisterIo {
    std::array<std::uint8_t, 128> regs{};
    esp_err_t error = ESP_OK;
    bool mismatch = false, write_fail = false;
    Registers() { regs[0x75] = 0x70; }
    esp_err_t readRegisters(std::uint8_t address, std::uint8_t* out, std::size_t length) override {
        if (error != ESP_OK) return error;
        std::memcpy(out, regs.data() + address, length);
        if (mismatch && address == 0x1B) *out ^= 1;
        return ESP_OK;
    }
    esp_err_t writeRegister(std::uint8_t address, std::uint8_t value) override {
        if (error != ESP_OK) return error;
        if (write_fail) return ESP_FAIL;
        regs[address] = value; return ESP_OK;
    }
    // Place a signed axis word into its actual big-endian burst position.
    void raw(int index, int value) {
        const auto bits = static_cast<std::uint16_t>(value);
        regs[0x3B + index * 2] = bits >> 8; regs[0x3C + index * 2] = bits & 255;
    }
};
// Model the AS5048A one-frame pipeline, including parity, flags and diagnostic failures.
// Optional transfer time advances the shared fake clock to exercise pair-duration limits.
struct Frames : EncoderFrameIo {
    std::uint16_t pending = 0, angle = 0, diagnostic = 0x100;
    bool corrupt = false, flagged = false;
    esp_err_t error = ESP_OK;
    Clock* clock = nullptr;
    std::int64_t transfer_delay_us = 0;
    int clears = 0;
    esp_err_t transfer(std::uint16_t tx, std::uint16_t* rx) override {
        if (error != ESP_OK) return error;
        if (clock != nullptr) clock->time += transfer_delay_us;
        CHECK(As5048a::evenParity(tx));
        *rx = pending;
        if (tx == 0) { pending = 0; return ESP_OK; }
        CHECK((tx & 0x4000) != 0);
        const auto address = tx & 0x3FFF;
        if (address == 1) { ++clears; pending = 0; return ESP_OK; }
        CHECK(address == 0x3FFD || address == 0x3FFF);
        pending = address == 0x3FFD ? diagnostic : angle;
        if (flagged) pending |= 0x4000;
        if (!As5048a::evenParity(pending)) pending |= 0x8000;
        if (corrupt) pending ^= 0x8000;
        return ESP_OK;
    }
};
// Validate device/profile acceptance, signed SI conversion and invalid-output behavior.
void imuTests() {
    Clock clock; Registers io; Mpu6xxx imu(io, clock); ImuSample sample;
    CHECK(imu.read(&sample) == ESP_ERR_INVALID_STATE);
    CHECK(imu.initialize() == ESP_OK); CHECK(io.regs[0x19] == 4 && io.regs[0x6A] == 0x10);
    CHECK(imu.read(&sample) == ESP_ERR_NOT_FINISHED && !sample.valid);
    io.regs[0x3A] = 1; io.raw(0, 8192); io.raw(1, -8192); io.raw(2, 4096);
    io.raw(3, -1234); io.raw(4, 6550); io.raw(5, -6550); io.raw(6, 0);
    CHECK(imu.read(&sample) == ESP_OK && sample.valid && !sample.attitude_valid);
    CHECK(std::fabs(sample.acceleration_m_s2.x - 9.80665F) < 0.0001F);
    CHECK(std::fabs(sample.acceleration_m_s2.y + 9.80665F) < 0.0001F);
    CHECK(std::fabs(sample.angular_velocity_rad_s.y + 1.745329F) < 0.0001F);
    CHECK(sample.temperature_raw == -1234 && sample.device_id == 0x70);
    CHECK(imu.read(&sample) == ESP_ERR_INVALID_STATE && !sample.valid);
    ++clock.time; io.raw(0, 32767);
    CHECK(imu.read(&sample) == ESP_ERR_INVALID_RESPONSE && !sample.valid);
    io.raw(0, -32768); CHECK(imu.read(&sample) == ESP_ERR_INVALID_RESPONSE);
    io.error = ESP_FAIL; CHECK(imu.read(&sample) == ESP_FAIL && sample.timestamp_us == 0);
    CHECK(imu.read(nullptr) == ESP_ERR_INVALID_ARG);
    Registers wrong; wrong.regs[0x75] = 0xFF; Mpu6xxx unknown(wrong, clock);
    CHECK(unknown.initialize() == ESP_ERR_NOT_SUPPORTED);
    Registers mismatch; mismatch.mismatch = true; Mpu6xxx failed(mismatch, clock);
    CHECK(failed.initialize() == ESP_ERR_INVALID_RESPONSE);
    CHECK(failed.read(&sample) == ESP_ERR_INVALID_STATE);
    Registers bad_write; bad_write.write_fail = true; Mpu6xxx write_failed(bad_write, clock);
    CHECK(write_failed.initialize() == ESP_FAIL);
    CHECK(write_failed.read(&sample) == ESP_ERR_INVALID_STATE);
    Registers newer; newer.regs[0x75] = 0x71; Mpu6xxx m9250(newer, clock);
    CHECK(m9250.initialize() == ESP_OK);
    newer.regs.fill(255); CHECK(m9250.read(&sample) == ESP_ERR_INVALID_RESPONSE);
}
// Validate delayed responses, error clearing and magnet/offset diagnostic gates.
void encoderTests() {
    Frames io; As5048a encoder(io); EncoderReading reading;
    CHECK(encoder.read(&reading) == ESP_ERR_INVALID_STATE);
    CHECK(encoder.initialize() == ESP_OK && io.clears == 1);
    io.angle = 16383; CHECK(encoder.read(&reading) == ESP_OK && reading.angle_ticks == 16383);
    io.corrupt = true; CHECK(encoder.read(&reading) == ESP_ERR_INVALID_RESPONSE && reading.angle_ticks == 0);
    CHECK(io.clears == 2); io.corrupt = false; io.flagged = true;
    CHECK(encoder.read(&reading) == ESP_ERR_INVALID_RESPONSE && io.clears == 3);
    io.flagged = false;
    for (auto bad : {0, 0x300, 0x500, 0x900}) {
        io.diagnostic = bad; CHECK(encoder.read(&reading) == ESP_ERR_INVALID_RESPONSE);
    }
    io.diagnostic = 0x100; io.error = ESP_FAIL; CHECK(encoder.read(&reading) == ESP_FAIL);
    CHECK(encoder.read(nullptr) == ESP_ERR_INVALID_ARG);
}
// Exercise wraparound, software sign/zero, history loss, speed bounds and slow reads.
void wheelTests() {
    Clock clock; Frames lio, rio; As5048a left(lio), right(rio);
    WheelEncoderConfig config; config.right_direction = -1; config.right_zero_ticks = 100;
    As5048aEncoders wheels(left, right, clock, config); WheelMeasurement m;
    CHECK(wheels.initialize() == ESP_OK);
    lio.angle = 16380; rio.angle = 100; CHECK(wheels.read(&m) == ESP_OK && m.valid && !m.velocity_valid);
    clock.time += 10000; lio.angle = 4; rio.angle = 92;
    CHECK(wheels.read(&m) == ESP_OK && m.velocity_valid);
    CHECK(std::fabs(m.left_speed_rad_s - m.right_speed_rad_s) < 0.00001F);
    CHECK(m.left_speed_rad_s > 0 && m.left_unwrapped_angle_rad > 6.283F);
    CHECK(wheels.read(&m) == ESP_ERR_INVALID_STATE && !m.valid);
    clock.time += 10000; CHECK(wheels.read(&m) == ESP_OK && !m.velocity_valid);
    clock.time += 30000; CHECK(wheels.read(&m) == ESP_OK && !m.velocity_valid);
    clock.time += 10000; lio.angle = 8000;
    CHECK(wheels.read(&m) == ESP_ERR_INVALID_RESPONSE && !m.valid);
    clock.time += 10000; CHECK(wheels.read(&m) == ESP_OK && !m.velocity_valid);
    lio.error = ESP_FAIL; clock.time += 10000; CHECK(wheels.read(&m) == ESP_FAIL && !m.left_valid && !m.right_valid);
    lio.error = ESP_OK; clock.time += 10000; CHECK(wheels.read(&m) == ESP_OK && !m.velocity_valid);
    clock.time += 10000; lio.angle -= 8; rio.angle += 8;
    CHECK(wheels.read(&m) == ESP_OK && m.velocity_valid && m.left_speed_rad_s < 0 && m.right_speed_rad_s < 0);
    lio.clock = &clock; lio.transfer_delay_us = 1500;
    CHECK(wheels.read(&m) == ESP_ERR_INVALID_STATE && !m.valid);
    lio.transfer_delay_us = 0; clock.time += 10000;
    CHECK(wheels.read(&m) == ESP_OK && !m.velocity_valid);
    CHECK(wheels.read(nullptr) == ESP_ERR_INVALID_ARG);
    config.left_direction = 0; As5048aEncoders invalid(left, right, clock, config);
    CHECK(invalid.initialize() == ESP_ERR_INVALID_ARG);
}
// Run one suite per CTest invocation; unknown names fail rather than silently skipping.
int main(int argc, char** argv) {
    CHECK(argc == 2);
    if (std::strcmp(argv[1], "imu") == 0) imuTests();
    else if (std::strcmp(argv[1], "encoder") == 0) encoderTests();
    else if (std::strcmp(argv[1], "wheels") == 0) wheelTests();
    else return 1;
}
