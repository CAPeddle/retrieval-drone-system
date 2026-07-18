// TRK-018/019: Z compensation and uncertainty propagation tests on the
// ADR-010 worked-example geometry (camera 1 m up, 45 degrees tilt).

#include "coordinate_mapper.hpp"

#include <gtest/gtest.h>

#include <cmath>

#include "calibration_fixture.hpp"

namespace {

namespace fx = calibration_fixture;

using tracking::CalibrationArtifacts;
using tracking::CoordinateMapper;
using tracking::FloorPosition;
using tracking::ObjectType;

// Builds artifacts directly from ground-truth geometry (the loader has its own
// tests; the mapper consumes the parsed structs).
CalibrationArtifacts make_artifacts(const cv::Matx33d& k, const cv::Matx33d& pixel_to_floor) {
    CalibrationArtifacts artifacts;
    artifacts.intrinsics.camera_matrix = k;
    artifacts.intrinsics.dist_coeffs = {0.0, 0.0, 0.0, 0.0, 0.0};
    artifacts.intrinsics.image_width = 640;
    artifacts.intrinsics.image_height = 480;
    artifacts.extrinsics.homography = pixel_to_floor;
    return artifacts;
}

CalibrationArtifacts fixture_artifacts() {
    return make_artifacts(fx::camera_matrix(), fx::pixel_to_floor());
}

// Alternative geometry for the tilt-dependence test: camera at `centre`
// looking at floor point `target` with the same K.
cv::Matx33d pixel_to_floor_for(const cv::Vec3d& centre, const cv::Vec3d& target) {
    cv::Vec3d z = target - centre;
    z /= cv::norm(z);
    cv::Vec3d x = z.cross(cv::Vec3d{0.0, 0.0, 1.0});
    x /= cv::norm(x);
    const cv::Vec3d y = z.cross(x);
    const cv::Matx33d r{x[0], x[1], x[2],  //
                        y[0], y[1], y[2],  //
                        z[0], z[1], z[2]};
    const cv::Vec3d t = -(r * centre);
    const cv::Matx33d cols{r(0, 0), r(0, 1), t[0],  //
                           r(1, 0), r(1, 1), t[1],  //
                           r(2, 0), r(2, 1), t[2]};
    return (fx::camera_matrix() * cols).inv();
}

CoordinateMapper make_mapper(double ball_radius_m = 0.03) {
    return {fixture_artifacts(), fx::coordinate_config(), ball_radius_m};
}

TEST(CoordinateMapperTest, RecoversCameraCentreFromHomography) {
    const CoordinateMapper mapper = make_mapper();
    const cv::Vec3d c = mapper.camera_centre();
    EXPECT_NEAR(c[0], 0.0, 1e-6);
    EXPECT_NEAR(c[1], 0.0, 1e-6);
    EXPECT_NEAR(c[2], 1.0, 1e-6);  // a sign/orthonormalisation slip fails here
}

TEST(CoordinateMapperTest, Adr010WorkedExampleBiasIsCorrected) {
    // Ball resting on the floor at (0, 2): centroid at Z = radius = 0.03 m.
    const cv::Vec3d ball_centre{0.0, 2.0, 0.03};
    const cv::Point2d px = fx::pixel_of_world(ball_centre);

    // Naive H-only projection (assumes Z=0): a systematic forward bias well
    // above the 2 cm alignment tolerance for this geometry.
    const cv::Point2d naive =
        fx::floor_of_pixel(fx::pixel_to_floor(), px.x, px.y);
    const double naive_bias = std::hypot(naive.x - 0.0, naive.y - 2.0);
    EXPECT_GT(naive_bias, 0.02);

    // Z-compensated projection recovers the true floor position (plan R6).
    const CoordinateMapper mapper = make_mapper();
    const FloorPosition pos = mapper.project_to_floor(
        {static_cast<float>(px.x), static_cast<float>(px.y)}, ObjectType::Ball);
    ASSERT_TRUE(pos.valid);
    EXPECT_NEAR(pos.x_m, 0.0, 0.005);
    EXPECT_NEAR(pos.y_m, 2.0, 0.005);
    EXPECT_DOUBLE_EQ(pos.z_m, 0.03);
}

TEST(CoordinateMapperTest, LaserProjectionMatchesPlainHomography) {
    // Z = 0 objects need no compensation: mapper output equals H projection.
    const cv::Vec3d laser{0.3, 1.5, 0.0};
    const cv::Point2d px = fx::pixel_of_world(laser);
    const CoordinateMapper mapper = make_mapper();
    const FloorPosition pos = mapper.project_to_floor(
        {static_cast<float>(px.x), static_cast<float>(px.y)}, ObjectType::Laser);
    ASSERT_TRUE(pos.valid);
    EXPECT_NEAR(pos.x_m, 0.3, 1e-3);
    EXPECT_NEAR(pos.y_m, 1.5, 1e-3);
    EXPECT_DOUBLE_EQ(pos.z_m, 0.0);
}

TEST(CoordinateMapperTest, ObjectPlaneAtCameraHeightIsInvalidNotNaN) {
    // expected_height >= C_z: the height scale degenerates (plan R8).
    const CoordinateMapper mapper = make_mapper(/*ball_radius_m=*/1.5);
    const cv::Point2d px = fx::pixel_of_world({0.0, 2.0, 0.0});
    const FloorPosition pos = mapper.project_to_floor(
        {static_cast<float>(px.x), static_cast<float>(px.y)}, ObjectType::Ball);
    EXPECT_FALSE(pos.valid);
    EXPECT_TRUE(std::isfinite(pos.x_m));  // safe defaults, never NaN
}

TEST(CoordinateMapperTest, OutsideAoiIsInvalid) {
    const CoordinateMapper mapper = make_mapper();
    const cv::Point2d px = fx::pixel_of_world({0.0, 5.0, 0.0});  // AOI y_max is 3.5
    const FloorPosition pos = mapper.project_to_floor(
        {static_cast<float>(px.x), static_cast<float>(px.y)}, ObjectType::Laser);
    EXPECT_FALSE(pos.valid);
}

TEST(CoordinateMapperTest, ExtremePixelsNeverProduceNaN) {
    const CoordinateMapper mapper = make_mapper();
    const float extremes[] = {-1.0e6F, -1000.0F, 0.0F, 320.0F, 1000.0F, 1.0e6F};
    for (const float u : extremes) {
        for (const float v : extremes) {
            const FloorPosition pos = mapper.project_to_floor({u, v}, ObjectType::Ball);
            EXPECT_TRUE(std::isfinite(pos.x_m)) << u << "," << v;
            EXPECT_TRUE(std::isfinite(pos.y_m)) << u << "," << v;
            EXPECT_TRUE(std::isfinite(pos.uncertainty_m)) << u << "," << v;
        }
    }
}

// ── TRK-019 uncertainty propagation ──────────────────────────────────────────

TEST(UncertaintyTest, FarObservationHasLargerUncertaintyThanNear) {
    const CoordinateMapper mapper = make_mapper();
    const cv::Point2d near_px = fx::pixel_of_world({0.0, 1.0, 0.0});
    const cv::Point2d far_px = fx::pixel_of_world({0.0, 3.0, 0.0});
    const FloorPosition near_pos = mapper.project_to_floor(
        {static_cast<float>(near_px.x), static_cast<float>(near_px.y)}, ObjectType::Laser);
    const FloorPosition far_pos = mapper.project_to_floor(
        {static_cast<float>(far_px.x), static_cast<float>(far_px.y)}, ObjectType::Laser);
    ASSERT_TRUE(near_pos.valid);
    ASSERT_TRUE(far_pos.valid);
    EXPECT_GT(far_pos.uncertainty_m, near_pos.uncertainty_m);
    EXPECT_GT(near_pos.uncertainty_m, 0.0);
}

TEST(UncertaintyTest, AnalyticMatchesNumericJacobian) {
    // Numeric cross-check (plan U7): finite-difference the projection and
    // compare spectral norms within 10%.
    const CoordinateMapper mapper = make_mapper();
    const cv::Point2d px = fx::pixel_of_world({0.2, 2.0, 0.0});
    const FloorPosition pos = mapper.project_to_floor(
        {static_cast<float>(px.x), static_cast<float>(px.y)}, ObjectType::Laser);
    ASSERT_TRUE(pos.valid);

    const double eps = 0.5;  // half-pixel steps
    const auto at = [&](double u, double v) {
        return mapper.project_to_floor({static_cast<float>(u), static_cast<float>(v)},
                                       ObjectType::Laser);
    };
    const FloorPosition pu = at(px.x + eps, px.y);
    const FloorPosition mu = at(px.x - eps, px.y);
    const FloorPosition pv = at(px.x, px.y + eps);
    const FloorPosition mv = at(px.x, px.y - eps);
    ASSERT_TRUE(pu.valid && mu.valid && pv.valid && mv.valid);
    const double j11 = (pu.x_m - mu.x_m) / (2 * eps);
    const double j21 = (pu.y_m - mu.y_m) / (2 * eps);
    const double j12 = (pv.x_m - mv.x_m) / (2 * eps);
    const double j22 = (pv.y_m - mv.y_m) / (2 * eps);
    const double m11 = j11 * j11 + j21 * j21;
    const double m12 = j11 * j12 + j21 * j22;
    const double m22 = j12 * j12 + j22 * j22;
    const double lambda_max = 0.5 * (m11 + m22) +
                              std::sqrt(0.25 * (m11 - m22) * (m11 - m22) + m12 * m12);
    const double numeric_uncertainty = std::sqrt(lambda_max);  // stddev 1 px

    EXPECT_NEAR(pos.uncertainty_m, numeric_uncertainty, 0.1 * numeric_uncertainty);
}

TEST(UncertaintyTest, ShallowerTiltGivesLargerUncertainty) {
    // Same floor point seen from a shallower ray stretches pixels over more
    // metres. Steep: camera 2 m up. Shallow: fixture camera 1 m up.
    const CalibrationArtifacts steep = make_artifacts(
        fx::camera_matrix(), pixel_to_floor_for({0.0, 0.0, 2.0}, {0.0, 1.0, 0.0}));
    const CoordinateMapper steep_mapper{steep, fx::coordinate_config(), 0.03};
    const CoordinateMapper shallow_mapper = make_mapper();

    // Pixel of the same floor point under each geometry.
    const cv::Vec3d point{0.0, 2.5, 0.0};
    const cv::Point2d shallow_px = fx::pixel_of_world(point);
    const FloorPosition shallow_pos = shallow_mapper.project_to_floor(
        {static_cast<float>(shallow_px.x), static_cast<float>(shallow_px.y)},
        ObjectType::Laser);
    // Project the steep geometry's pixel of the same point through its mapper.
    const cv::Matx33d steep_floor_to_pixel =
        pixel_to_floor_for({0.0, 0.0, 2.0}, {0.0, 1.0, 0.0}).inv();
    const double wp = steep_floor_to_pixel(2, 0) * point[0] +
                      steep_floor_to_pixel(2, 1) * point[1] + steep_floor_to_pixel(2, 2);
    const cv::Point2d steep_px{
        (steep_floor_to_pixel(0, 0) * point[0] + steep_floor_to_pixel(0, 1) * point[1] +
         steep_floor_to_pixel(0, 2)) /
            wp,
        (steep_floor_to_pixel(1, 0) * point[0] + steep_floor_to_pixel(1, 1) * point[1] +
         steep_floor_to_pixel(1, 2)) /
            wp};
    const FloorPosition steep_pos = steep_mapper.project_to_floor(
        {static_cast<float>(steep_px.x), static_cast<float>(steep_px.y)},
        ObjectType::Laser);

    ASSERT_TRUE(shallow_pos.valid);
    ASSERT_TRUE(steep_pos.valid);
    EXPECT_GT(shallow_pos.uncertainty_m, steep_pos.uncertainty_m);
}

}  // namespace
