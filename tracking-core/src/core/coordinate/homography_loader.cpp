// TRK-017: calibration artifact loading and validation. Field names mirror
// tools/calibrate_intrinsics.py and tools/calibrate_extrinsics.py exactly.

#include "homography_loader.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>

namespace tracking {

namespace {

using nlohmann::json;

// Self-consistency floor: an exact-fit artifact reports a tiny reprojection
// error, so the tolerance never drops below 1 mm.
constexpr double kMinSelfConsistencyToleranceM = 1.0e-3;

// Horizon safety: over the anchors + AOI corners, the projective denominator
// must keep one sign and min|w|/max|w| must exceed this ratio. Provisional —
// tightens/loosens with real install geometry.
constexpr double kMinDenominatorRatio = 0.01;

json load_json(const std::string& path, const char* label) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        throw CalibrationError(std::string(label) + " artifact missing: " + path);
    }
    try {
        return json::parse(stream);
    } catch (const json::exception& e) {
        throw CalibrationError(std::string(label) + " artifact malformed JSON (" + path +
                               "): " + e.what());
    }
}

// Required-field access with the artifact name in the error, mirroring the
// config loader's dotted-path idiom.
const json& require_field(const json& root, const char* field, const char* label) {
    const auto it = root.find(field);
    if (it == root.end()) {
        throw CalibrationError(std::string(label) + " artifact missing field: " + field);
    }
    return *it;
}

cv::Matx33d parse_matrix3x3(const json& node, const char* field, const char* label) {
    if (!node.is_array() || node.size() != 3) {
        throw CalibrationError(std::string(label) + " artifact field " + field +
                               " is not a 3x3 matrix");
    }
    cv::Matx33d m;
    for (int r = 0; r < 3; ++r) {
        const json& row = node[static_cast<std::size_t>(r)];
        if (!row.is_array() || row.size() != 3) {
            throw CalibrationError(std::string(label) + " artifact field " + field +
                                   " is not a 3x3 matrix");
        }
        for (int c = 0; c < 3; ++c) {
            const json& cell = row[static_cast<std::size_t>(c)];
            if (!cell.is_number()) {
                throw CalibrationError(std::string(label) + " artifact field " + field +
                                       " has a non-numeric entry");
            }
            m(r, c) = cell.get<double>();
        }
    }
    return m;
}

IntrinsicsData parse_intrinsics(const json& root, const std::string& path) {
    IntrinsicsData data;
    const int version = require_field(root, "intrinsics_version", "intrinsics").get<int>();
    if (version != 1) {
        throw CalibrationError("intrinsics artifact has unsupported intrinsics_version " +
                               std::to_string(version) + " (expected 1): " + path);
    }
    data.camera_id = require_field(root, "camera_id", "intrinsics").get<std::string>();
    data.camera_matrix =
        parse_matrix3x3(require_field(root, "camera_matrix", "intrinsics"),
                        "camera_matrix", "intrinsics");
    for (const json& coeff : require_field(root, "dist_coeffs", "intrinsics")) {
        data.dist_coeffs.push_back(coeff.get<double>());
    }
    const json& size = require_field(root, "image_size", "intrinsics");
    if (!size.is_array() || size.size() != 2) {
        throw CalibrationError("intrinsics artifact image_size is not [width, height]");
    }
    data.image_width = size[0].get<int>();
    data.image_height = size[1].get<int>();
    data.reprojection_error_px =
        require_field(root, "reprojection_error_px", "intrinsics").get<double>();
    data.calibration_timestamp =
        require_field(root, "calibration_timestamp", "intrinsics").get<std::string>();

    if (data.camera_matrix(0, 0) <= 0.0 || data.camera_matrix(1, 1) <= 0.0) {
        throw CalibrationError("intrinsics artifact camera_matrix has non-positive focal length");
    }
    if (data.image_width <= 0 || data.image_height <= 0) {
        throw CalibrationError("intrinsics artifact image_size must be positive");
    }
    if (data.dist_coeffs.empty()) {
        throw CalibrationError("intrinsics artifact dist_coeffs is empty");
    }
    for (const double v : data.dist_coeffs) {
        if (!std::isfinite(v)) {
            throw CalibrationError("intrinsics artifact dist_coeffs has a non-finite entry");
        }
    }
    return data;
}

HomographyData parse_extrinsics(const json& root, const std::string& path) {
    HomographyData data;
    const int version = require_field(root, "extrinsics_version", "extrinsics").get<int>();
    if (version != 1) {
        throw CalibrationError("extrinsics artifact has unsupported extrinsics_version " +
                               std::to_string(version) + " (expected 1): " + path);
    }
    data.camera_id = require_field(root, "camera_id", "extrinsics").get<std::string>();
    data.homography =
        parse_matrix3x3(require_field(root, "homography_3x3", "extrinsics"),
                        "homography_3x3", "extrinsics");
    for (const json& anchor : require_field(root, "floor_anchor_points", "extrinsics")) {
        FloorAnchor parsed;
        parsed.marker_id = require_field(anchor, "marker_id", "extrinsics").get<int>();
        parsed.floor_x_m = require_field(anchor, "floor_x_m", "extrinsics").get<double>();
        parsed.floor_y_m = require_field(anchor, "floor_y_m", "extrinsics").get<double>();
        parsed.image_x_px = require_field(anchor, "image_x_px", "extrinsics").get<double>();
        parsed.image_y_px = require_field(anchor, "image_y_px", "extrinsics").get<double>();
        data.anchors.push_back(parsed);
    }
    data.reprojection_error_m =
        require_field(root, "reprojection_error_m", "extrinsics").get<double>();
    data.calibration_timestamp =
        require_field(root, "calibration_timestamp", "extrinsics").get<std::string>();

    if (data.anchors.size() < 4) {
        throw CalibrationError("extrinsics artifact has fewer than 4 floor_anchor_points");
    }
    // KTD-7: a distorted-fit homography contaminates the camera-centre
    // recovery and can regionally exceed the R6/clause-8 budgets. Fatal, with
    // the recovery action in the message.
    if (!require_field(root, "undistorted_coordinates", "extrinsics").get<bool>()) {
        throw CalibrationError(
            "extrinsics artifact was fitted on raw distorted pixels "
            "(undistorted_coordinates=false); re-run calibrate_extrinsics.py with "
            "--intrinsics to produce an undistorted-fit artifact: " + path);
    }
    return data;
}

// Projects a pixel through H, returning the FloorPlane2D point; the caller has
// already established the denominator is safe over the validated region.
cv::Point2d project(const cv::Matx33d& h, double u, double v) {
    const double w = h(2, 0) * u + h(2, 1) * v + h(2, 2);
    return {(h(0, 0) * u + h(0, 1) * v + h(0, 2)) / w,
            (h(1, 0) * u + h(1, 1) * v + h(1, 2)) / w};
}

void validate_geometry(const HomographyData& extrinsics,
                       const IntrinsicsData& intrinsics,
                       const CoordinateConfig& coordinate) {
    // Degeneracy gate (ADR-006): spectral condition number of the UNIT-
    // NORMALISED homography. Raw pixel->metre entries mix scales, so the raw
    // cond is dominated by units, not geometry; normalising both domains to
    // [-1, 1] (pixels over the image, metres over the AOI) makes the gate
    // unit-free — healthy fits sit at O(1..1e3), collinear-anchor fits blow up.
    const double w = static_cast<double>(intrinsics.image_width);
    const double hgt = static_cast<double>(intrinsics.image_height);
    const cv::Matx33d norm_pix{2.0 / w, 0.0, -1.0,  //
                               0.0, 2.0 / hgt, -1.0,  //
                               0.0, 0.0, 1.0};
    const double ax = coordinate.floor_aoi_x_max_m - coordinate.floor_aoi_x_min_m;
    const double ay = coordinate.floor_aoi_y_max_m - coordinate.floor_aoi_y_min_m;
    const cv::Matx33d norm_floor{
        2.0 / ax, 0.0,
        -(coordinate.floor_aoi_x_max_m + coordinate.floor_aoi_x_min_m) / ax,  //
        0.0, 2.0 / ay,
        -(coordinate.floor_aoi_y_max_m + coordinate.floor_aoi_y_min_m) / ay,  //
        0.0, 0.0, 1.0};
    const cv::Matx33d h_normalised = norm_floor * extrinsics.homography * norm_pix.inv();
    cv::Mat singular_values;  // SVD reallocates its output; a Matx wrapper would be copied
    cv::SVD::compute(cv::Mat(h_normalised), singular_values);
    const double sigma_max = singular_values.at<double>(0);
    const double sigma_min = singular_values.at<double>(2);
    if (sigma_min <= 0.0 || sigma_max / sigma_min >= coordinate.condition_number_max) {
        throw CalibrationError(
            "extrinsics homography is near-singular (normalised condition number >= " +
            std::to_string(coordinate.condition_number_max) + ")");
    }

    // Horizon safety (plan R5): the projective denominator over the anchors
    // and the AOI corners (mapped floor->pixel via H^-1) must keep one sign
    // and stay bounded away from zero.
    const cv::Matx33d h_inv = extrinsics.homography.inv();
    std::vector<std::array<double, 2>> pixels;
    for (const FloorAnchor& anchor : extrinsics.anchors) {
        pixels.push_back({anchor.image_x_px, anchor.image_y_px});
    }
    const std::array<std::array<double, 2>, 4> aoi_floor = {{
        {coordinate.floor_aoi_x_min_m, coordinate.floor_aoi_y_min_m},
        {coordinate.floor_aoi_x_min_m, coordinate.floor_aoi_y_max_m},
        {coordinate.floor_aoi_x_max_m, coordinate.floor_aoi_y_min_m},
        {coordinate.floor_aoi_x_max_m, coordinate.floor_aoi_y_max_m},
    }};
    for (const auto& corner : aoi_floor) {
        const double wf = h_inv(2, 0) * corner[0] + h_inv(2, 1) * corner[1] + h_inv(2, 2);
        if (wf == 0.0) {
            throw CalibrationError("floor AOI corner maps through a zero denominator");
        }
        pixels.push_back({(h_inv(0, 0) * corner[0] + h_inv(0, 1) * corner[1] + h_inv(0, 2)) / wf,
                          (h_inv(1, 0) * corner[0] + h_inv(1, 1) * corner[1] + h_inv(1, 2)) / wf});
    }
    double min_abs = 0.0;
    double max_abs = 0.0;
    bool any_positive = false;
    bool any_negative = false;
    bool first = true;
    const cv::Matx33d& h = extrinsics.homography;
    for (const auto& px : pixels) {
        const double w = h(2, 0) * px[0] + h(2, 1) * px[1] + h(2, 2);
        (w > 0.0 ? any_positive : any_negative) = true;
        const double abs_w = std::abs(w);
        min_abs = first ? abs_w : std::min(min_abs, abs_w);
        max_abs = first ? abs_w : std::max(max_abs, abs_w);
        first = false;
    }
    if ((any_positive && any_negative) || max_abs == 0.0 ||
        min_abs / max_abs < kMinDenominatorRatio) {
        throw CalibrationError(
            "extrinsics homography denominator crosses or nears zero inside the "
            "anchor/AOI region (horizon inside the working area)");
    }

    // Self-consistency: the artifact carries its own evidence (plan R5).
    const double tolerance =
        std::max(extrinsics.reprojection_error_m, kMinSelfConsistencyToleranceM);
    for (const FloorAnchor& anchor : extrinsics.anchors) {
        const cv::Point2d mapped = project(h, anchor.image_x_px, anchor.image_y_px);
        const double err = std::hypot(mapped.x - anchor.floor_x_m,
                                      mapped.y - anchor.floor_y_m);
        if (!(err <= tolerance)) {
            throw CalibrationError(
                "extrinsics artifact is not self-consistent: anchor marker " +
                std::to_string(anchor.marker_id) + " reprojects with error " +
                std::to_string(err) + " m (tolerance " + std::to_string(tolerance) +
                " m) — the stored homography does not reproduce the stored anchors");
        }
    }
}

}  // namespace

CalibrationArtifacts load_calibration_artifacts(const std::string& intrinsics_path,
                                                const std::string& extrinsics_path,
                                                const CoordinateConfig& coordinate) {
    CalibrationArtifacts artifacts;
    try {
        artifacts.intrinsics =
            parse_intrinsics(load_json(intrinsics_path, "intrinsics"), intrinsics_path);
        artifacts.extrinsics =
            parse_extrinsics(load_json(extrinsics_path, "extrinsics"), extrinsics_path);
    } catch (const json::exception& e) {
        throw CalibrationError(std::string("calibration artifact parse error: ") + e.what());
    }

    // Pairing (plan R5): the two artifacts must describe the same camera, and
    // the undistorted-fit extrinsics must postdate the intrinsics that did the
    // undistorting — a re-calibrated K invalidates the extrinsics fit (live
    // risk R-05, focus drift) with every per-file check still passing.
    if (artifacts.intrinsics.camera_id != artifacts.extrinsics.camera_id) {
        throw CalibrationError("calibration artifacts disagree on camera_id ('" +
                               artifacts.intrinsics.camera_id + "' vs '" +
                               artifacts.extrinsics.camera_id + "')");
    }
    if (artifacts.extrinsics.calibration_timestamp <=
        artifacts.intrinsics.calibration_timestamp) {
        throw CalibrationError(
            "extrinsics artifact predates the intrinsics it was undistorted with; "
            "re-run calibrate_extrinsics.py --intrinsics after any intrinsics "
            "re-calibration");
    }

    validate_geometry(artifacts.extrinsics, artifacts.intrinsics, coordinate);
    return artifacts;
}

}  // namespace tracking
