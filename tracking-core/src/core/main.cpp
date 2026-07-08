#include "config.hpp"
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
    } catch (const tracking::ConfigError& e) {
        std::cerr << "Config error: " << e.what() << std::endl;
        return 1;
    }

    cv::VideoCapture camera(config.camera.device_id);
    if (!camera.isOpened()) {
        std::cerr << "Unable to open camera device " << config.camera.device_id
                  << std::endl;
        return 1;
    }

    zmq::context_t context(1);
    zmq::socket_t publisher(context, zmq::socket_type::pub);
    publisher.set(zmq::sockopt::sndhwm, 1);  // ADR-002: drop stale frames on lag.
    publisher.bind(config.zmq.bind_address);

    tracking::Detector detector;
    tracking::Tracker tracker;

    while (true) {
        cv::Mat frame;
        camera >> frame;
        if (frame.empty()) {
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

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}
