#include "config.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

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
  width: 640
  height: 480
  exposure_us: 5000
pipeline:
  ring_buffer_capacity: 4
  capture_cpu_core: 2
  capture_thread_priority: 80
frame_quality:
  underexposed_threshold: 20
  overexposed_threshold: 240
  blur_threshold: 100
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
logging:
  level: warn
  output_dir: "/tmp/tracking_core/"
  max_file_size_mb: 10
)";

TEST_F(ConfigTest, LoadsValidConfig) {
    const tracking::Config cfg = tracking::Config::load(write_temp(kValidYaml));

    EXPECT_EQ(cfg.camera.device_id, 0);
    EXPECT_EQ(cfg.camera.target_fps, 60);
    EXPECT_EQ(cfg.camera.width, 640);
    EXPECT_EQ(cfg.camera.height, 480);
    EXPECT_EQ(cfg.pipeline.ring_buffer_capacity, 4);
    EXPECT_EQ(cfg.camera.exposure_us, 5000);
    EXPECT_EQ(cfg.pipeline.capture_cpu_core, 2);
    EXPECT_EQ(cfg.pipeline.capture_thread_priority, 80);
    EXPECT_DOUBLE_EQ(cfg.frame_quality.underexposed_threshold, 20.0);
    EXPECT_DOUBLE_EQ(cfg.frame_quality.overexposed_threshold, 240.0);
    EXPECT_DOUBLE_EQ(cfg.frame_quality.blur_threshold, 100.0);
    EXPECT_DOUBLE_EQ(cfg.laser.modulation_frequency_hz, 15.0);
    EXPECT_DOUBLE_EQ(cfg.laser.modulation_duty_cycle, 0.5);
    EXPECT_DOUBLE_EQ(cfg.safe_for_control.age_max_ms, 50.0);
    EXPECT_DOUBLE_EQ(cfg.safe_for_control.laser_settled_speed_m_per_s, 0.05);
    EXPECT_DOUBLE_EQ(cfg.safe_for_control.alignment_tolerance_m, 0.02);
    EXPECT_DOUBLE_EQ(cfg.ball.radius_m, 0.03);
    EXPECT_EQ(cfg.zmq.bind_address, "tcp://*:5556");
    EXPECT_EQ(cfg.calibration.intrinsics_path, "config/intrinsics.json");
    EXPECT_EQ(cfg.calibration.extrinsics_path, "config/extrinsics.json");
    EXPECT_EQ(cfg.logging.level, "warn");
    EXPECT_EQ(cfg.logging.output_dir, "/tmp/tracking_core/");
    EXPECT_EQ(cfg.logging.max_file_size_mb, 10);
}

TEST_F(ConfigTest, ThrowsOnMissingRequiredField) {
    // ball.radius_m is absent.
    const std::string yaml = R"(
camera: {device_id: 0, target_fps: 60, width: 640, height: 480, exposure_us: 5000}
laser: {modulation_frequency_hz: 15.0, modulation_duty_cycle: 0.5}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
logging: {level: warn, output_dir: "/tmp/tracking_core/", max_file_size_mb: 10}
pipeline: {ring_buffer_capacity: 4, capture_cpu_core: 2, capture_thread_priority: 80}
frame_quality: {underexposed_threshold: 20, overexposed_threshold: 240, blur_threshold: 100}
)";
    EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError);
}

TEST_F(ConfigTest, ThrowsOnMissingRequiredSection) {
    // The entire `laser` section is absent.
    const std::string yaml = R"(
camera: {device_id: 0, target_fps: 60, width: 640, height: 480, exposure_us: 5000}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {radius_m: 0.03}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
logging: {level: warn, output_dir: "/tmp/tracking_core/", max_file_size_mb: 10}
pipeline: {ring_buffer_capacity: 4, capture_cpu_core: 2, capture_thread_priority: 80}
frame_quality: {underexposed_threshold: 20, overexposed_threshold: 240, blur_threshold: 100}
)";
    EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError);
}

TEST_F(ConfigTest, ThrowsOnTypeMismatch) {
    // ball.radius_m is a non-numeric string.
    const std::string yaml = R"(
camera: {device_id: 0, target_fps: 60, width: 640, height: 480, exposure_us: 5000}
laser: {modulation_frequency_hz: 15.0, modulation_duty_cycle: 0.5}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {radius_m: "not_a_number"}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
logging: {level: warn, output_dir: "/tmp/tracking_core/", max_file_size_mb: 10}
pipeline: {ring_buffer_capacity: 4, capture_cpu_core: 2, capture_thread_priority: 80}
frame_quality: {underexposed_threshold: 20, overexposed_threshold: 240, blur_threshold: 100}
)";
    EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError);
}

TEST_F(ConfigTest, ThrowsOnMissingFile) {
    EXPECT_THROW(tracking::Config::load("/nonexistent/path/tracking_core.yaml"),
                 tracking::ConfigError);
}

TEST_F(ConfigTest, ThrowsOnNonMapRoot) {
    // A scalar/sequence root must fail cleanly, not abort the process.
    EXPECT_THROW(tracking::Config::load(write_temp("just a scalar, not a map\n")),
                 tracking::ConfigError);
}

TEST_F(ConfigTest, ThrowsOnNonFiniteThreshold) {
    // A NaN safety threshold must be rejected — it silently disables ADR-007.
    const std::string yaml = R"(
camera: {device_id: 0, target_fps: 60, width: 640, height: 480, exposure_us: 5000}
laser: {modulation_frequency_hz: 15.0, modulation_duty_cycle: 0.5}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: .nan}
ball: {radius_m: 0.03}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
logging: {level: warn, output_dir: "/tmp/tracking_core/", max_file_size_mb: 10}
pipeline: {ring_buffer_capacity: 4, capture_cpu_core: 2, capture_thread_priority: 80}
frame_quality: {underexposed_threshold: 20, overexposed_threshold: 240, blur_threshold: 100}
)";
    EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError);
}

TEST_F(ConfigTest, ThrowsOnNonPositiveRadius) {
    const std::string yaml = R"(
camera: {device_id: 0, target_fps: 60, width: 640, height: 480, exposure_us: 5000}
laser: {modulation_frequency_hz: 15.0, modulation_duty_cycle: 0.5}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {radius_m: -0.03}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
logging: {level: warn, output_dir: "/tmp/tracking_core/", max_file_size_mb: 10}
pipeline: {ring_buffer_capacity: 4, capture_cpu_core: 2, capture_thread_priority: 80}
frame_quality: {underexposed_threshold: 20, overexposed_threshold: 240, blur_threshold: 100}
)";
    EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError);
}

TEST_F(ConfigTest, ThrowsOnDutyCycleOutOfRange) {
    const std::string yaml = R"(
camera: {device_id: 0, target_fps: 60, width: 640, height: 480, exposure_us: 5000}
laser: {modulation_frequency_hz: 15.0, modulation_duty_cycle: 5.0}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {radius_m: 0.03}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
logging: {level: warn, output_dir: "/tmp/tracking_core/", max_file_size_mb: 10}
pipeline: {ring_buffer_capacity: 4, capture_cpu_core: 2, capture_thread_priority: 80}
frame_quality: {underexposed_threshold: 20, overexposed_threshold: 240, blur_threshold: 100}
)";
    EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError);
}

TEST_F(ConfigTest, ThrowsOnEmptyBindAddress) {
    const std::string yaml = R"(
camera: {device_id: 0, target_fps: 60, width: 640, height: 480, exposure_us: 5000}
laser: {modulation_frequency_hz: 15.0, modulation_duty_cycle: 0.5}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {radius_m: 0.03}
zmq: {bind_address: ""}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
logging: {level: warn, output_dir: "/tmp/tracking_core/", max_file_size_mb: 10}
pipeline: {ring_buffer_capacity: 4, capture_cpu_core: 2, capture_thread_priority: 80}
frame_quality: {underexposed_threshold: 20, overexposed_threshold: 240, blur_threshold: 100}
)";
    EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError);
}

TEST_F(ConfigTest, ThrowsOnMissingLoggingSection) {
    const std::string yaml = R"(
camera: {device_id: 0, target_fps: 60, width: 640, height: 480, exposure_us: 5000}
laser: {modulation_frequency_hz: 15.0, modulation_duty_cycle: 0.5}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {radius_m: 0.03}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
pipeline: {ring_buffer_capacity: 4, capture_cpu_core: 2, capture_thread_priority: 80}
frame_quality: {underexposed_threshold: 20, overexposed_threshold: 240, blur_threshold: 100}
)";
    EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError);
}

TEST_F(ConfigTest, ThrowsOnInvalidLogLevel) {
    // "verbose" is not a member of the spdlog level set.
    const std::string yaml = R"(
camera: {device_id: 0, target_fps: 60, width: 640, height: 480, exposure_us: 5000}
laser: {modulation_frequency_hz: 15.0, modulation_duty_cycle: 0.5}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {radius_m: 0.03}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
logging: {level: verbose, output_dir: "/tmp/tracking_core/", max_file_size_mb: 10}
pipeline: {ring_buffer_capacity: 4, capture_cpu_core: 2, capture_thread_priority: 80}
frame_quality: {underexposed_threshold: 20, overexposed_threshold: 240, blur_threshold: 100}
)";
    try {
        tracking::Config::load(write_temp(yaml));
        FAIL() << "expected ConfigError";
    } catch (const tracking::ConfigError& e) {
        // The error must name the field and list the allowed values.
        EXPECT_NE(std::string(e.what()).find("logging.level"), std::string::npos);
        EXPECT_NE(std::string(e.what()).find("warn"), std::string::npos);
    }
}

TEST_F(ConfigTest, ThrowsOnNonPositiveMaxFileSize) {
    // 300 exceeds the 256 MB upper bound (4 files on RAM-backed tmpfs).
    for (const char* size : {"0", "-5", "300"}) {
        const std::string yaml = std::string(R"(
camera: {device_id: 0, target_fps: 60, width: 640, height: 480, exposure_us: 5000}
laser: {modulation_frequency_hz: 15.0, modulation_duty_cycle: 0.5}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {radius_m: 0.03}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
pipeline: {ring_buffer_capacity: 4, capture_cpu_core: 2, capture_thread_priority: 80}
frame_quality: {underexposed_threshold: 20, overexposed_threshold: 240, blur_threshold: 100}
logging: {level: warn, output_dir: "/tmp/tracking_core/", max_file_size_mb: )") +
                                 size + "}\n";
        EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError)
            << "max_file_size_mb=" << size;
    }
}

TEST_F(ConfigTest, ThrowsOnEmptyLogOutputDir) {
    const std::string yaml = R"(
camera: {device_id: 0, target_fps: 60, width: 640, height: 480, exposure_us: 5000}
laser: {modulation_frequency_hz: 15.0, modulation_duty_cycle: 0.5}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {radius_m: 0.03}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
logging: {level: warn, output_dir: "", max_file_size_mb: 10}
pipeline: {ring_buffer_capacity: 4, capture_cpu_core: 2, capture_thread_priority: 80}
frame_quality: {underexposed_threshold: 20, overexposed_threshold: 240, blur_threshold: 100}
)";
    EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError);
}

TEST_F(ConfigTest, ThrowsOnBadRingBufferCapacity) {
    // Zero, negative, and above the 64-slot pre-allocation ceiling.
    for (const char* cap : {"0", "-2", "100"}) {
        const std::string yaml = std::string(R"(
camera: {device_id: 0, target_fps: 60, width: 640, height: 480, exposure_us: 5000}
laser: {modulation_frequency_hz: 15.0, modulation_duty_cycle: 0.5}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {radius_m: 0.03}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
logging: {level: warn, output_dir: "/tmp/tracking_core/", max_file_size_mb: 10}
pipeline: {ring_buffer_capacity: )") + cap + ", capture_cpu_core: 2, capture_thread_priority: 80}\nframe_quality: {underexposed_threshold: 20, overexposed_threshold: 240, blur_threshold: 100}\n";
        EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError)
            << "ring_buffer_capacity=" << cap;
    }
}

TEST_F(ConfigTest, ThrowsOnInvertedExposureThresholds) {
    const std::string yaml = R"(
camera: {device_id: 0, target_fps: 60, width: 640, height: 480, exposure_us: 5000}
laser: {modulation_frequency_hz: 15.0, modulation_duty_cycle: 0.5}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {radius_m: 0.03}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
logging: {level: warn, output_dir: "/tmp/tracking_core/", max_file_size_mb: 10}
pipeline: {ring_buffer_capacity: 4, capture_cpu_core: 2, capture_thread_priority: 80}
frame_quality: {underexposed_threshold: 240, overexposed_threshold: 20, blur_threshold: 100}
)";
    EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError);
}

TEST_F(ConfigTest, ThrowsOnCaptureThreadPriorityOutOfRange) {
    // SCHED_FIFO priorities are 1-99.
    for (const char* prio : {"0", "100"}) {
        const std::string yaml = std::string(R"(
camera: {device_id: 0, target_fps: 60, width: 640, height: 480, exposure_us: 5000}
laser: {modulation_frequency_hz: 15.0, modulation_duty_cycle: 0.5}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {radius_m: 0.03}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
logging: {level: warn, output_dir: "/tmp/tracking_core/", max_file_size_mb: 10}
pipeline: {ring_buffer_capacity: 4, capture_cpu_core: 2, capture_thread_priority: )") + prio + "}\nframe_quality: {underexposed_threshold: 20, overexposed_threshold: 240, blur_threshold: 100}\n";
        EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError)
            << "capture_thread_priority=" << prio;
    }
}

TEST_F(ConfigTest, ThrowsOnNonPositiveFrameDimensions) {
    const std::string yaml = R"(
camera: {device_id: 0, target_fps: 60, width: 0, height: 480, exposure_us: 5000}
laser: {modulation_frequency_hz: 15.0, modulation_duty_cycle: 0.5}
safe_for_control: {age_max_ms: 50, laser_settled_speed_m_per_s: 0.05, alignment_tolerance_m: 0.02}
ball: {radius_m: 0.03}
zmq: {bind_address: "tcp://*:5556"}
calibration: {intrinsics_path: "a.json", extrinsics_path: "b.json"}
logging: {level: warn, output_dir: "/tmp/tracking_core/", max_file_size_mb: 10}
pipeline: {ring_buffer_capacity: 4, capture_cpu_core: 2, capture_thread_priority: 80}
frame_quality: {underexposed_threshold: 20, overexposed_threshold: 240, blur_threshold: 100}
)";
    EXPECT_THROW(tracking::Config::load(write_temp(yaml)), tracking::ConfigError);
}

#ifdef TRACKING_CORE_CONFIG_TEMPLATE
TEST(ConfigTemplateTest, ShippedTemplateLoads) {
    // The shipped config/tracking_core.yaml must load through the real loader —
    // catches struct-vs-template drift (a field in one but not the other).
    EXPECT_NO_THROW(tracking::Config::load(TRACKING_CORE_CONFIG_TEMPLATE));
}
#endif

}  // namespace
