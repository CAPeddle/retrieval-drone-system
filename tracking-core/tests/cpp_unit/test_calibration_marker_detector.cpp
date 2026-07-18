// Unit tests for the TRK-011 calibration marker detector. Synthetic ArUco
// markers generated in-test; the generation call is version-seamed to match
// the detector (KTD-2).
#include "calibration_marker_detector.hpp"

#include <gtest/gtest.h>
#include <opencv2/core/version.hpp>
#include <opencv2/imgproc.hpp>

#if CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR < 7
#include <opencv2/aruco.hpp>
#else
#include <opencv2/objdetect/aruco_detector.hpp>
#endif

#include <algorithm>
#include <string>
#include <vector>

namespace {

constexpr int kRows = 480;
constexpr int kCols = 640;

// A single marker image (white background, black marker) of `side` px.
cv::Mat generate_marker(const std::string& dict_name, int id, int side) {
    const int dict_id = tracking::aruco_dictionary_id(dict_name);
    cv::Mat marker;
#if CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR < 7
    cv::Ptr<cv::aruco::Dictionary> dict = cv::aruco::getPredefinedDictionary(dict_id);
    dict->drawMarker(id, side, marker, 1);
#else
    cv::aruco::Dictionary dict = cv::aruco::getPredefinedDictionary(dict_id);
    cv::aruco::generateImageMarker(dict, id, side, marker, 1);
#endif
    return marker;
}

// Pastes `marker` onto a white kRows x kCols frame at top-left (x, y).
cv::Mat frame_with_marker(const cv::Mat& marker, int x, int y) {
    cv::Mat frame(kRows, kCols, CV_8UC1, cv::Scalar(255));
    marker.copyTo(frame(cv::Rect(x, y, marker.cols, marker.rows)));
    return frame;
}

tracking::CalibrationConfig config_for(const std::string& dict) {
    tracking::CalibrationConfig config;
    config.aruco_dictionary = dict;
    config.marker_ids = {0};  // unused by detect(); present for construction realism
    return config;
}

TEST(CalibrationMarkerDetectorTest, DetectsSingleMarkerWithCorrectId) {
    tracking::CalibrationMarkerDetector detector(config_for("4X4_50"));
    const cv::Mat frame = frame_with_marker(generate_marker("4X4_50", 7, 200), 220, 140);
    const std::vector<tracking::MarkerObservation> obs = detector.detect(frame);
    ASSERT_EQ(obs.size(), 1u);
    EXPECT_EQ(obs[0].marker_id, 7);
    // Centroid near the pasted marker's centre (220 + 100, 140 + 100).
    EXPECT_NEAR(obs[0].centroid_px.x, 320, 5.0);
    EXPECT_NEAR(obs[0].centroid_px.y, 240, 5.0);
}

TEST(CalibrationMarkerDetectorTest, NoMarkersReturnsEmpty) {
    tracking::CalibrationMarkerDetector detector(config_for("4X4_50"));
    const cv::Mat blank(kRows, kCols, CV_8UC1, cv::Scalar(255));
    EXPECT_TRUE(detector.detect(blank).empty());
}

TEST(CalibrationMarkerDetectorTest, DetectsMultipleMarkers) {
    tracking::CalibrationMarkerDetector detector(config_for("4X4_50"));
    cv::Mat frame(kRows, kCols, CV_8UC1, cv::Scalar(255));
    generate_marker("4X4_50", 3, 120).copyTo(frame(cv::Rect(60, 180, 120, 120)));
    generate_marker("4X4_50", 9, 120).copyTo(frame(cv::Rect(460, 180, 120, 120)));
    std::vector<tracking::MarkerObservation> obs = detector.detect(frame);
    ASSERT_EQ(obs.size(), 2u);
    std::vector<int> ids{obs[0].marker_id, obs[1].marker_id};
    EXPECT_NE(std::find(ids.begin(), ids.end(), 3), ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), 9), ids.end());
}

TEST(CalibrationMarkerDetectorTest, RotatedMarkerStillDetected) {
    tracking::CalibrationMarkerDetector detector(config_for("4X4_50"));
    const cv::Mat base = frame_with_marker(generate_marker("4X4_50", 12, 200), 220, 140);
    cv::Mat rotated;
    const cv::Mat rot = cv::getRotationMatrix2D(cv::Point2f(320, 240), 30.0, 1.0);
    cv::warpAffine(base, rotated, rot, base.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT,
                   cv::Scalar(255));
    const std::vector<tracking::MarkerObservation> obs = detector.detect(rotated);
    ASSERT_EQ(obs.size(), 1u);
    EXPECT_EQ(obs[0].marker_id, 12);
}

TEST(CalibrationMarkerDetectorTest, DictionaryMismatchReturnsEmpty) {
    // Detector on 4X4_50, marker from 5X5_50 → no detection.
    tracking::CalibrationMarkerDetector detector(config_for("4X4_50"));
    const cv::Mat frame = frame_with_marker(generate_marker("5X5_50", 5, 200), 220, 140);
    EXPECT_TRUE(detector.detect(frame).empty());
}

TEST(CalibrationMarkerDetectorTest, UnknownDictionaryThrows) {
    tracking::CalibrationConfig config = config_for("BOGUS_99");
    EXPECT_THROW(tracking::CalibrationMarkerDetector{config}, tracking::ConfigError);
}

}  // namespace
