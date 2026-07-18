#include "replay_source.hpp"

#include <thread>

namespace tracking {

ReplaySource::ReplaySource(const std::string& path) : ReplaySource(path, Options()) {}

ReplaySource::ReplaySource(const std::string& path, const Options& options)
    : capture_(path), options_(options) {}

bool ReplaySource::grab(cv::Mat& out) {
    if (!capture_.isOpened()) {
        return false;
    }
    if (options_.fps > 0.0) {
        if (!started_) {
            next_ = std::chrono::steady_clock::now();
            started_ = true;
        }
        std::this_thread::sleep_until(next_);
        next_ += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(1.0 / options_.fps));
    }
    if (capture_.read(out)) {
        return true;
    }
    if (options_.loop) {
        // Rewind and retry once; a clip that still yields nothing is dead.
        capture_.set(cv::CAP_PROP_POS_FRAMES, 0);
        return capture_.read(out);
    }
    return false;
}

}  // namespace tracking
