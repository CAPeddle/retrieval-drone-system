#pragma once

// TRK-011: ArUco marker detector for ADR-004 Phase 2 calibration-health
// monitoring — detects the static markers whose IDs are configured, per frame,
// with sub-pixel corner refinement. Individual-marker detection only; full
// Charuco-board detection for install-time calibration lives in the Python
// tools (KTD-2/KTD-3), not here, because the C++ runtime consumer only needs
// individual markers.
//
// OpenCV API version seam (KTD-2): 4.6 (dev box) exposes only the free-function
// API (cv::aruco::detectMarkers + Ptr<Dictionary>/DetectorParameters::create),
// 4.10 (Pi 5) exposes the class API (cv::aruco::ArucoDetector). The includes are
// version-guarded: core/version.hpp first so the guard macros exist, then the
// old branch pulls opencv2/aruco.hpp (contrib) and the new branch
// opencv2/objdetect/aruco_detector.hpp. The public surface below is identical on
// both.

#include "config.hpp"

#include <opencv2/core.hpp>
#include <opencv2/core/version.hpp>

#if CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR < 7
#include <opencv2/aruco.hpp>
#else
#include <opencv2/objdetect/aruco_detector.hpp>
#endif

#include <array>
#include <vector>

namespace tracking {

struct MarkerObservation {
    int marker_id = 0;
    std::array<cv::Point2f, 4> corners_px{};  // refined corner order per ArUco
    cv::Point2f centroid_px{0.0f, 0.0f};      // mean of the 4 corners
    // Mean cv::cornerSubPix refinement shift, in pixels. NOTE: this is a
    // refinement-magnitude proxy, not a geometric reprojection residual — the
    // ticket's "reprojection_quality" name is renamed to avoid promising
    // reprojection information the value does not carry (record for TRK-024).
    double corner_residual_px = 0.0;
};

class CalibrationMarkerDetector {
public:
    // Builds the dictionary + detector from config; startup-only (may allocate).
    // Throws ConfigError on an unrecognised dictionary name.
    explicit CalibrationMarkerDetector(const CalibrationConfig& config);

    // Detects all markers of the configured dictionary in `frame` (grayscale or
    // BGR), returns one observation each with refined corners.
    std::vector<MarkerObservation> detect(const cv::Mat& frame);

private:
#if CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR < 7
    cv::Ptr<cv::aruco::Dictionary> dictionary_;
    cv::Ptr<cv::aruco::DetectorParameters> params_;
#else
    cv::aruco::Dictionary dictionary_;
    cv::aruco::ArucoDetector detector_;
#endif
    cv::Mat gray_;  // pre-allocated grayscale target for BGR inputs
};

// Maps a config dictionary name ("4X4_50", ...) to the OpenCV predefined
// dictionary enum. Throws ConfigError on an unknown name. Exposed so tests can
// generate markers from the same dictionary the detector uses.
int aruco_dictionary_id(const std::string& name);

}  // namespace tracking
