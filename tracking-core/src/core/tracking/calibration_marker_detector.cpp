#include "calibration_marker_detector.hpp"

#include <opencv2/imgproc.hpp>

#include <cmath>

namespace tracking {

int aruco_dictionary_id(const std::string& name) {
    if (name == "4X4_50") return cv::aruco::DICT_4X4_50;
    if (name == "4X4_100") return cv::aruco::DICT_4X4_100;
    if (name == "5X5_50") return cv::aruco::DICT_5X5_50;
    throw ConfigError("unknown ArUco dictionary: " + name);
}

namespace {

// cornerSubPix window; small, fixed — refinement is per-corner and cheap.
const cv::Size kSubPixWin(5, 5);
const cv::Size kSubPixZeroZone(-1, -1);
const cv::TermCriteria kSubPixCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 30,
                                       0.01);

}  // namespace

// Shared across both ArUco API versions: refine each detected marker's corners,
// compute its centroid, and report the mean corner-refinement shift.
std::vector<MarkerObservation> CalibrationMarkerDetector::build_observations(
    const cv::Mat& gray) const {
    std::vector<MarkerObservation> observations;
    observations.reserve(ids_.size());
    for (std::size_t i = 0; i < ids_.size(); ++i) {
        std::vector<cv::Point2f> refined = corners_[i];
        std::array<cv::Point2f, 4> before;
        for (int c = 0; c < 4; ++c) {
            before[c] = refined[c];
        }
        cv::cornerSubPix(gray, refined, kSubPixWin, kSubPixZeroZone, kSubPixCriteria);
        MarkerObservation obs;
        obs.marker_id = ids_[i];
        cv::Point2f sum(0.0f, 0.0f);
        double shift_sum = 0.0;
        for (int c = 0; c < 4; ++c) {
            obs.corners_px[c] = refined[c];
            sum += refined[c];
            shift_sum += std::hypot(refined[c].x - before[c].x, refined[c].y - before[c].y);
        }
        obs.centroid_px = sum / 4.0f;
        obs.corner_residual_px = shift_sum / 4.0;  // mean over the 4 corners
        observations.push_back(obs);
    }
    return observations;
}

#if CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR < 7

CalibrationMarkerDetector::CalibrationMarkerDetector(const CalibrationConfig& config)
    : dictionary_(cv::aruco::getPredefinedDictionary(
          aruco_dictionary_id(config.aruco_dictionary))),
      params_(cv::aruco::DetectorParameters::create()) {}

std::vector<MarkerObservation> CalibrationMarkerDetector::detect(const cv::Mat& frame) {
    const cv::Mat* gray = &frame;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray_, cv::COLOR_BGR2GRAY);
        gray = &gray_;
    }
    ids_.clear();
    corners_.clear();
    cv::aruco::detectMarkers(*gray, dictionary_, corners_, ids_, params_);
    return build_observations(*gray);
}

#else

CalibrationMarkerDetector::CalibrationMarkerDetector(const CalibrationConfig& config)
    : dictionary_(cv::aruco::getPredefinedDictionary(
          aruco_dictionary_id(config.aruco_dictionary))),
      detector_(dictionary_, cv::aruco::DetectorParameters()) {}

std::vector<MarkerObservation> CalibrationMarkerDetector::detect(const cv::Mat& frame) {
    const cv::Mat* gray = &frame;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray_, cv::COLOR_BGR2GRAY);
        gray = &gray_;
    }
    ids_.clear();
    corners_.clear();
    detector_.detectMarkers(*gray, corners_, ids_);
    return build_observations(*gray);
}

#endif

}  // namespace tracking
