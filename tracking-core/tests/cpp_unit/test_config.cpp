#include "config.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

// Writes YAML content to a unique temp file per test and removes it on teardown.
class ConfigTest : public ::testing::Test {
protected:
    std::string write_temp(const std::string& content) {
        const std::string path = ::testing::TempDir() + "trk003_" +
            ::testing::UnitTest::GetInstance()->current_test_info()->name() + ".yaml";
        std::ofstream(path) << content;
        temp_paths_.push_back(path);
        return path;
    }

    void TearDown() override {
        for (const std::string& path : temp_paths_) {
            std::remove(path.c_str());
        }
    }

private:
    std::vector<std::string> temp_paths_;
};

constexpr const char* kValidYaml = R"(
camera:
  device_id: 0
  target_fps: 60
laser:
  modulation_frequency_hz: 15.0
  modulation_duty_cycle: 0.5
safe_for_control:
  age_max_ms: 50
  laser_settled_speed_m_per_s: 0.05
  alignment_tolerance_m: 0.02
ball:
  radius_m: 0.03
zmq:
  bind_address: "tcp://*:5556"
calibration:
  intrinsics_path: "config/intrinsics.json"
  extrinsics_path: "config/extrinsics.json"
)";

TEST_F(ConfigTest, LoadsValidConfig) {
    const tracking::Config cfg = tracking::Config::load(write_temp(kValidYaml));

    EXPECT_EQ(cfg.camera.device_id, 0);
    EXPECT_EQ(cfg.camera.target_fps, 60);
    EXPECT_DOUBLE_EQ(cfg.laser.modulation_frequency_hz, 15.0);
    EXPECT_DOUBLE_EQ(cfg.laser.modulation_duty_cycle, 0.5);
    EXPECT_DOUBLE_EQ(cfg.safe_for_control.age_max_ms, 50.0);
    EXPECT_DOUBLE_EQ(cfg.safe_for_control.laser_settled_speed_m_per_s, 0.05);
    EXPECT_DOUBLE_EQ(cfg.safe_for_control.alignment_tolerance_m, 0.02);
    EXPECT_DOUBLE_EQ(cfg.ball.radius_m, 0.03);
    EXPECT_EQ(cfg.zmq.bind_address, "tcp://*:5556");
    EXPECT_EQ(cfg.calibration.intrinsics_path, "config/intrinsics.json");
    EXPECT_EQ(cfg.calibration.extrinsics_path, "config/extrinsics.json");
}

TEST_F(ConfigTest, ThrowsOnMissingRequiredField) {
    // ball.radius_m is absent.
    const std::string yaml = R"(
camera: {device_id: 0, target_fps: 60}
laser: {modulation_frequency_hz: 15.0, modulation_duty_cycle: 0.5}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
)";
    EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError);
}

TEST_F(ConfigTest, ThrowsOnMissingRequiredSection) {
    // The entire `laser` section is absent.
    const std::string yaml = R"(
camera: {device_id: 0, target_fps: 60}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {radius_m: 0.03}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
)";
    EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError);
}

TEST_F(ConfigTest, ThrowsOnTypeMismatch) {
    // ball.radius_m is a non-numeric string.
    const std::string yaml = R"(
camera: {device_id: 0, target_fps: 60}
laser: {modulation_frequency_hz: 15.0, modulation_duty_cycle: 0.5}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {radius_m: "not_a_number"}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
)";
    EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError);
}

TEST_F(ConfigTest, ThrowsOnMissingFile) {
    EXPECT_THROW(tracking::Config::load("/nonexistent/path/tracking_core.yaml"),
                 tracking::ConfigError);
}

}  // namespace
