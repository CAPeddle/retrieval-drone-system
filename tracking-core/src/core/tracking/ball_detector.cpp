#include "ball_detector.hpp"

#include <opencv2/imgproc.hpp>

#include <cmath>

namespace tracking {

namespace {

constexpr std::size_t kContourReserve = 64;  // typical candidate count per frame

double circularity_of(double area, double perimeter) {
    if (perimeter <= 0.0) {
        return 0.0;
    }
    return 4.0 * CV_PI * area / (perimeter * perimeter);
}

}  // namespace

BallDetector::BallDetector(const BallConfig& config, int rows, int cols)
    : config_(config) {
    gray_.create(rows, cols, CV_8UC1);
    blurred_.create(rows, cols, CV_8UC1);
    binary_.create(rows, cols, CV_8UC1);
    morph_kernel_ = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    contours_.reserve(kContourReserve);
    hull_.reserve(kContourReserve);
}

std::optional<BallObservation> BallDetector::detect(const cv::Mat& frame) {
    const cv::Mat* gray = &frame;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray_, cv::COLOR_BGR2GRAY);
        gray = &gray_;
    }

    const int k = config_.detection_blur_kernel;
    cv::GaussianBlur(*gray, blurred_, cv::Size(k, k), 0);
    // Fixed brightness prior (not Otsu): a scene with nothing bright yields an
    // empty binary image, which is what the zero-false-positive replay gate
    // needs — Otsu would always segment something.
    cv::threshold(blurred_, binary_, config_.brightness_threshold, 255, cv::THRESH_BINARY);
    cv::morphologyEx(binary_, binary_, cv::MORPH_CLOSE, morph_kernel_);

    contours_.clear();
    cv::findContours(binary_, contours_, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    const double area_min =
        CV_PI * static_cast<double>(config_.expected_radius_px_min) *
        static_cast<double>(config_.expected_radius_px_min);
    const double area_max =
        CV_PI * static_cast<double>(config_.expected_radius_px_max) *
        static_cast<double>(config_.expected_radius_px_max);

    std::optional<BallObservation> best;
    double best_score = 0.0;
    for (const std::vector<cv::Point>& contour : contours_) {
        const double area = cv::contourArea(contour);
        if (area < area_min || area > area_max) {
            continue;
        }
        const double perimeter = cv::arcLength(contour, /*closed=*/true);
        const double circularity = circularity_of(area, perimeter);
        if (circularity < config_.min_circularity) {
            continue;
        }
        // Convexity: a ball's contour is near-convex. Reject shapes whose hull
        // area is much larger than the contour area (concavities).
        cv::convexHull(contour, hull_);
        const double hull_area = cv::contourArea(hull_);
        if (hull_area > 0.0 && area / hull_area < 0.85) {
            continue;
        }
        const double score = circularity * area;
        if (score <= best_score) {
            continue;
        }
        const cv::Moments m = cv::moments(contour);
        if (m.m00 <= 0.0) {
            continue;
        }
        BallObservation obs;
        obs.centroid_px = cv::Point2f(static_cast<float>(m.m10 / m.m00),
                                      static_cast<float>(m.m01 / m.m00));
        obs.radius_px = std::sqrt(area / CV_PI);
        obs.circularity = circularity;
        obs.confidence = circularity;  // provisional (KTD-6); TRK-015 may redefine
        best = obs;
        best_score = score;
    }
    return best;
}

}  // namespace tracking
