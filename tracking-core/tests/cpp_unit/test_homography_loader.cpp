// TRK-017: calibration artifact loading and validation tests. Each defect
// class throws a CalibrationError with a distinct, actionable message. The
// self-consistency case is the contract test the TRK-013 P1 finding taught us
// to write (artifact-metadata solution doc).

#include "homography_loader.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <string>
#include <vector>

#include "calibration_fixture.hpp"

namespace {

namespace fx = calibration_fixture;

class HomographyLoaderTest : public ::testing::Test {
protected:
    std::string write(const std::string& name, const nlohmann::json& payload) {
        const std::string path = fx::write_json(
            ::testing::TempDir(),
            std::string("trk017_") +
                ::testing::UnitTest::GetInstance()->current_test_info()->name() + "_" + name,
            payload);
        temp_paths_.push_back(path);
        return path;
    }

    void TearDown() override {
        for (const std::string& path : temp_paths_) {
            std::remove(path.c_str());
        }
    }

    // Loads with optional mutations applied to either artifact; returns the
    // error message, or empty string on success.
    std::string load_error(const nlohmann::json& intrinsics,
                           const nlohmann::json& extrinsics) {
        const std::string ipath = write("intrinsics.json", intrinsics);
        const std::string epath = write("extrinsics.json", extrinsics);
        try {
            tracking::load_calibration_artifacts(ipath, epath, fx::coordinate_config());
        } catch (const tracking::CalibrationError& e) {
            return e.what();
        }
        return {};
    }

private:
    std::vector<std::string> temp_paths_;
};

TEST_F(HomographyLoaderTest, ValidFixtureLoads) {
    const std::string ipath = write("intrinsics.json", fx::intrinsics_json());
    const std::string epath = write("extrinsics.json", fx::extrinsics_json());
    const tracking::CalibrationArtifacts artifacts =
        tracking::load_calibration_artifacts(ipath, epath, fx::coordinate_config());
    EXPECT_EQ(artifacts.intrinsics.camera_id, "fixture-cam");
    EXPECT_EQ(artifacts.extrinsics.anchors.size(), 5U);
    EXPECT_DOUBLE_EQ(artifacts.intrinsics.camera_matrix(0, 0), 600.0);
}

TEST_F(HomographyLoaderTest, MissingFileThrows) {
    const std::string epath = write("extrinsics.json", fx::extrinsics_json());
    try {
        tracking::load_calibration_artifacts("/nonexistent/intrinsics.json", epath,
                                             fx::coordinate_config());
        FAIL() << "expected CalibrationError";
    } catch (const tracking::CalibrationError& e) {
        EXPECT_NE(std::string(e.what()).find("missing"), std::string::npos);
    }
}

TEST_F(HomographyLoaderTest, MalformedJsonThrows) {
    const std::string ipath = write("intrinsics.json", fx::intrinsics_json());
    const std::string bad = ::testing::TempDir() + "trk017_malformed.json";
    std::ofstream(bad) << "{ not json";
    try {
        tracking::load_calibration_artifacts(ipath, bad, fx::coordinate_config());
        FAIL() << "expected CalibrationError";
    } catch (const tracking::CalibrationError& e) {
        EXPECT_NE(std::string(e.what()).find("malformed"), std::string::npos);
    }
    std::remove(bad.c_str());
}

TEST_F(HomographyLoaderTest, WrongVersionThrows) {
    nlohmann::json extrinsics = fx::extrinsics_json();
    extrinsics["extrinsics_version"] = 2;
    const std::string err = load_error(fx::intrinsics_json(), extrinsics);
    EXPECT_NE(err.find("unsupported extrinsics_version"), std::string::npos) << err;

    nlohmann::json intrinsics = fx::intrinsics_json();
    intrinsics["intrinsics_version"] = 0;
    const std::string err2 = load_error(intrinsics, fx::extrinsics_json());
    EXPECT_NE(err2.find("unsupported intrinsics_version"), std::string::npos) << err2;
}

TEST_F(HomographyLoaderTest, NearSingularHomographyThrows) {
    nlohmann::json extrinsics = fx::extrinsics_json();
    // Rank-deficient H: second row a multiple of the first.
    extrinsics["homography_3x3"] = {{1.0, 2.0, 3.0}, {2.0, 4.0, 6.0}, {0.0, 0.0, 1.0}};
    const std::string err = load_error(fx::intrinsics_json(), extrinsics);
    EXPECT_NE(err.find("near-singular"), std::string::npos) << err;
}

TEST_F(HomographyLoaderTest, SelfConsistencyViolationThrows) {
    // The stored homography no longer reproduces the stored anchors: shift one
    // anchor's floor coordinate by 5 cm — far beyond the stored 1e-6 m error.
    nlohmann::json extrinsics = fx::extrinsics_json();
    extrinsics["floor_anchor_points"][2]["floor_x_m"] =
        extrinsics["floor_anchor_points"][2]["floor_x_m"].get<double>() + 0.05;
    const std::string err = load_error(fx::intrinsics_json(), extrinsics);
    EXPECT_NE(err.find("not self-consistent"), std::string::npos) << err;
}

TEST_F(HomographyLoaderTest, DistortedFitArtifactThrows) {
    nlohmann::json extrinsics = fx::extrinsics_json();
    extrinsics["undistorted_coordinates"] = false;
    const std::string err = load_error(fx::intrinsics_json(), extrinsics);
    EXPECT_NE(err.find("--intrinsics"), std::string::npos) << err;  // recovery action named
}

TEST_F(HomographyLoaderTest, CameraIdMismatchThrows) {
    nlohmann::json intrinsics = fx::intrinsics_json();
    intrinsics["camera_id"] = "other-cam";
    const std::string err = load_error(intrinsics, fx::extrinsics_json());
    EXPECT_NE(err.find("camera_id"), std::string::npos) << err;
}

TEST_F(HomographyLoaderTest, ExtrinsicsOlderThanIntrinsicsThrows) {
    // Focus-drift scenario (live risk R-05): intrinsics re-calibrated after
    // the extrinsics fit. Every per-file check still passes; only the pairing
    // check catches it.
    nlohmann::json intrinsics = fx::intrinsics_json();
    intrinsics["calibration_timestamp"] = "2026-07-03T10:00:00+00:00";  // after extrinsics
    const std::string err = load_error(intrinsics, fx::extrinsics_json());
    EXPECT_NE(err.find("predates"), std::string::npos) << err;
}

TEST_F(HomographyLoaderTest, AoiReachingBehindCameraThrows) {
    // Floor behind the camera flips the projective denominator: an AOI that
    // reaches there must fail the horizon-safety check loudly at startup.
    const std::string ipath = write("intrinsics.json", fx::intrinsics_json());
    const std::string epath = write("extrinsics.json", fx::extrinsics_json());
    tracking::CoordinateConfig cfg = fx::coordinate_config();
    cfg.floor_aoi_y_min_m = -2.0;  // behind the camera (camera at y=0 looking +y)
    try {
        tracking::load_calibration_artifacts(ipath, epath, cfg);
        FAIL() << "expected CalibrationError";
    } catch (const tracking::CalibrationError& e) {
        EXPECT_NE(std::string(e.what()).find("denominator"), std::string::npos);
    }
}

TEST_F(HomographyLoaderTest, TooFewAnchorsThrows) {
    nlohmann::json extrinsics = fx::extrinsics_json();
    nlohmann::json anchors = extrinsics["floor_anchor_points"];
    extrinsics["floor_anchor_points"] = {anchors[0], anchors[1], anchors[2]};
    const std::string err = load_error(fx::intrinsics_json(), extrinsics);
    EXPECT_NE(err.find("fewer than 4"), std::string::npos) << err;
}

}  // namespace
