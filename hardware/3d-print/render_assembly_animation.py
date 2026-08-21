#!/usr/bin/env python3
"""Render an exploded-view MP4 from the SmartHomeCube FreeCAD assembly.

Requires FreeCAD, NumPy, Pillow, and FFmpeg. FreeCAD supplies the exact body
geometry and authored placements from ``src/SmartHomeCube.FCStd``.
"""

from __future__ import annotations

import argparse
import math
import struct
import shutil
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


PARTS = (
    ("Body", "Main", (0.18, 0.48, 0.78), 0.0),
    ("Body003", "Inlay_mid", (0.35, 0.72, 0.47), 35.0),
    ("Body004", "Inlay_top", (0.80, 0.38, 0.31), 55.0),
    ("Body001", "Lid", (0.94, 0.66, 0.20), 80.0),
)


def smoothstep(value: float) -> float:
    return value * value * (3.0 - 2.0 * value)


def explosion_amount(frame: int, fps: int) -> float:
    """Assembled hold, explode, exploded hold, then reassemble."""
    seconds = frame / fps
    if seconds < 0.6:
        return 0.0
    if seconds < 2.6:
        return smoothstep((seconds - 0.6) / 2.0)
    if seconds < 4.1:
        return 1.0
    if seconds < 6.1:
        return smoothstep((6.1 - seconds) / 2.0)
    return 0.0


def find_freecadcmd(temp_dir: Path) -> str:
    executable = shutil.which("FreeCADCmd") or shutil.which("freecadcmd")
    if executable:
        return executable
    appimage = Path("/opt/appimages/freecad.AppImage")
    if not appimage.exists():
        raise SystemExit("FreeCADCmd not found, and no FreeCAD AppImage is available")
    extract_dir = temp_dir / "freecad"
    extract_dir.mkdir()
    subprocess.run(
        (str(appimage), "--appimage-extract"),
        cwd=extract_dir,
        check=True,
        stdout=subprocess.DEVNULL,
    )
    return str(extract_dir / "squashfs-root/usr/bin/freecadcmd")


def export_freecad_bodies(model_path: Path, output_dir: Path, freecadcmd: str) -> None:
    script = output_dir / "export_bodies.py"
    rows = ["import FreeCAD as App", "import Mesh", f"doc = App.openDocument({str(model_path)!r})"]
    for object_name, _, _, _ in PARTS:
        rows.append(f"Mesh.export([doc.getObject({object_name!r})], {str(output_dir / (object_name + '.stl'))!r})")
    rows.append("App.closeDocument(doc.Name)")
    script.write_text("\n".join(rows) + "\n", encoding="utf-8")
    environment = dict(__import__("os").environ)
    environment.update(XDG_CACHE_HOME=str(output_dir / "cache"), XDG_CONFIG_HOME=str(output_dir / "config"))
    subprocess.run(
        (freecadcmd, str(script)),
        check=True,
        env=environment,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def load_stl(path: Path) -> tuple[np.ndarray, np.ndarray]:
    data = path.read_bytes()
    triangle_count = struct.unpack_from("<I", data, 80)[0]
    vertices = np.empty((triangle_count * 3, 3), dtype=float)
    for index in range(triangle_count):
        record = struct.unpack_from("<12fH", data, 84 + index * 50)
        vertices[index * 3 : index * 3 + 3] = np.asarray(record[3:12]).reshape(3, 3)
    return vertices, np.arange(triangle_count * 3).reshape(-1, 3)


def render_frame(
    meshes: list[tuple[np.ndarray, np.ndarray, np.ndarray, float]],
    amount: float,
    output: Path,
    width: int,
    height: int,
) -> None:
    supersampling = 2
    render_width, render_height = width * supersampling, height * supersampling
    scale_factor = min(render_width / 140.0, render_height / 145.0)
    center = np.array([18.0, -18.0, 57.0])
    view = np.array([1.0, -1.25, 0.82])
    view /= np.linalg.norm(view)
    right = np.cross(np.array([0.0, 0.0, 1.0]), view)
    right /= np.linalg.norm(right)
    up = np.cross(view, right)
    light = np.array([-0.35, -0.25, 1.0])
    light /= np.linalg.norm(light)
    part_draw_lists = []

    for vertices, triangles, color, distance in meshes:
        draw_list = []
        moved = vertices.copy()
        moved[:, 2] += distance * amount
        moved[:, 0] += math.sin(amount * math.pi) * distance * 0.055
        relative = moved - center
        projected = np.column_stack((relative @ right, relative @ up)) * scale_factor
        projected[:, 0] += render_width / 2
        projected[:, 1] = render_height / 2 - projected[:, 1]
        faces = moved[triangles]
        normals = np.cross(faces[:, 1] - faces[:, 0], faces[:, 2] - faces[:, 0])
        lengths = np.linalg.norm(normals, axis=1)
        valid = lengths > 1e-9
        normals[valid] /= lengths[valid, None]
        brightness = np.clip(0.42 + 0.58 * np.abs(normals @ light), 0.0, 1.0)
        depths = np.mean(relative[triangles] @ view, axis=1)
        for index, triangle in enumerate(triangles):
            shade = tuple(np.clip(color * brightness[index], 0, 255).astype(int))
            points = [tuple(projected[vertex]) for vertex in triangle]
            draw_list.append((depths[index], points, shade))
        part_draw_lists.append(draw_list)

    image = Image.new("RGB", (render_width, render_height), (18, 22, 28))
    draw = ImageDraw.Draw(image)
    # Internal parts are behind the enclosure skin in the assembled state.
    # Explicit body layering avoids centroid-order artifacts where intersecting
    # CAD bodies share nearly the same projected depth.
    for part_index in (1, 2, 0, 3):
        for _, points, color in sorted(part_draw_lists[part_index], key=lambda item: item[0]):
            draw.polygon(points, fill=color)
    image.resize((width, height), Image.Resampling.LANCZOS).save(output, optimize=True)


def require_tool(name: str) -> str:
    executable = shutil.which(name)
    if not executable:
        raise SystemExit(f"Required executable not found: {name}")
    return executable


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path("SmartHomeCube-assembly.mp4"))
    parser.add_argument("--fps", type=int, default=24)
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    args = parser.parse_args()

    ffmpeg = require_tool("ffmpeg")
    model_dir = Path(__file__).resolve().parent
    args.output = args.output.resolve()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    frame_count = math.ceil(6.7 * args.fps)

    with tempfile.TemporaryDirectory(prefix="smarthomecube-animation-") as temp_name:
        temp_dir = Path(temp_name)
        freecadcmd = find_freecadcmd(temp_dir)
        export_freecad_bodies(model_dir / "src/SmartHomeCube.FCStd", temp_dir, freecadcmd)
        meshes = [
            (*load_stl(temp_dir / (object_name + ".stl")), np.asarray(color) * 255, distance)
            for object_name, _, color, distance in PARTS
        ]
        for frame in range(frame_count):
            amount = explosion_amount(frame, args.fps)
            png_path = temp_dir / f"frame-{frame:04d}.png"
            render_frame(meshes, amount, png_path, args.width, args.height)
            print(f"Rendered {frame + 1}/{frame_count}", end="\r", flush=True)

        subprocess.run(
            (
                ffmpeg,
                "-y",
                "-framerate",
                str(args.fps),
                "-i",
                str(temp_dir / "frame-%04d.png"),
                "-c:v",
                "libx264",
                "-preset",
                "medium",
                "-crf",
                "18",
                "-pix_fmt",
                "yuv420p",
                "-movflags",
                "+faststart",
                str(args.output),
            ),
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    print(f"\nCreated {args.output}")


if __name__ == "__main__":
    main()
