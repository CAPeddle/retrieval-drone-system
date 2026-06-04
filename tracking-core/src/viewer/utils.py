import cv2
import numpy as np


def decode_frame(payload: bytes):
    array = np.frombuffer(payload, dtype=np.uint8)
    if array.size == 0:
        return None
    return cv2.imdecode(array, cv2.IMREAD_COLOR)
