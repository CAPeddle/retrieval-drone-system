"""Generate SYNTHETIC bench calibration artifacts for pipeline verification.

Writes an intrinsics/extrinsics JSON pair that passes every TRK-017 loader
check (schema, self-consistency, pairing, undistorted fit) using the same
known geometry as the C++ test fixtures: a zero-distortion camera 1 m above
the floor at 45 degrees tilt. The artifacts let the real binary run end to end
(replay mode, throughput, shutdown, stream) when no physically calibrated
camera exists.

THESE ARTIFACTS ARE NOT A CALIBRATION. camera_id is set to
"bench-synthetic" so no one mistakes the output coordinates for measured
floor positions. Replace with real TRK-012/013 artifacts before any
accuracy-bearing use.

Usage:
    python tools/make_bench_calibration.py [output_dir]   # default: config/
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import numpy as np

CAMERA_ID = "bench-synthetic"


def camera_matrix() -> np.ndarray:
    return np.array([[600.0, 0.0, 320.0], [0.0, 600.0, 240.0], [0.0, 0.0, 1.0]])


def floor_to_pixel() -> np.ndarray:
    """K [r1 r2 t] for a camera at (0, 0, 1) looking at floor point (0, 1, 0)."""
    s = 1.0 / math.sqrt(2.0)
    rotation = np.array([[1.0, 0.0, 0.0], [0.0, -s, -s], [0.0, s, -s]])
    centre = np.array([0.0, 0.0, 1.0])
    t = -rotation @ centre
    cols = np.column_stack([rotation[:, 0], rotation[:, 1], t])
    return camera_matrix() @ cols


def main() -> int:
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("config")
    out_dir.mkdir(parents=True, exist_ok=True)

    h_floor_to_pixel = floor_to_pixel()
    h_pixel_to_floor = np.linalg.inv(h_floor_to_pixel)

    anchors = []
    for marker_id, (x_m, y_m) in enumerate(
        [(0.0, 1.0), (0.8, 2.0), (-0.8, 2.0), (0.4, 3.0), (-0.4, 1.5)]
    ):
        pixel = h_floor_to_pixel @ np.array([x_m, y_m, 1.0])
        pixel = pixel[:2] / pixel[2]
        anchors.append(
            {
                "marker_id": marker_id,
                "floor_x_m": x_m,
                "floor_y_m": y_m,
                "image_x_px": float(pixel[0]),
                "image_y_px": float(pixel[1]),
            }
        )

    intrinsics = {
        "intrinsics_version": 1,
        "camera_id": CAMERA_ID,
        "camera_matrix": camera_matrix().tolist(),
        "dist_coeffs": [0.0, 0.0, 0.0, 0.0, 0.0],
        "image_size": [640, 480],
        "reprojection_error_px": 0.0,
        "calibration_timestamp": "2026-07-19T00:00:00+00:00",
    }
    extrinsics = {
        "extrinsics_version": 1,
        "camera_id": CAMERA_ID,
        "homography_3x3": h_pixel_to_floor.tolist(),
        "floor_anchor_points": anchors,
        "reprojection_error_m": 1.0e-9,
        "undistorted_coordinates": True,
        "calibration_timestamp": "2026-07-19T00:00:01+00:00",
    }

    (out_dir / "intrinsics.json").write_text(json.dumps(intrinsics, indent=2))
    (out_dir / "extrinsics.json").write_text(json.dumps(extrinsics, indent=2))
    print(f"WROTE SYNTHETIC bench artifacts to {out_dir}/ (camera_id={CAMERA_ID})")
    print("Not a calibration - pipeline verification only.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
