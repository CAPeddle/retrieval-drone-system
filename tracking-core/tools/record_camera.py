import argparse
from pathlib import Path

import cv2


def main() -> None:
    parser = argparse.ArgumentParser(description="Record raw camera frames to disk")
    parser.add_argument("--output", default="recordings", help="Output directory")
    parser.add_argument("--frames", type=int, default=100, help="Number of frames to capture")
    args = parser.parse_args()

    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        raise RuntimeError("Unable to open camera")

    for idx in range(args.frames):
        ok, frame = cap.read()
        if not ok:
            continue
        cv2.imwrite(str(output_dir / f"frame_{idx:05d}.png"), frame)

    cap.release()


if __name__ == "__main__":
    main()
