#include "config.hpp"

#include <string>

#include <yaml-cpp/yaml.h>

namespace tracking {

ConfigError::ConfigError(const std::string& message)
    : std::runtime_error(message) {}

namespace {

// Returns the required map `section` under `root`, throwing ConfigError if it is
// absent or is not a map.
YAML::Node require_section(const YAML::Node& root, const char* section) {
    const YAML::Node node = root[section];
    if (!node || !node.IsMap()) {
        throw ConfigError(std::string("missing required section: ") + section);
    }
    return node;
}

// Returns the required scalar `section.key`, throwing ConfigError with the
// dotted path on absence or on a type that cannot convert to T.
template <typename T>
T require(const YAML::Node& section_node, const char* section, const char* key) {
    const YAML::Node node = section_node[key];
    if (!node || node.IsNull()) {
        throw ConfigError(std::string("missing required field: ") + section + "." + key);
    }
    try {
        return node.as<T>();
    } catch (const YAML::BadConversion&) {
        throw ConfigError(std::string("type mismatch for field: ") + section + "." + key);
    }
}

}  // namespace

Config Config::load(const std::string& path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(path);
    } catch (const YAML::Exception& e) {
        throw ConfigError(std::string("failed to load config '") + path + "': " + e.what());
    }

    Config cfg;

    const YAML::Node camera = require_section(root, "camera");
    cfg.camera.device_id = require<int>(camera, "camera", "device_id");
    cfg.camera.target_fps = require<int>(camera, "camera", "target_fps");

    const YAML::Node laser = require_section(root, "laser");
    cfg.laser.modulation_frequency_hz =
        require<double>(laser, "laser", "modulation_frequency_hz");
    cfg.laser.modulation_duty_cycle =
        require<double>(laser, "laser", "modulation_duty_cycle");

    const YAML::Node sfc = require_section(root, "safe_for_control");
    cfg.safe_for_control.age_max_ms =
        require<double>(sfc, "safe_for_control", "age_max_ms");
    cfg.safe_for_control.laser_settled_speed_m_per_s =
        require<double>(sfc, "safe_for_control", "laser_settled_speed_m_per_s");
    cfg.safe_for_control.alignment_tolerance_m =
        require<double>(sfc, "safe_for_control", "alignment_tolerance_m");

    const YAML::Node ball = require_section(root, "ball");
    cfg.ball.radius_m = require<double>(ball, "ball", "radius_m");

    const YAML::Node zmq = require_section(root, "zmq");
    cfg.zmq.bind_address = require<std::string>(zmq, "zmq", "bind_address");

    const YAML::Node calibration = require_section(root, "calibration");
    cfg.calibration.intrinsics_path =
        require<std::string>(calibration, "calibration", "intrinsics_path");
    cfg.calibration.extrinsics_path =
        require<std::string>(calibration, "calibration", "extrinsics_path");

    return cfg;
}

}  // namespace tracking
