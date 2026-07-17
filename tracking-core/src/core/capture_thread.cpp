#include "capture_thread.hpp"

#include "logging.hpp"

#include <chrono>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

namespace tracking {

namespace {

// Best-effort realtime setup for the current thread. Failures are expected on
// unprivileged dev machines — WARN once and continue (ticket TRK-006:
// graceful degradation; tests must not require privileges).
void apply_realtime(const CaptureThread::Options& options) {
#ifdef __linux__
    sched_param param{};
    param.sched_priority = options.thread_priority;
    // Priority-inversion note: this thread holds SCHED_FIFO while logging
    // through spdlog's mutex-guarded queue. Calls here are bounded to
    // startup and the 1 Hz drop summary — never per-frame (KTD-2 in the
    // TRK-004 plan).
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        LOG_WARN("capture thread: SCHED_FIFO priority {} unavailable (unprivileged?) — "
                 "continuing at default scheduling",
                 options.thread_priority);
    }
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(options.cpu_core, &cpus);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpus), &cpus) != 0) {
        LOG_WARN("capture thread: could not pin to CPU core {} — continuing unpinned",
                 options.cpu_core);
    }
#else
    (void)options;
#endif
}

}  // namespace

CaptureThread::CaptureThread(FrameSource& source, FrameRingBuffer& buffer,
                             const Options& options)
    : source_(source), buffer_(buffer), options_(options) {}

CaptureThread::~CaptureThread() { stop(); }

void CaptureThread::start() {
    if (thread_.joinable()) {
        return;  // already running
    }
    stop_.store(false, std::memory_order_relaxed);
    thread_ = std::thread([this] { run(); });
}

void CaptureThread::stop() {
    stop_.store(true, std::memory_order_relaxed);
    if (thread_.joinable()) {
        thread_.join();
    }
}

void CaptureThread::run() {
    using clock = std::chrono::steady_clock;
    apply_realtime(options_);

    std::uint64_t drops_in_window = 0;
    clock::time_point window_start = clock::now();

    while (!stop_.load(std::memory_order_relaxed)) {
        const clock::time_point grab_start = clock::now();
        if (!source_.grab(scratch_)) {
            grab_failures_.fetch_add(1, std::memory_order_relaxed);
            // Source failure (disconnect / synthetic source exhausted): brief
            // pause instead of a hot spin; the owner decides recovery.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        const clock::time_point grab_end = clock::now();

        FrameMetadata metadata;
        metadata.capture_timestamp_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(grab_end.time_since_epoch())
                .count();
        metadata.sequence_number = next_sequence_++;
        metadata.capture_duration_us = static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(grab_end - grab_start)
                .count());
        metadata.camera_id = options_.camera_id;

        if (buffer_.try_push(scratch_, metadata)) {
            frames_dropped_.fetch_add(1, std::memory_order_relaxed);
            ++drops_in_window;
        }
        frames_captured_.fetch_add(1, std::memory_order_relaxed);

        // Drop summary at most once per second — aggregated, never per-frame
        // (KTD-2: bounded logging from the SCHED_FIFO thread).
        if (drops_in_window > 0 && grab_end - window_start >= std::chrono::seconds(1)) {
            LOG_WARN("capture: dropped {} frame(s) in the last second (consumer lagging)",
                     drops_in_window);
            drops_in_window = 0;
            window_start = grab_end;
        }
    }
}

}  // namespace tracking
