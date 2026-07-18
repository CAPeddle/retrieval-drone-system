// Plan U13: export thread implementation.

#include "export_thread.hpp"

#include <chrono>
#include <fstream>
#include <string>

namespace tracking {

namespace {

std::int64_t steady_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace

ExportThread::ExportThread(SpscRing<TrackingSnapshot>& ring, ZmqPublisher& publisher)
    : ring_(&ring), publisher_(&publisher) {}

ExportThread::~ExportThread() { stop(); }

void ExportThread::start() {
    if (thread_.joinable()) {
        return;
    }
    stop_.store(false, std::memory_order_relaxed);
    thread_ = std::thread(&ExportThread::run, this);
}

void ExportThread::stop() {
    if (!thread_.joinable()) {
        return;
    }
    stop_.store(true, std::memory_order_relaxed);
    thread_.join();
}

double ExportThread::read_cpu_temp_c() {
    // Pi 5 thermal zone; 1 Hz cadence, export thread only. -1.0 when the
    // platform has no readable zone (dev host, containers).
    std::ifstream stream("/sys/class/thermal/thermal_zone0/temp");
    long millidegrees = 0;
    if (stream >> millidegrees) {
        return static_cast<double>(millidegrees) / 1000.0;
    }
    return -1.0;
}

void ExportThread::run() {
    while (!stop_.load(std::memory_order_relaxed)) {
        const std::int64_t now = steady_ns();
        if (now - last_temp_read_ns_ >= 1'000'000'000) {
            cpu_temp_c_ = read_cpu_temp_c();
            last_temp_read_ns_ = now;
        }
        const std::optional<TrackingSnapshot> snapshot = ring_->try_pop();
        if (!snapshot.has_value()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        publisher_->publish(*snapshot, cpu_temp_c_);
        published_.fetch_add(1, std::memory_order_relaxed);
    }
    // Drain (plan R23): the freshest outstanding snapshot still goes out so a
    // consumer sees the final pre-shutdown state.
    while (const std::optional<TrackingSnapshot> snapshot = ring_->try_pop()) {
        publisher_->publish(*snapshot, cpu_temp_c_);
        published_.fetch_add(1, std::memory_order_relaxed);
    }
}

}  // namespace tracking
