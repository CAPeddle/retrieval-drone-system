#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <zmq.hpp>

#include "include/tracking_pipeline.hpp"

int main() {
    cv::VideoCapture camera(0);
    if (!camera.isOpened()) {
        std::cerr << "Unable to open camera device 0" << std::endl;
        return 1;
    }

    zmq::context_t context(1);
    zmq::socket_t publisher(context, zmq::socket_type::pub);
    publisher.set(zmq::sockopt::sndhwm, 1);
    publisher.bind("tcp://*:5556");

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
