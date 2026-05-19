#!/usr/bin/env python3
"""
Convert an aerosynth-gtav capture session to a normalised per-frame layout.

Usage:
    python convert.py <session_dir> --out-dir <output_dir>

Output layout (one subdirectory per frame):
    <out_dir>/frame_NNNNNN/
        left_rgb.png        - left RGB
        right_rgb.png       - right RGB
        left_depth.npy      - float32 depth in metres
        left_depth.png      - depth visualisation (near=dark, far=bright)
        left_seg_cat.npy    - int32 stencil class IDs (see palette below)
        left_seg_cat.png    - false-colour segmentation (copied from source)
        calib.json          - K matrix, baseline, left/right extrinsics

Stencil class IDs (GTA V stencil buffer values, preserved as-is):
    0  inanimate objects / artificial surfaces (roads, buildings, props)
    1  persons
    2  vehicles
    3  foliage / trees
    4  natural ground / terrain
    7  sky
   -1  unknown (any colour not in the palette above)
"""

import argparse
import json
import math
import struct
from pathlib import Path

import numpy as np
from PIL import Image

# GTA V false-colour seg palette -> original stencil ID
SEG_PALETTE = {
    (64, 64, 64):       0,
    (255, 0, 0):        1,
    (0, 0, 255):        2,
    (0, 204, 0):        3,
    (139, 69, 19):      4,
    (135, 206, 235):    7,
}


def decode_depth_bmp(path):
    """Read a 16-bit BI_RGB BMP and return a float32 array of depth in metres."""
    data = Path(path).read_bytes()
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    width        = struct.unpack_from("<i", data, 18)[0]
    height       = struct.unpack_from("<i", data, 22)[0]
    top_down = height < 0
    height = abs(height)
    depth16 = np.frombuffer(data[pixel_offset:], dtype="<u2", count=width * height)
    depth16 = depth16.reshape(height, width)
    if not top_down:
        depth16 = depth16[::-1].copy()
    return depth16.astype(np.float32) / 65535.0 * 500.0


def decode_seg_bmp(path):
    """Convert the false-colour seg BMP to an int32 array of stencil class IDs."""
    rgb = np.array(Image.open(path).convert("RGB"))
    out = np.full(rgb.shape[:2], -1, dtype=np.int32)
    for colour, cls_id in SEG_PALETTE.items():
        mask = np.all(rgb == np.array(colour, dtype=np.uint8), axis=-1)
        out[mask] = cls_id
    return out


def depth_to_vis(depth_m):
    """Normalise float32 depth to a uint8 grayscale image for visualisation."""
    valid = depth_m < 500.0
    d = np.where(valid, depth_m, 0.0)
    mx = d.max()
    if mx > 0:
        d = d / mx * 255.0
    return d.astype(np.uint8)


def fov_to_K(fov_deg, width, height):
    """Build a 3x3 intrinsics matrix from horizontal FOV."""
    fx = (width / 2.0) / math.tan(math.radians(fov_deg) / 2.0)
    return [[fx, 0.0, width / 2.0],
            [0.0, fx, height / 2.0],
            [0.0, 0.0, 1.0]]


def convert_session(session_dir, out_dir):
    session_dir = Path(session_dir)
    out_dir = Path(out_dir)

    jsons = sorted(session_dir.glob("frame_*.json"))
    if not jsons:
        raise FileNotFoundError(f"No frame JSON files found in {session_dir}")

    print(f"Converting {len(jsons)} frame(s): {session_dir} -> {out_dir}")

    for json_path in jsons:
        frame_name = json_path.stem
        frame_out = out_dir / frame_name
        frame_out.mkdir(parents=True, exist_ok=True)

        with open(json_path) as f:
            meta = json.load(f)

        W    = meta["resolution"]["width"]
        H    = meta["resolution"]["height"]
        cam  = meta["camera"]
        cam_r = meta.get("camera_right", {})
        files = meta["files"]

        Image.open(session_dir / files["rgb"]).convert("RGB").save(frame_out / "left_rgb.png")
        Image.open(session_dir / files["right"]).convert("RGB").save(frame_out / "right_rgb.png")

        depth_m = decode_depth_bmp(session_dir / files["depth"])
        np.save(frame_out / "left_depth.npy", depth_m)
        Image.fromarray(depth_to_vis(depth_m)).save(frame_out / "left_depth.png")

        seg = decode_seg_bmp(session_dir / files["segmentation"])
        np.save(frame_out / "left_seg_cat.npy", seg)
        Image.open(session_dir / files["segmentation"]).save(frame_out / "left_seg_cat.png")

        calib = {
            "K": fov_to_K(cam["intrinsics"]["fov"], W, H),
            "baseline_m": 1.0,
            "near_clip": cam["intrinsics"]["near_clip"],
            "far_clip":  cam["intrinsics"]["far_clip"],
            "left":  {"position": cam["position"],  "rotation": cam["rotation"]},
            "right": {"position": cam_r.get("position", {}), "rotation": cam_r.get("rotation", {})},
        }
        with open(frame_out / "calib.json", "w") as f:
            json.dump(calib, f, indent=2)

        print(f"  {frame_name}")

    print("Done.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert aerosynth-gtav session to normalised format.")
    parser.add_argument("session_dir", help="Path to a capture session directory")
    parser.add_argument("--out-dir", required=True, help="Output directory")
    args = parser.parse_args()
    convert_session(args.session_dir, args.out_dir)
