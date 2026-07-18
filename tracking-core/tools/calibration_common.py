"""Shared helpers for the TRK-012/013 calibration tools.

Small, stdlib-plus-cv2 utilities the intrinsics and extrinsics tools both need:
loading the YAML config, resolving default output paths, atomic JSON writes, and
building the ArUco dictionary. Keeps the two CLIs from drifting apart.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Any

import cv2
import yaml


def load_config(config_path: Path) -> dict[str, Any]:
    with open(config_path, "r", encoding="utf-8") as handle:
        return yaml.safe_load(handle)


def default_output(config_path: Path, key: str) -> Path:
    """Resolve `calibration.<key>` (a repo-relative path) against the repo root."""
    cfg = load_config(config_path)
    return (config_path.parent.parent / cfg["calibration"][key]).resolve()


def aruco_dictionary(config_path: Path) -> cv2.aruco.Dictionary:
    cfg = load_config(config_path)
    name = cfg["calibration"]["aruco_dictionary"]
    return cv2.aruco.getPredefinedDictionary(getattr(cv2.aruco, f"DICT_{name}"))


def atomic_write_json(path: Path, payload: dict[str, Any]) -> None:
    """Write JSON via a temp file + rename so a reader never sees a partial file."""
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    with open(tmp, "w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2)
    os.replace(tmp, path)
