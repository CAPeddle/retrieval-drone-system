#!/usr/bin/env python3
"""TRK-012: compute per-camera intrinsics from Charuco captures.

Offline, install-time tool. Primary mode reads a directory of board images
(deterministic, testable); a thin --camera live-capture wrapper exists for
bench use and is not covered by tests. Output is a versioned JSON file consumed
by the tracking core at startup (ADR-004 Phase 1, ADR-010).

Modern OpenCV path (KTD-3): CharucoDetector.detectBoard -> matchImagePoints ->
calibrateCamera. The removed cv2.aruco.calibrateCameraCharuco is never called.
"""

from __future__ import annotations

import argparse
import datetime
import sys
from pathlib import Path

import cv2
import numpy as np

from calibration_common import atomic_write_json, default_output, load_config

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONFIG = REPO_ROOT / "config" / "tracking_core.yaml"
INTRINSICS_VERSION = 1
MIN_VIEWS_DEFAULT = 8


def load_board(
    config_path: Path,
) -> tuple[cv2.aruco.CharucoBoard, cv2.aruco.CharucoDetector]:
    """Build the Charuco board + detector from the YAML calibration section."""
    cfg = load_config(config_path)
    charuco = cfg["calibration"]["charuco"]
    dict_name = cfg["calibration"]["aruco_dictionary"]
    dictionary = cv2.aruco.getPredefinedDictionary(
        getattr(cv2.aruco, f"DICT_{dict_name}")
    )
    board = cv2.aruco.CharucoBoard(
        (charuco["squares_x"], charuco["squares_y"]),
        float(charuco["square_length_m"]),
        float(charuco["marker_length_m"]),
        dictionary,
    )
    return board, cv2.aruco.CharucoDetector(board)


def collect_correspondences(
    image_paths: list[Path],
    board: cv2.aruco.CharucoBoard,
    detector: cv2.aruco.CharucoDetector,
) -> tuple[list[np.ndarray], list[np.ndarray], tuple[int, int]]:
    """Detect the board in each image; return per-view object/image points."""
    object_points: list[np.ndarray] = []
    image_points: list[np.ndarray] = []
    image_size: tuple[int, int] | None = None
    for path in image_paths:
        gray = cv2.imread(str(path), cv2.IMREAD_GRAYSCALE)
        if gray is None:
            continue
        image_size = (gray.shape[1], gray.shape[0])
        corners, ids, _, _ = detector.detectBoard(gray)
        if ids is None or len(ids) < 4:
            continue
        obj, img = board.matchImagePoints(corners, ids)
        if obj is None or len(obj) < 4:
            continue
        object_points.append(obj)
        image_points.append(img)
    if image_size is None:
        raise ValueError("no readable images found")
    return object_points, image_points, image_size


def calibrate(
    object_points: list[np.ndarray],
    image_points: list[np.ndarray],
    image_size: tuple[int, int],
) -> tuple[float, np.ndarray, np.ndarray]:
    """Run calibrateCamera; fix the distortion terms the synthetic camera lacks."""
    flags = cv2.CALIB_FIX_K3 | cv2.CALIB_ZERO_TANGENT_DIST
    rms, camera_matrix, dist_coeffs, _, _ = cv2.calibrateCamera(
        object_points, image_points, image_size, None, None, flags=flags
    )
    return rms, camera_matrix, dist_coeffs


def write_json(
    path: Path,
    camera_id: str,
    camera_matrix: np.ndarray,
    dist_coeffs: np.ndarray,
    image_size: tuple[int, int],
    reprojection_error_px: float,
) -> None:
    """Atomic write of the versioned intrinsics JSON (ADR-004 schema)."""
    payload = {
        "intrinsics_version": INTRINSICS_VERSION,
        "camera_id": camera_id,
        "camera_matrix": camera_matrix.tolist(),
        "dist_coeffs": dist_coeffs.reshape(-1).tolist(),
        "image_size": [image_size[0], image_size[1]],
        "reprojection_error_px": float(reprojection_error_px),
        "calibration_timestamp": datetime.datetime.now(
            datetime.timezone.utc
        ).isoformat(),
    }
    atomic_write_json(path, payload)


def run(args: argparse.Namespace) -> int:
    board, detector = load_board(args.config)
    if args.images is not None:
        image_paths = sorted(Path(args.images).glob("*.png")) + sorted(
            Path(args.images).glob("*.jpg")
        )
    else:  # pragma: no cover — live capture is bench-only (KTD-5)
        image_paths = _capture_live(args.camera, args.frames)

    object_points, image_points, image_size = collect_correspondences(
        image_paths, board, detector
    )
    if len(object_points) < args.min_views:
        print(
            f"error: only {len(object_points)} usable views (need >= {args.min_views}); "
            "hold the board steadier and cover more of the frame",
            file=sys.stderr,
        )
        return 1

    rms, camera_matrix, dist_coeffs = calibrate(object_points, image_points, image_size)
    if rms > args.max_reprojection_error:
        print(
            f"error: reprojection error {rms:.3f} px exceeds "
            f"{args.max_reprojection_error} px; recapture with a flatter, "
            "better-lit board",
            file=sys.stderr,
        )
        return 1

    write_json(
        Path(args.output), args.camera_id, camera_matrix, dist_coeffs, image_size, rms
    )
    print(
        f"intrinsics written to {args.output} (reprojection error {rms:.3f} px, "
        f"{len(object_points)} views)"
    )
    return 0


def _capture_live(camera_id: int, frames: int) -> list[Path]:  # pragma: no cover
    raise NotImplementedError(
        "live capture is a bench-only wrapper; use --images for automated runs"
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--images", help="directory of Charuco board captures")
    source.add_argument("--camera", type=int, help="live camera index (bench-only)")
    parser.add_argument(
        "--frames", type=int, default=20, help="frames to capture (live)"
    )
    parser.add_argument(
        "--config",
        type=Path,
        default=DEFAULT_CONFIG,
        help="tracking_core.yaml for board geometry + output path",
    )
    parser.add_argument("--output", help="intrinsics JSON path (default: from config)")
    parser.add_argument("--camera-id", default="cam0", help="camera identity label")
    parser.add_argument(
        "--max-reprojection-error",
        type=float,
        default=1.0,
        help="reject calibration above this px error",
    )
    parser.add_argument(
        "--min-views",
        type=int,
        default=MIN_VIEWS_DEFAULT,
        help="minimum usable board views",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.output is None:
        args.output = str(default_output(args.config, "intrinsics_path"))
    try:
        return run(args)
    except (OSError, ValueError, cv2.error) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
