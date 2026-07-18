#pragma once

// TRK-018/019: pixel -> FloorPlane2D projection with object-class Z
// compensation (ADR-010) and Jacobian uncertainty propagation, per plan KTD-4.
//
// The extrinsics artifact stores only H (undistorted pixels -> floor Z=0), so
// the camera centre C is recovered once at construction via plane-pose
// extraction: B = K^-1 * H^-1 (H^-1 maps floor->pixels, so B ∝ [r1 r2 t]),
// orthonormalise, sign chosen so C_z > 0. At runtime an observation maps
// through H to the Z=0 point X0, then scales along the camera ray:
//     X_h = C + (C_z - h)/C_z * (X0 - C)     h = expected_height(type)
// which equals the ray-plane intersection at Z = h using only H, K, and C.
// All matrix work happens at construction; the per-observation cost is an
// undistortion, a homography multiply, and a scale (<= 0.1 ms, TRK-018).

#include <array>
#include <cstdint>

#include <opencv2/core.hpp>

#include "config.hpp"
#include "homography_loader.hpp"
#include "track.hpp"

namespace tracking {

struct FloorPosition {
    double x_m = 0.0;
    double y_m = 0.0;
    double z_m = 0.0;            // expected_height(type) per ADR-010
    double uncertainty_m = 0.0;  // TRK-019 Jacobian-propagated (1-sigma)
    bool valid = false;          // false: degenerate mapping, observation dropped (plan R8)
};

class CoordinateMapper {
public:
    // Throws CalibrationError if the plane-pose extraction fails (startup
    // context). `ball_radius_m` supplies expected_height(Ball) per ADR-010.
    CoordinateMapper(const CalibrationArtifacts& artifacts,
                     const CoordinateConfig& coordinate, double ball_radius_m);

    // Hot path: no allocation, no exceptions. Returns valid=false (never NaN)
    // for degenerate inputs: near-zero projective denominator, non-positive
    // height-scale geometry, or a result outside the configured floor AOI.
    FloorPosition project_to_floor(const cv::Point2f& pixel, ObjectType type) const;

    // Camera centre in FloorPlane2D coordinates (recovered at construction).
    cv::Vec3d camera_centre() const noexcept { return camera_centre_; }

    double expected_height_m(ObjectType type) const noexcept;

private:
    cv::Point2d undistort(const cv::Point2f& pixel) const;

    cv::Matx33d homography_;         // undistorted pixels -> floor Z=0
    cv::Matx33d camera_matrix_;
    cv::Vec3d camera_centre_{0.0, 0.0, 0.0};
    std::array<double, 8> dist_coeffs_{};  // zero-padded to 8 (OpenCV rational model)
    int dist_coeff_count_ = 0;
    double ball_radius_m_ = 0.0;
    double pixel_uncertainty_stddev_px_ = 0.0;
    CoordinateConfig coordinate_;
};

}  // namespace tracking
