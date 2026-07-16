#include "config.hpp"

#include <yaml-cpp/yaml.h>

#include <cmath>
#include <initializer_list>
#include <string>

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

// Rejects non-finite (NaN/inf) doubles. A NaN threshold silently disables the
// ADR-007 predicate, because every comparison against NaN is false.
void require_finite(double value, const char* field) {
    if (!std::isfinite(value)) {
        throw ConfigError(std::string("field must be finite: ") + field);
    }
}

void require_gt(double value, double bound, const char* field) {
    require_finite(value, field);
    if (value <= bound) {
        throw ConfigError(std::string("field must be > ") + std::to_string(bound) + ": " + field);
    }
}

void require_in(double value, double lo, double hi, const char* field) {
    require_finite(value, field);
    if (value < lo || value > hi) {
        throw ConfigError(std::string("field out of range [") + std::to_string(lo) + ", " +
                          std::to_string(hi) + "]: " + field);
    }
}

void require_nonempty(const std::string& value, const char* field) {
    if (value.empty()) {
        throw ConfigError(std::string("field must not be empty: ") + field);
    }
}

// Rejects a string outside `allowed`, listing the allowed values in the error so
// the operator can fix the config without reading source.
void require_one_of(const std::string& value,
                    std::initializer_list<const char*> allowed, const char* field) {
    for (const char* candidate : allowed) {
        if (value == candidate) {
            return;
        }
    }
    std::string message = std::string("invalid value '") + value + "' for " + field +
                          " (allowed:";
    for (const char* candidate : allowed) {
        message += std::string(" ") + candidate;
    }
    message += ")";
    throw ConfigError(message);
}

}  // namespace

Config Config::load(const std::string& path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(path);
    } catch (const YAML::Exception& e) {
        throw ConfigError(std::string("failed to load config '") + path + "': " + e.what());
    }
    if (!root.IsMap()) {
        throw ConfigError(std::string("config root is not a mapping: ") + path);
    }

    Config cfg;
    // Defence in depth: any yaml-cpp error inside the field walk (e.g. a
    // non-map node reached via operator[]) becomes an actionable ConfigError
    // rather than an uncaught exception that aborts the process.
    try {
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

        const YAML::Node logging = require_section(root, "logging");
        cfg.logging.level = require<std::string>(logging, "logging", "level");
        cfg.logging.output_dir = require<std::string>(logging, "logging", "output_dir");
        cfg.logging.max_file_size_mb = require<int>(logging, "logging", "max_file_size_mb");
    } catch (const ConfigError&) {
        throw;  // already actionable
    } catch (const YAML::Exception& e) {
        throw ConfigError(std::string("invalid config '") + path + "': " + e.what());
    }

    // Semantic validation. Presence + type is not enough: nonsensical values
    // (negative, out-of-range, NaN/inf) would otherwise flow into the
    // ADR-005/ADR-007 pipeline and fail far from the cause.
    if (cfg.camera.device_id < 0) {
        throw ConfigError("field must be >= 0: camera.device_id");
    }
    if (cfg.camera.target_fps <= 0) {
        throw ConfigError("field must be > 0: camera.target_fps");
    }
    require_gt(cfg.laser.modulation_frequency_hz, 0.0, "laser.modulation_frequency_hz");
    require_in(cfg.laser.modulation_duty_cycle, 0.0, 1.0, "laser.modulation_duty_cycle");
    require_gt(cfg.safe_for_control.age_max_ms, 0.0, "safe_for_control.age_max_ms");
    require_finite(cfg.safe_for_control.laser_settled_speed_m_per_s,
                   "safe_for_control.laser_settled_speed_m_per_s");
    if (cfg.safe_for_control.laser_settled_speed_m_per_s < 0.0) {
        throw ConfigError("field must be >= 0: safe_for_control.laser_settled_speed_m_per_s");
    }
    require_gt(cfg.safe_for_control.alignment_tolerance_m, 0.0,
              "safe_for_control.alignment_tolerance_m");
    require_gt(cfg.ball.radius_m, 0.0, "ball.radius_m");
    require_nonempty(cfg.zmq.bind_address, "zmq.bind_address");
    require_nonempty(cfg.calibration.intrinsics_path, "calibration.intrinsics_path");
    require_nonempty(cfg.calibration.extrinsics_path, "calibration.extrinsics_path");
    require_one_of(cfg.logging.level,
                   {"trace", "debug", "info", "warn", "error", "critical"},
                   "logging.level");
    require_nonempty(cfg.logging.output_dir, "logging.output_dir");
    if (cfg.logging.max_file_size_mb <= 0) {
        throw ConfigError("field must be > 0: logging.max_file_size_mb");
    }

    return cfg;
}

}  // namespace tracking
