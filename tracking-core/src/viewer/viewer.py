import cv2
import zmq

from utils import decode_frame


def main() -> None:
    context = zmq.Context()
    subscriber = context.socket(zmq.SUB)
    subscriber.connect("tcp://127.0.0.1:5556")
    subscriber.setsockopt_string(zmq.SUBSCRIBE, "frames")

    while True:
        _topic, payload = subscriber.recv_multipart()
        frame = decode_frame(payload)
        if frame is None:
            continue

        cv2.imshow("Tracking Viewer", frame)
        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    subscriber.close()
    context.term()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
