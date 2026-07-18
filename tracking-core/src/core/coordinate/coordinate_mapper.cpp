// TRK-018/019: coordinate mapper implementation. Plane-pose extraction and
// closed-form Z compensation per plan KTD-4; analytic Jacobian uncertainty per
// TRK-019 (undistortion Jacobian treated as identity — near-unity across the
// calibrated region; the numeric cross-check lives in the tests).

#include "coordinate_mapper.hpp"

#include <cmath>

namespace tracking {

namespace {

// Hot-path degeneracy floors (plan R8). The homography is Frobenius-normalised
// at construction, so the absolute denominator epsilon is meaningful.
constexpr double kMinProjectiveDenominator = 1.0e-9;
constexpr double kMinHeightClearanceM = 1.0e-6;

// Iterative inverse of the OpenCV plumb-bob distortion model (k1 k2 p1 p2 k3).
// Allocation-free equivalent of cv::undistortPoints(..., P=K) for one point;
// zero coefficients reduce to the identity.
cv::Point2d undistort_normalised(double xd, double yd, const std::array<double, 8>& k) {
    double x = xd;
    double y = yd;
    for (int i = 0; i < 8; ++i) {
        const double r2 = x * x + y * y;
        const double radial = 1.0 + r2 * (k[0] + r2 * (k[1] + r2 * k[4]));
        const double dx = 2.0 * k[2] * x * y + k[3] * (r2 + 2.0 * x * x);
        const double dy = k[2] * (r2 + 2.0 * y * y) + 2.0 * k[3] * x * y;
        if (radial == 0.0) {
            return {xd, yd};
        }
        x = (xd - dx) / radial;
        y = (yd - dy) / radial;
    }
    return {x, y};
}

}  // namespace

CoordinateMapper::CoordinateMapper(const CalibrationArtifacts& artifacts,
                                   const CoordinateConfig& coordinate,
                                   double ball_radius_m)
    : camera_matrix_(artifacts.intrinsics.camera_matrix),
      ball_radius_m_(ball_radius_m),
      pixel_uncertainty_stddev_px_(coordinate.pixel_uncertainty_stddev_px),
      coordinate_(coordinate) {
    dist_coeff_count_ = static_cast<int>(artifacts.intrinsics.dist_coeffs.size());
    for (int i = 0; i < dist_coeff_count_ && i < 8; ++i) {
        dist_coeffs_[static_cast<std::size_t>(i)] =
            artifacts.intrinsics.dist_coeffs[static_cast<std::size_t>(i)];
    }

    // Frobenius-normalise H, sign fixed so the projective denominator is
    // positive at the principal point (the loader already proved single-sign
    // over the working region).
    cv::Matx33d h = artifacts.extrinsics.homography;
    double frob = 0.0;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            frob += h(r, c) * h(r, c);
        }
    }
    h *= 1.0 / std::sqrt(frob);
    const double cx = camera_matrix_(0, 2);
    const double cy = camera_matrix_(1, 2);
    if (h(2, 0) * cx + h(2, 1) * cy + h(2, 2) < 0.0) {
        h *= -1.0;
    }
    homography_ = h;

    // Plane-pose extraction (plan KTD-4): B = K^-1 * H^-1 ∝ [r1 r2 t]. Note
    // cv::decomposeHomographyMat does NOT apply — it models a two-view
    // homography K(R + t n^T / d)K^-1, not a world-plane-to-image mapping.
    const cv::Matx33d b = camera_matrix_.inv() * homography_.inv();
    const cv::Vec3d b1{b(0, 0), b(1, 0), b(2, 0)};
    const cv::Vec3d b2{b(0, 1), b(1, 1), b(2, 1)};
    const cv::Vec3d b3{b(0, 2), b(1, 2), b(2, 2)};
    const double norm_b1 = cv::norm(b1);
    if (norm_b1 <= 0.0) {
        throw CalibrationError("plane-pose extraction failed: zero first column");
    }
    bool found = false;
    for (const double sign : {1.0, -1.0}) {
        const double lambda = sign / norm_b1;
        const cv::Vec3d r1 = lambda * b1;
        const cv::Vec3d r2 = lambda * b2;
        const cv::Vec3d r3 = r1.cross(r2);
        cv::Matx33d rot{r1[0], r2[0], r3[0],  //
                        r1[1], r2[1], r3[1],  //
                        r1[2], r2[2], r3[2]};
        // Orthonormalise: nearest rotation via SVD (R = U V^T, det-corrected).
        cv::Mat w, u, vt;
        cv::SVD::compute(cv::Mat(rot), w, u, vt);
        cv::Mat r_mat = u * vt;
        if (cv::determinant(r_mat) < 0.0) {
            u.col(2) *= -1.0;
            r_mat = u * vt;
        }
        const cv::Matx33d r_ortho(r_mat.ptr<double>());
        const cv::Vec3d t = lambda * b3;
        const cv::Vec3d centre = -(r_ortho.t() * t);
        if (centre[2] > 0.0) {
            camera_centre_ = centre;
            found = true;
            break;
        }
    }
    if (!found) {
        throw CalibrationError(
            "plane-pose extraction failed: no sign choice places the camera above "
            "the floor (C_z > 0)");
    }
}

double CoordinateMapper::expected_height_m(ObjectType type) const noexcept {
    switch (type) {
        case ObjectType::Laser:
            return 0.0;  // the dot lies on the floor (ADR-010)
        case ObjectType::Ball:
            return ball_radius_m_;  // centroid one radius above the floor
        case ObjectType::CalibrationMarker:
            return 0.0;
    }
    return 0.0;
}

cv::Point2d CoordinateMapper::undistort(const cv::Point2f& pixel) const {
    const double fx = camera_matrix_(0, 0);
    const double fy = camera_matrix_(1, 1);
    const double cx = camera_matrix_(0, 2);
    const double cy = camera_matrix_(1, 2);
    const cv::Point2d normalised = undistort_normalised(
        (static_cast<double>(pixel.x) - cx) / fx,
        (static_cast<double>(pixel.y) - cy) / fy, dist_coeffs_);
    return {fx * normalised.x + cx, fy * normalised.y + cy};
}

FloorPosition CoordinateMapper::project_to_floor(const cv::Point2f& pixel,
                                                 ObjectType type) const {
    FloorPosition result;  // valid=false until every gate passes (plan R8)

    const cv::Point2d p = undistort(pixel);
    const cv::Matx33d& h = homography_;
    const double w = h(2, 0) * p.x + h(2, 1) * p.y + h(2, 2);
    if (!(std::abs(w) > kMinProjectiveDenominator)) {
        return result;  // near-horizon pixel
    }
    const double a = h(0, 0) * p.x + h(0, 1) * p.y + h(0, 2);
    const double b = h(1, 0) * p.x + h(1, 1) * p.y + h(1, 2);
    const double x0 = a / w;
    const double y0 = b / w;

    const double height = expected_height_m(type);
    const double clearance = camera_centre_[2] - height;
    if (!(clearance > kMinHeightClearanceM)) {
        return result;  // object plane at or above the camera
    }
    const double scale = clearance / camera_centre_[2];
    if (!(scale > 0.0)) {
        return result;
    }
    const double x = camera_centre_[0] + scale * (x0 - camera_centre_[0]);
    const double y = camera_centre_[1] + scale * (y0 - camera_centre_[1]);
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return result;
    }
    if (x < coordinate_.floor_aoi_x_min_m || x > coordinate_.floor_aoi_x_max_m ||
        y < coordinate_.floor_aoi_y_min_m || y > coordinate_.floor_aoi_y_max_m) {
        return result;  // outside the working area
    }

    // TRK-019: analytic Jacobian of the projective map at p, scaled by the
    // (constant) height factor; uncertainty is its spectral norm times the
    // configured pixel stddev.
    const double w2 = w * w;
    const double j11 = (h(0, 0) * w - a * h(2, 0)) / w2;
    const double j12 = (h(0, 1) * w - a * h(2, 1)) / w2;
    const double j21 = (h(1, 0) * w - b * h(2, 0)) / w2;
    const double j22 = (h(1, 1) * w - b * h(2, 1)) / w2;
    const double m11 = j11 * j11 + j21 * j21;
    const double m12 = j11 * j12 + j21 * j22;
    const double m22 = j12 * j12 + j22 * j22;
    const double half_trace = 0.5 * (m11 + m22);
    const double half_diff = 0.5 * (m11 - m22);
    const double lambda_max =
        half_trace + std::sqrt(half_diff * half_diff + m12 * m12);
    const double sigma = scale * std::sqrt(lambda_max > 0.0 ? lambda_max : 0.0);
    const double uncertainty = sigma * pixel_uncertainty_stddev_px_;
    if (!std::isfinite(uncertainty)) {
        return result;
    }

    result.x_m = x;
    result.y_m = y;
    result.z_m = height;
    result.uncertainty_m = uncertainty;
    result.valid = true;
    return result;
}

}  // namespace tracking
