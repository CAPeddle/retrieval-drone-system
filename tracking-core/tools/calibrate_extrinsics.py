#!/usr/bin/env python3
"""TRK-013: compute the image->FloorPlane2D homography (ADR-006).

Offline, install-time tool. The operator places ArUco markers at known floor
positions (a layout JSON: marker_id -> floor x,y in metres) and captures one
image; the tool detects the markers, matches them to their floor coordinates,
and fits the homography that maps undistorted image pixels to the floor frame.

Least squares, not RANSAC (KTD from review): anchors are few, hand-measured
correspondences, and findHomography's RANSAC threshold is in destination units
(metres here) — a bad anchor must FAIL the validation gate, not be silently
excluded as an outlier. Reprojection error is computed over ALL anchors.
"""

from __future__ import annotations

import argparse
import datetime
import json
import sys
from pathlib import Path

import cv2
import numpy as np

from calibration_common import (
    aruco_dictionary,
    atomic_write_json,
    default_output,
)

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONFIG = REPO_ROOT / "config" / "tracking_core.yaml"
EXTRINSICS_VERSION = 1
MIN_ANCHORS = 4
COLLINEARITY_RATIO = 1e-3  # reject sigma2/sigma1 below this (provisional, KTD-6)
MAX_ERROR_M = 0.020  # ADR-004 CAL_HEALTH_FAIL_M
MAX_ERROR_PX = 3.0  # ADR-006 rejection gate


def detect_marker_centroids(
    image: np.ndarray, config_path: Path
) -> dict[int, np.ndarray]:
    """Return {marker_id: image centroid (px)} for every detected marker."""
    dictionary = aruco_dictionary(config_path)
    detector = cv2.aruco.ArucoDetector(dictionary, cv2.aruco.DetectorParameters())
    gray = image if image.ndim == 2 else cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    corners, ids, _ = detector.detectMarkers(gray)
    centroids: dict[int, np.ndarray] = {}
    if ids is None:
        return centroids
    for marker_corners, marker_id in zip(corners, ids.flatten()):
        centroids[int(marker_id)] = marker_corners.reshape(4, 2).mean(axis=0)
    return centroids


def load_layout(path: Path) -> dict[int, np.ndarray]:
    """Load {marker_id: [floor_x_m, floor_y_m]} from a layout JSON."""
    with open(path, "r", encoding="utf-8") as handle:
        raw = json.load(handle)
    return {int(k): np.array(v, dtype=np.float64) for k, v in raw.items()}


def undistort(points: np.ndarray, intrinsics_path: Path | None) -> np.ndarray:
    """Undistort image points if intrinsics are supplied; else pass through."""
    if intrinsics_path is None:
        return points
    with open(intrinsics_path, "r", encoding="utf-8") as handle:
        data = json.load(handle)
    camera_matrix = np.array(data["camera_matrix"], dtype=np.float64)
    dist = np.array(data["dist_coeffs"], dtype=np.float64)
    reshaped = points.reshape(-1, 1, 2)
    # P=camera_matrix keeps the result in pixel coordinates.
    undistorted = cv2.undistortPoints(reshaped, camera_matrix, dist, P=camera_matrix)
    return undistorted.reshape(-1, 2)


def condition_ratio(floor_points: np.ndarray) -> float:
    """sigma2/sigma1 of the mean-centred floor points; ~0 means collinear."""
    centred = floor_points - floor_points.mean(axis=0)
    singular = np.linalg.svd(centred, compute_uv=False)
    if singular[0] == 0:
        return 0.0
    return float(singular[1] / singular[0])


def reprojection_errors(
    homography: np.ndarray, image_pts: np.ndarray, floor_pts: np.ndarray
) -> tuple[float, float]:
    """Max metric error (image->floor) and max pixel error (floor->image),
    both over ALL anchors — never an inlier subset."""
    to_floor = cv2.perspectiveTransform(image_pts.reshape(-1, 1, 2), homography)
    metric_err = np.linalg.norm(to_floor.reshape(-1, 2) - floor_pts, axis=1).max()
    inverse = np.linalg.inv(homography)
    to_image = cv2.perspectiveTransform(floor_pts.reshape(-1, 1, 2), inverse)
    pixel_err = np.linalg.norm(to_image.reshape(-1, 2) - image_pts, axis=1).max()
    return float(metric_err), float(pixel_err)


def write_json(
    path: Path,
    camera_id: str,
    homography: np.ndarray,
    anchors: list[dict[str, float]],
    reprojection_error_m: float,
    undistorted: bool,
) -> None:
    payload = {
        "extrinsics_version": EXTRINSICS_VERSION,
        "camera_id": camera_id,
        "homography_3x3": homography.tolist(),
        "floor_anchor_points": anchors,
        "reprojection_error_m": reprojection_error_m,
        # Whether the homography operates on undistorted pixels — true only when
        # --intrinsics was supplied. The runtime consumer (TRK-017) must
        # undistort first iff this is true; a false value means the fit was on
        # raw distorted pixels and the contract differs.
        "undistorted_coordinates": undistorted,
        "calibration_timestamp": datetime.datetime.now(
            datetime.timezone.utc
        ).isoformat(),
    }
    atomic_write_json(path, payload)


def run(args: argparse.Namespace) -> int:
    image = cv2.imread(str(args.image), cv2.IMREAD_COLOR)
    if image is None:
        print(f"error: cannot read image {args.image}", file=sys.stderr)
        return 1
    layout = load_layout(Path(args.layout))
    centroids = detect_marker_centroids(image, args.config)

    matched_ids = sorted(set(layout) & set(centroids))
    if len(matched_ids) < MIN_ANCHORS:
        print(
            f"error: only {len(matched_ids)} anchors match between layout and "
            f"detected markers (need >= {MIN_ANCHORS})",
            file=sys.stderr,
        )
        return 1
    if len(matched_ids) == MIN_ANCHORS:
        print(
            "warning: exactly 4 anchors — the homography is an exact fit and the "
            "reprojection gates carry no information; use >= 5 anchors",
            file=sys.stderr,
        )

    image_pts = np.array([centroids[i] for i in matched_ids], dtype=np.float64)
    floor_pts = np.array([layout[i] for i in matched_ids], dtype=np.float64)

    if condition_ratio(floor_pts) < COLLINEARITY_RATIO:
        print(
            "error: floor anchors are near-collinear (degenerate layout); spread "
            "them across the field of view",
            file=sys.stderr,
        )
        return 1

    undistorted = args.intrinsics is not None
    image_pts = undistort(image_pts, Path(args.intrinsics) if undistorted else None)
    homography, _ = cv2.findHomography(image_pts, floor_pts, method=0)
    if homography is None:
        print("error: homography fit failed", file=sys.stderr)
        return 1

    error_m, error_px = reprojection_errors(homography, image_pts, floor_pts)
    if error_m > MAX_ERROR_M:
        print(
            f"error: reprojection error {error_m * 1000:.1f} mm exceeds "
            f"{MAX_ERROR_M * 1000:.0f} mm (CAL_HEALTH_FAIL_M); re-measure the anchors",
            file=sys.stderr,
        )
        return 1
    if error_px > MAX_ERROR_PX:
        print(
            f"error: reprojection error {error_px:.2f} px exceeds "
            f"{MAX_ERROR_PX} px (ADR-006 gate)",
            file=sys.stderr,
        )
        return 1

    # Store the image points the homography was actually fitted on (undistorted
    # when intrinsics were supplied) so applying H to them reproduces the floor
    # coords — self-consistent with the undistorted_coordinates flag.
    anchors = [
        {
            "marker_id": int(marker_id),
            "floor_x_m": float(layout[marker_id][0]),
            "floor_y_m": float(layout[marker_id][1]),
            "image_x_px": float(image_pts[k][0]),
            "image_y_px": float(image_pts[k][1]),
        }
        for k, marker_id in enumerate(matched_ids)
    ]
    write_json(
        Path(args.output), args.camera_id, homography, anchors, error_m, undistorted
    )
    print(
        f"extrinsics written to {args.output} "
        f"(reprojection error {error_m * 1000:.1f} mm / {error_px:.2f} px, "
        f"{len(matched_ids)} anchors)"
    )
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--image", required=True, help="capture with markers at floor anchors"
    )
    parser.add_argument(
        "--layout", required=True, help="JSON: marker_id -> [floor_x_m, floor_y_m]"
    )
    parser.add_argument(
        "--intrinsics", help="intrinsics JSON for undistortion (recommended)"
    )
    parser.add_argument(
        "--config",
        type=Path,
        default=DEFAULT_CONFIG,
        help="tracking_core.yaml for the ArUco dictionary + output path",
    )
    parser.add_argument("--output", help="extrinsics JSON path (default: from config)")
    parser.add_argument("--camera-id", default="cam0", help="camera identity label")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.output is None:
        args.output = str(default_output(args.config, "extrinsics_path"))
    try:
        return run(args)
    except (OSError, ValueError, KeyError, cv2.error) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
