#include "camera_source.hpp"
#include "capture_thread.hpp"
#include "config.hpp"
#include "frame_ring_buffer.hpp"
#include "logging.hpp"
#include "tracking_pipeline.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <zmq.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    const std::string config_path =
        (argc > 1) ? argv[1] : "config/tracking_core.yaml";
    tracking::Config config;
    try {
        config = tracking::Config::load(config_path);
        // One catch for the whole pre-logger window: ConfigError from load,
        // filesystem_error/spdlog_ex from init. The logger cannot report its
        // own bootstrap failures, so this path stays on stderr.
        tracking::logging::init(config.logging);
    } catch (const std::exception& e) {
        std::cerr << "Startup error: " << e.what() << std::endl;
        return 1;
    }

    // Post-init failures (zmq bind, OpenCV throws) must drain the logger and
    // exit 1 rather than reach std::terminate with queued messages lost.
    try {
        tracking::CameraSource camera(config.camera);
        if (!camera.is_open()) {
            LOG_ERROR("Unable to open camera device {}", config.camera.device_id);
            tracking::logging::shutdown();
            return 1;
        }

        zmq::context_t context(1);
        zmq::socket_t publisher(context, zmq::socket_type::pub);
        publisher.set(zmq::sockopt::sndhwm, 1);  // ADR-002: drop stale frames on lag.
        publisher.bind(config.zmq.bind_address);

        tracking::Detector detector;
        tracking::Tracker tracker;

        // Capture (producer) and processing (consumer) sides of the TRK-005
        // ring buffer. Frames arrive as 8-bit BGR at the configured size.
        tracking::FrameRingBuffer ring_buffer(
            static_cast<std::size_t>(config.pipeline.ring_buffer_capacity),
            config.camera.height, config.camera.width, CV_8UC3);
        tracking::CaptureThread::Options capture_options;
        capture_options.cpu_core = config.pipeline.capture_cpu_core;
        capture_options.thread_priority = config.pipeline.capture_thread_priority;
        capture_options.camera_id = 0;
        tracking::CaptureThread capture(camera, ring_buffer, capture_options);
        capture.start();

        cv::Mat frame(config.camera.height, config.camera.width, CV_8UC3);
        while (true) {
            const auto metadata = ring_buffer.try_pop(frame);
            if (!metadata.has_value()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            const cv::Rect detection = detector.detect(frame);
            const cv::Rect box = tracker.update(detection);
            if (box.area() > 0) {
                cv::rectangle(frame, box, cv::Scalar(0, 255, 0), 2);
            }

            std::vector<uchar> encoded;
            if (!cv::imencode(".jpg", frame, encoded)) {
                continue;
            }

            zmq::message_t topic("frames", 6);
            publisher.send(topic, zmq::send_flags::sndmore);
            publisher.send(zmq::buffer(encoded), zmq::send_flags::none);
        }
    } catch (const std::exception& e) {
        LOG_CRITICAL("fatal error after startup: {}", e.what());
        tracking::logging::shutdown();
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    tracking::logging::shutdown();
    return 0;
}
