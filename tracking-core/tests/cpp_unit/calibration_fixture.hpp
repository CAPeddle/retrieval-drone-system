#pragma once

// Shared synthetic calibration geometry for TRK-017..019 tests: a camera 1 m
// above the floor at 45 degrees tilt (the ADR-010 worked-example geometry),
// zero distortion, exact-fit homography. Provides ground truth (K, R, camera
// centre, H) plus JSON builders matching the TRK-012/013 artifact schemas so
// tests can mutate individual fields and write fixture files to TempDir().

#include <cmath>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>
#include <opencv2/core.hpp>

#include "config.hpp"

namespace calibration_fixture {

inline cv::Matx33d camera_matrix() {
    return {600.0, 0.0, 320.0,  //
            0.0, 600.0, 240.0,  //
            0.0, 0.0, 1.0};
}

inline cv::Vec3d camera_centre() { return {0.0, 0.0, 1.0}; }

// World -> camera rotation: camera at (0,0,1) looking at floor point (0,1,0),
// i.e. 45 degrees down-tilt toward +Y. Right-handed, image y down.
inline cv::Matx33d rotation() {
    const double s = 1.0 / std::sqrt(2.0);
    // rows: x_cam, y_cam, z_cam expressed in world coordinates
    return {1.0, 0.0, 0.0,   //
            0.0, -s, -s,     //
            0.0, s, -s};
}

// Homography floor(Z=0, metres) -> pixels: K [r1 r2 t] with t = -R C.
inline cv::Matx33d floor_to_pixel() {
    const cv::Matx33d r = rotation();
    const cv::Vec3d t = -(r * camera_centre());
    const cv::Matx33d cols{r(0, 0), r(0, 1), t[0],  //
                           r(1, 0), r(1, 1), t[1],  //
                           r(2, 0), r(2, 1), t[2]};
    return camera_matrix() * cols;
}

// Homography pixels -> floor (what the TRK-013 artifact stores).
inline cv::Matx33d pixel_to_floor() { return floor_to_pixel().inv(); }

// Projects a world point (any Z) to a pixel through the fixture camera.
inline cv::Point2d pixel_of_world(const cv::Vec3d& world) {
    const cv::Vec3d cam = rotation() * (world - camera_centre());
    const cv::Matx33d k = camera_matrix();
    return {k(0, 0) * cam[0] / cam[2] + k(0, 2), k(1, 1) * cam[1] / cam[2] + k(1, 2)};
}

inline cv::Point2d floor_of_pixel(const cv::Matx33d& h, double u, double v) {
    const double w = h(2, 0) * u + h(2, 1) * v + h(2, 2);
    return {(h(0, 0) * u + h(0, 1) * v + h(0, 2)) / w,
            (h(1, 0) * u + h(1, 1) * v + h(1, 2)) / w};
}

inline nlohmann::json matrix_json(const cv::Matx33d& m) {
    nlohmann::json rows = nlohmann::json::array();
    for (int r = 0; r < 3; ++r) {
        rows.push_back({m(r, 0), m(r, 1), m(r, 2)});
    }
    return rows;
}

inline nlohmann::json intrinsics_json() {
    return {
        {"intrinsics_version", 1},
        {"camera_id", "fixture-cam"},
        {"camera_matrix", matrix_json(camera_matrix())},
        {"dist_coeffs", {0.0, 0.0, 0.0, 0.0, 0.0}},
        {"image_size", {640, 480}},
        {"reprojection_error_px", 0.2},
        {"calibration_timestamp", "2026-07-01T10:00:00+00:00"},
    };
}

inline nlohmann::json extrinsics_json() {
    const cv::Matx33d h = pixel_to_floor();
    // Five well-spread visible floor anchors; pixels are the exact projections
    // so the artifact is perfectly self-consistent.
    const double floor_pts[5][2] = {
        {0.0, 1.0}, {0.8, 2.0}, {-0.8, 2.0}, {0.4, 3.0}, {-0.4, 1.5}};
    nlohmann::json anchors = nlohmann::json::array();
    for (int i = 0; i < 5; ++i) {
        const cv::Point2d px =
            pixel_of_world({floor_pts[i][0], floor_pts[i][1], 0.0});
        anchors.push_back({{"marker_id", i},
                           {"floor_x_m", floor_pts[i][0]},
                           {"floor_y_m", floor_pts[i][1]},
                           {"image_x_px", px.x},
                           {"image_y_px", px.y}});
    }
    return {
        {"extrinsics_version", 1},
        {"camera_id", "fixture-cam"},
        {"homography_3x3", matrix_json(h)},
        {"floor_anchor_points", anchors},
        {"reprojection_error_m", 1.0e-6},
        {"undistorted_coordinates", true},
        {"calibration_timestamp", "2026-07-02T10:00:00+00:00"},
    };
}

// Fixture AOI: entirely inside the camera's visible floor region. (Floor
// behind the camera legitimately flips the projective denominator, so an AOI
// reaching behind the camera FAILS validation by design.)
inline tracking::CoordinateConfig coordinate_config() {
    tracking::CoordinateConfig cfg;
    cfg.pixel_uncertainty_stddev_px = 1.0;
    cfg.condition_number_max = 1.0e5;  // gate runs on the unit-normalised H
    cfg.floor_aoi_x_min_m = -0.9;
    cfg.floor_aoi_x_max_m = 0.9;
    cfg.floor_aoi_y_min_m = 0.5;
    cfg.floor_aoi_y_max_m = 3.5;
    return cfg;
}

inline std::string write_json(const std::string& dir, const std::string& name,
                              const nlohmann::json& payload) {
    const std::string path = dir + name;
    std::ofstream(path) << payload.dump(2);
    return path;
}

}  // namespace calibration_fixture
