#pragma once

// TRK-017: startup loading and validation of the calibration artifacts
// (TRK-012 intrinsics JSON, TRK-013 extrinsics JSON). Fail-fast per plan
// KTD-7: any defect — schema, geometry, self-consistency, pairing, or a
// distorted-fit homography — is a fatal startup error with a precise message.
// Startup-only code: throwing and std::string are permitted here.
//
// The self-consistency check follows the artifact-contract learning
// (docs/solutions/python/2026-07-18-artifact-metadata-derived-not-asserted.md):
// the artifact carries its own evidence — stored anchors mapped through the
// stored homography must reproduce the stored floor coordinates — so a lying
// metadata flag is detectable rather than silently biasing safe_for_control.

#include <stdexcept>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "config.hpp"

namespace tracking {

class CalibrationError : public std::runtime_error {
public:
    explicit CalibrationError(const std::string& message)
        : std::runtime_error(message) {}
};

struct IntrinsicsData {
    cv::Matx33d camera_matrix;
    std::vector<double> dist_coeffs;
    int image_width = 0;
    int image_height = 0;
    double reprojection_error_px = 0.0;
    std::string camera_id;
    std::string calibration_timestamp;  // ISO 8601 UTC (lexicographically ordered)
};

struct FloorAnchor {
    int marker_id = 0;
    double floor_x_m = 0.0;
    double floor_y_m = 0.0;
    double image_x_px = 0.0;
    double image_y_px = 0.0;
};

struct HomographyData {
    cv::Matx33d homography;  // undistorted pixels -> FloorPlane2D metres (ADR-006)
    std::vector<FloorAnchor> anchors;
    double reprojection_error_m = 0.0;
    std::string camera_id;
    std::string calibration_timestamp;
};

struct CalibrationArtifacts {
    IntrinsicsData intrinsics;
    HomographyData extrinsics;
};

// Loads and validates both artifacts (plan R5). Throws CalibrationError on:
// unreadable/malformed JSON; version != 1; condition number >=
// coordinate.condition_number_max; a projective-denominator sign flip or
// near-zero inside the anchors + floor-AOI region (horizon safety — a
// determinant-sign check is NOT used: homographies are scale-ambiguous, so
// det sign is a fitting-convention artefact); anchors that fail to reproduce
// their stored floor coordinates through the stored homography;
// undistorted_coordinates == false (re-run calibrate_extrinsics.py with
// --intrinsics); camera_id mismatch between the artifacts; or an extrinsics
// timestamp that predates the intrinsics it claims to have undistorted with.
CalibrationArtifacts load_calibration_artifacts(const std::string& intrinsics_path,
                                                const std::string& extrinsics_path,
                                                const CoordinateConfig& coordinate);

}  // namespace tracking
