#include "modulation_detector.hpp"

#include <opencv2/imgproc.hpp>

#include <cmath>

namespace tracking {

namespace {

// Small floor on per-pixel AC power so the purity ratio never divides by ~0 on a
// flat region. A flat region has near-zero bin power too, so it fails the power
// gate regardless — the floor only prevents inf/NaN, it does not admit anything.
constexpr double kAcFloor = 1e-3;

}  // namespace

ModulationDetector::ModulationDetector(const LaserConfig& config, int target_fps,
                                       int rows, int cols)
    : config_(config) {
    // Window and cadence derive from the capture rate and modulation frequency;
    // config load has already validated the ratio is integral and >= 8 (KTD-1),
    // so the rounding here is exact.
    window_ = static_cast<int>(std::lround(2.0 * target_fps / config.modulation_frequency_hz));
    frames_per_cycle_ =
        static_cast<int>(std::lround(target_fps / config.modulation_frequency_hz));
    detection_start_ = std::max(config.grace_period_cycles * frames_per_cycle_, window_);
    gap_ceiling_ns_ =
        static_cast<std::int64_t>(1.5 * 1.0e9 / static_cast<double>(target_fps));
    // Normalised two-sided purity is 2*|X2|^2 / (N * AC). Comparing the raw ratio
    // |X2|^2 / AC against psd_purity_min * N / 2 is the same test without scaling
    // the whole purity map each frame.
    purity_ratio_threshold_ = config.psd_purity_min * window_ / 2.0;

    // Modulation sits at bin 2 of the window (two periods by construction), so
    // the per-frame rotation is exp(-j * 2pi * 2 * n / N). At N=8 this is the
    // four-phase set {1, -j, -1, j, ...}; computing the general coefficients keeps
    // the code correct for any validated N without a complex library.
    coef_cos_.resize(window_);
    coef_negsin_.resize(window_);
    for (int n = 0; n < window_; ++n) {
        const double theta = 2.0 * CV_PI * 2.0 * n / window_;
        coef_cos_[n] = std::cos(theta);
        coef_negsin_[n] = -std::sin(theta);
    }

    ring_.resize(window_);
    for (cv::Mat& slot : ring_) {
        slot.create(rows, cols, CV_32F);
    }
    gray_.create(rows, cols, CV_8UC1);
    for (cv::Mat* m : {&re_, &im_, &power_, &sum_, &sumsq_, &ac_, &ratio_, &sq_}) {
        m->create(rows, cols, CV_32F);
    }
    for (cv::Mat* m : {&mask_, &mask_power_, &mask_purity_}) {
        m->create(rows, cols, CV_8UC1);
    }
    morph_kernel_ = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
}

void ModulationDetector::flush() {
    fill_ = 0;
    write_index_ = 0;
    have_last_ = false;
    ++flushes_;
}

void ModulationDetector::push_frame(const cv::Mat& frame, const FrameMetadata& metadata) {
    // Window integrity (KTD-3): the window is only valid over contiguous admitted
    // frames with no oversized inter-frame capture gap. A break flushes so a
    // window can never span a discontinuity.
    if (have_last_) {
        const bool contiguous =
            metadata.sequence_number == last_sequence_ + 1 &&
            metadata.capture_timestamp_ns >= last_timestamp_ns_ &&
            (metadata.capture_timestamp_ns - last_timestamp_ns_) <= gap_ceiling_ns_;
        if (!contiguous) {
            flush();
        }
    }

    const cv::Mat* src = &frame;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray_, cv::COLOR_BGR2GRAY);
        src = &gray_;
    }
    src->convertTo(ring_[write_index_], CV_32F);
    write_index_ = (write_index_ + 1) % window_;
    if (fill_ < detection_start_) {
        ++fill_;  // saturates at detection_start_; only the >= comparisons matter
    }
    have_last_ = true;
    last_sequence_ = metadata.sequence_number;
    last_timestamp_ns_ = metadata.capture_timestamp_ns;
}

std::optional<LaserObservation> ModulationDetector::detect() {
    if (fill_ < detection_start_) {
        return std::nullopt;  // window not full / grace not complete (KTD-7)
    }
    return correlate_and_extract();
}

std::optional<LaserObservation> ModulationDetector::correlate_and_extract() {
    // Whole-Mat four-phase accumulation over the window in temporal order
    // (oldest..newest). write_index_ points at the oldest slot once full.
    re_.setTo(0);
    im_.setTo(0);
    sum_.setTo(0);
    sumsq_.setTo(0);
    for (int n = 0; n < window_; ++n) {
        const cv::Mat& f = ring_[(write_index_ + n) % window_];
        if (coef_cos_[n] != 0.0) {
            cv::scaleAdd(f, coef_cos_[n], re_, re_);
        }
        if (coef_negsin_[n] != 0.0) {
            cv::scaleAdd(f, coef_negsin_[n], im_, im_);
        }
        cv::add(sum_, f, sum_);
        cv::multiply(f, f, sq_);
        cv::add(sumsq_, sq_, sumsq_);
    }

    // power = |X2|^2 = re^2 + im^2.
    cv::multiply(re_, re_, power_);
    cv::multiply(im_, im_, sq_);
    cv::add(power_, sq_, power_);

    // AC power = sum(x^2) - sum(x)^2 / N; floored so the purity ratio stays finite.
    cv::multiply(sum_, sum_, sq_);
    cv::addWeighted(sumsq_, 1.0, sq_, -1.0 / window_, 0.0, ac_);
    cv::max(ac_, kAcFloor, ac_);
    cv::divide(power_, ac_, ratio_);  // raw purity ratio = |X2|^2 / AC

    // KTD-2 dual gate: power must EXCEED the floor AND the raw purity ratio must
    // exceed purity_min*N/2 (R2 "exceeds" — a pixel exactly at a floor fails, the
    // conservative direction for a safety detector).
    cv::compare(power_, config_.psd_power_min, mask_power_, cv::CMP_GT);
    cv::compare(ratio_, purity_ratio_threshold_, mask_purity_, cv::CMP_GT);
    cv::bitwise_and(mask_power_, mask_purity_, mask_);
    cv::morphologyEx(mask_, mask_, cv::MORPH_CLOSE, morph_kernel_);

    const int ncomp =
        cv::connectedComponentsWithStats(mask_, labels_, stats_, centroids_, 8, CV_32S);

    // Area-filter survivors. Fail closed on more than one (KTD-6): a modulated
    // specular ghost is indistinguishable by PSD (R-04), so ambiguity is unsafe.
    int survivor = -1;
    int survivor_count = 0;
    for (int label = 1; label < ncomp; ++label) {
        const int area = stats_.at<int>(label, cv::CC_STAT_AREA);
        if (area < config_.min_cluster_size_px || area > config_.max_cluster_size_px) {
            continue;
        }
        ++survivor_count;
        survivor = label;
    }
    if (survivor_count == 0) {
        return std::nullopt;
    }
    if (survivor_count > 1) {
        ++ambiguous_;
        return std::nullopt;
    }

    // Single survivor: PSD-power-weighted sub-pixel centroid over the component,
    // and the peak-power pixel for phase / purity / brightness.
    const int left = stats_.at<int>(survivor, cv::CC_STAT_LEFT);
    const int top = stats_.at<int>(survivor, cv::CC_STAT_TOP);
    const int width = stats_.at<int>(survivor, cv::CC_STAT_WIDTH);
    const int height = stats_.at<int>(survivor, cv::CC_STAT_HEIGHT);

    double weight_sum = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    double peak_power = 0.0;
    int peak_x = left;
    int peak_y = top;
    for (int y = top; y < top + height; ++y) {
        const int* lrow = labels_.ptr<int>(y);
        const float* prow = power_.ptr<float>(y);
        for (int x = left; x < left + width; ++x) {
            if (lrow[x] != survivor) {
                continue;
            }
            const double p = prow[x];
            weight_sum += p;
            cx += p * x;
            cy += p * y;
            if (p > peak_power) {
                peak_power = p;
                peak_x = x;
                peak_y = y;
            }
        }
    }

    LaserObservation obs;
    if (weight_sum > 0.0) {
        obs.centroid_px = cv::Point2f(static_cast<float>(cx / weight_sum),
                                      static_cast<float>(cy / weight_sum));
    } else {  // degenerate (all-zero power in the component) — fall back to CC centroid
        obs.centroid_px =
            cv::Point2f(static_cast<float>(centroids_.at<double>(survivor, 0)),
                        static_cast<float>(centroids_.at<double>(survivor, 1)));
    }
    obs.psd_power = peak_power;
    obs.purity = 2.0 * peak_power / (window_ * static_cast<double>(ac_.at<float>(peak_y, peak_x)));
    obs.cluster_area_px = static_cast<double>(stats_.at<int>(survivor, cv::CC_STAT_AREA));
    obs.capture_timestamp_ns = last_timestamp_ns_;

    // Peak brightness over the ON frames only (the latest frame is dark half the
    // time). The bin-2 reconstruction at the peak pixel is proportional to
    // cos(2pi*2*n/N + arg(X2)); frames where that is positive are the ON phase.
    const double phase =
        std::atan2(static_cast<double>(im_.at<float>(peak_y, peak_x)),
                   static_cast<double>(re_.at<float>(peak_y, peak_x)));
    double peak_brightness = 0.0;
    for (int n = 0; n < window_; ++n) {
        if (std::cos(2.0 * CV_PI * 2.0 * n / window_ + phase) <= 0.0) {
            continue;
        }
        const double v = ring_[(write_index_ + n) % window_].at<float>(peak_y, peak_x);
        peak_brightness = std::max(peak_brightness, v);
    }
    obs.peak_brightness = peak_brightness;
    return obs;
}

}  // namespace tracking
