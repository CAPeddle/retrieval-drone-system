#include "calibration_marker_detector.hpp"

#include <opencv2/imgproc.hpp>

#include <numeric>

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
    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;
    cv::aruco::detectMarkers(*gray, dictionary_, corners, ids, params_);

    std::vector<MarkerObservation> observations;
    observations.reserve(ids.size());
    for (std::size_t i = 0; i < ids.size(); ++i) {
        std::vector<cv::Point2f> refined = corners[i];
        const double before_x = refined[0].x;
        const double before_y = refined[0].y;
        cv::cornerSubPix(*gray, refined, kSubPixWin, kSubPixZeroZone, kSubPixCriteria);
        MarkerObservation obs;
        obs.marker_id = ids[i];
        cv::Point2f sum(0.0f, 0.0f);
        for (int c = 0; c < 4; ++c) {
            obs.corners_px[c] = refined[c];
            sum += refined[c];
        }
        obs.centroid_px = sum / 4.0f;
        obs.corner_residual_px =
            std::hypot(refined[0].x - before_x, refined[0].y - before_y);
        observations.push_back(obs);
    }
    return observations;
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
    std::vector<int> ids;
    std::vector<std::vector<cv::Point2f>> corners;
    detector_.detectMarkers(*gray, corners, ids);

    std::vector<MarkerObservation> observations;
    observations.reserve(ids.size());
    for (std::size_t i = 0; i < ids.size(); ++i) {
        std::vector<cv::Point2f> refined = corners[i];
        const double before_x = refined[0].x;
        const double before_y = refined[0].y;
        cv::cornerSubPix(*gray, refined, kSubPixWin, kSubPixZeroZone, kSubPixCriteria);
        MarkerObservation obs;
        obs.marker_id = ids[i];
        cv::Point2f sum(0.0f, 0.0f);
        for (int c = 0; c < 4; ++c) {
            obs.corners_px[c] = refined[c];
            sum += refined[c];
        }
        obs.centroid_px = sum / 4.0f;
        obs.corner_residual_px =
            std::hypot(refined[0].x - before_x, refined[0].y - before_y);
        observations.push_back(obs);
    }
    return observations;
}

#endif

}  // namespace tracking
