#!/usr/bin/env python3
"""Generate small deterministic OBJ meshes for feature-detection regression tests."""

from __future__ import annotations

import math
from pathlib import Path


ROOT = Path(__file__).resolve().parent


class ObjMesh:
    def __init__(self) -> None:
        self.vertices: list[tuple[float, float, float]] = []
        self.faces: list[tuple[int, int, int]] = []
        self._ids: dict[tuple[float, float, float], int] = {}

    def vertex(self, x: float, y: float, z: float) -> int:
        key = (round(x, 12), round(y, 12), round(z, 12))
        found = self._ids.get(key)
        if found is not None:
            return found
        self.vertices.append(key)
        idx = len(self.vertices)
        self._ids[key] = idx
        return idx

    def tri(self, a: int, b: int, c: int) -> None:
        if a != b and b != c and a != c:
            self.faces.append((a, b, c))

    def quad(self, a: int, b: int, c: int, d: int) -> None:
        self.tri(a, b, c)
        self.tri(a, c, d)

    def write(self, path: Path, header: list[str]) -> None:
        with path.open("w", encoding="ascii", newline="\n") as out:
            for line in header:
                out.write(f"# {line}\n")
            for x, y, z in self.vertices:
                out.write(f"v {x:.12g} {y:.12g} {z:.12g}\n")
            for a, b, c in self.faces:
                out.write(f"f {a} {b} {c}\n")


def ring(center: tuple[float, float], radius: float, z: float, segments: int) -> list[tuple[float, float, float]]:
    cx, cy = center
    return [
        (
            cx + radius * math.cos(2.0 * math.pi * i / segments),
            cy + radius * math.sin(2.0 * math.pi * i / segments),
            z,
        )
        for i in range(segments)
    ]


def dot(a: tuple[float, float, float], b: tuple[float, float, float]) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a: tuple[float, float, float], b: tuple[float, float, float]) -> tuple[float, float, float]:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def normalize(v: tuple[float, float, float]) -> tuple[float, float, float]:
    length = math.sqrt(dot(v, v))
    return (v[0] / length, v[1] / length, v[2] / length)


def add(
    a: tuple[float, float, float], b: tuple[float, float, float]
) -> tuple[float, float, float]:
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def scale(v: tuple[float, float, float], s: float) -> tuple[float, float, float]:
    return (v[0] * s, v[1] * s, v[2] * s)


def local_frame(axis: tuple[float, float, float]) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    n = normalize(axis)
    seed = (0.0, 1.0, 0.0) if abs(n[0]) > 0.8 else (1.0, 0.0, 0.0)
    u = normalize(cross(seed, n))
    v = normalize(cross(n, u))
    return u, v


def ring_on_plane(
    center: tuple[float, float, float],
    u: tuple[float, float, float],
    v: tuple[float, float, float],
    radius: float,
    segments: int,
) -> list[tuple[float, float, float]]:
    points = []
    for i in range(segments):
        angle = 2.0 * math.pi * i / segments
        points.append(
            add(
                center,
                add(scale(u, radius * math.cos(angle)), scale(v, radius * math.sin(angle))),
            )
        )
    return points


def ellipse_on_plane(
    center: tuple[float, float, float],
    u: tuple[float, float, float],
    v: tuple[float, float, float],
    major_radius: float,
    minor_radius: float,
    segments: int,
) -> list[tuple[float, float, float]]:
    points = []
    for i in range(segments):
        angle = 2.0 * math.pi * i / segments
        points.append(
            add(
                center,
                add(
                    scale(u, major_radius * math.cos(angle)),
                    scale(v, minor_radius * math.sin(angle)),
                ),
            )
        )
    return points


def annular_hole_plate(
    path: Path,
    *,
    top_hole_center: tuple[float, float],
    bottom_hole_center: tuple[float, float],
    outer_radius: float,
    hole_radius: float,
    half_thickness: float,
    segments: int,
    name: str,
) -> None:
    mesh = ObjMesh()
    top_outer = [mesh.vertex(*p) for p in ring((0.0, 0.0), outer_radius, half_thickness, segments)]
    top_inner = [mesh.vertex(*p) for p in ring(top_hole_center, hole_radius, half_thickness, segments)]
    bottom_outer = [mesh.vertex(*p) for p in ring((0.0, 0.0), outer_radius, -half_thickness, segments)]
    bottom_inner = [mesh.vertex(*p) for p in ring(bottom_hole_center, hole_radius, -half_thickness, segments)]

    for i in range(segments):
        j = (i + 1) % segments
        mesh.quad(top_outer[i], top_outer[j], top_inner[j], top_inner[i])
        mesh.quad(bottom_outer[i], bottom_inner[i], bottom_inner[j], bottom_outer[j])
        mesh.quad(top_outer[i], bottom_outer[i], bottom_outer[j], top_outer[j])
        mesh.quad(top_inner[i], top_inner[j], bottom_inner[j], bottom_inner[i])

    mesh.write(
        path,
        [
            f"{name}.obj",
            "units: arbitrary",
            f"outer_radius={outer_radius} hole_radius={hole_radius} half_thickness={half_thickness} segments={segments}",
            f"top_hole_center={top_hole_center} bottom_hole_center={bottom_hole_center}",
        ],
    )


def elliptical_hole_plate(
    path: Path,
    *,
    major_radius: float,
    minor_radius: float,
    half_thickness: float,
    outer_radius: float,
    segments: int,
    name: str,
) -> None:
    mesh = ObjMesh()
    u = (1.0, 0.0, 0.0)
    v = (0.0, 1.0, 0.0)
    top_center = (0.0, 0.0, half_thickness)
    bottom_center = (0.0, 0.0, -half_thickness)
    top_outer = [mesh.vertex(*p) for p in ring_on_plane(top_center, u, v, outer_radius, segments)]
    bottom_outer = [mesh.vertex(*p) for p in ring_on_plane(bottom_center, u, v, outer_radius, segments)]
    top_inner = [
        mesh.vertex(*p)
        for p in ellipse_on_plane(top_center, u, v, major_radius, minor_radius, segments)
    ]
    bottom_inner = [
        mesh.vertex(*p)
        for p in ellipse_on_plane(bottom_center, u, v, major_radius, minor_radius, segments)
    ]

    for i in range(segments):
        j = (i + 1) % segments
        mesh.quad(top_outer[i], top_outer[j], top_inner[j], top_inner[i])
        mesh.quad(bottom_outer[i], bottom_inner[i], bottom_inner[j], bottom_outer[j])
        mesh.quad(top_outer[i], bottom_outer[i], bottom_outer[j], top_outer[j])
        mesh.quad(top_inner[i], top_inner[j], bottom_inner[j], bottom_inner[i])

    mesh.write(
        path,
        [
            f"{name}.obj",
            "units: arbitrary",
            f"major_radius={major_radius} minor_radius={minor_radius} half_thickness={half_thickness} segments={segments}",
        ],
    )


def tilted_annular_hole_plate(path: Path) -> None:
    mesh = ObjMesh()
    segments = 32
    outer_radius = 1.8
    hole_radius = 0.5
    half_thickness = 0.65
    axis = normalize((0.35, 0.2, 1.0))
    u, v = local_frame(axis)
    top_center = scale(axis, half_thickness)
    bottom_center = scale(axis, -half_thickness)

    top_outer = [mesh.vertex(*p) for p in ring_on_plane(top_center, u, v, outer_radius, segments)]
    top_inner = [mesh.vertex(*p) for p in ring_on_plane(top_center, u, v, hole_radius, segments)]
    bottom_outer = [mesh.vertex(*p) for p in ring_on_plane(bottom_center, u, v, outer_radius, segments)]
    bottom_inner = [mesh.vertex(*p) for p in ring_on_plane(bottom_center, u, v, hole_radius, segments)]

    for i in range(segments):
        j = (i + 1) % segments
        mesh.quad(top_outer[i], top_outer[j], top_inner[j], top_inner[i])
        mesh.quad(bottom_outer[i], bottom_inner[i], bottom_inner[j], bottom_outer[j])
        mesh.quad(top_outer[i], bottom_outer[i], bottom_outer[j], top_outer[j])
        mesh.quad(top_inner[i], top_inner[j], bottom_inner[j], bottom_inner[i])

    mesh.write(
        path,
        [
            "tilted_coaxial_hole_plate.obj",
            "units: arbitrary; local circular faces are perpendicular to the tilted bore axis",
            f"axis=({axis[0]:.12g},{axis[1]:.12g},{axis[2]:.12g})",
            f"outer_radius={outer_radius} hole_radius={hole_radius} half_thickness={half_thickness} segments={segments}",
        ],
    )


def stepped_boss_pocket(path: Path) -> None:
    mesh = ObjMesh()
    xs = [-1.5, -0.8, -0.2, 0.25, 0.85, 1.5]
    ys = [-1.0, -0.35, 0.35, 1.0]
    bottom = -0.45

    def top_z(ix: int, iy: int) -> float:
        cx = 0.5 * (xs[ix] + xs[ix + 1])
        cy = 0.5 * (ys[iy] + ys[iy + 1])
        if -0.8 < cx < -0.2 and -0.35 < cy < 0.35:
            return 0.7
        if 0.25 < cx < 0.85 and -0.35 < cy < 0.35:
            return -0.25
        return 0.2

    heights = [[top_z(ix, iy) for iy in range(len(ys) - 1)] for ix in range(len(xs) - 1)]

    for ix in range(len(xs) - 1):
        for iy in range(len(ys) - 1):
            z = heights[ix][iy]
            a = mesh.vertex(xs[ix], ys[iy], z)
            b = mesh.vertex(xs[ix + 1], ys[iy], z)
            c = mesh.vertex(xs[ix + 1], ys[iy + 1], z)
            d = mesh.vertex(xs[ix], ys[iy + 1], z)
            mesh.quad(a, b, c, d)

            ab = mesh.vertex(xs[ix], ys[iy], bottom)
            bb = mesh.vertex(xs[ix + 1], ys[iy], bottom)
            cb = mesh.vertex(xs[ix + 1], ys[iy + 1], bottom)
            db = mesh.vertex(xs[ix], ys[iy + 1], bottom)
            mesh.quad(ab, db, cb, bb)

            if ix == 0:
                mesh.quad(mesh.vertex(xs[ix], ys[iy], z), mesh.vertex(xs[ix], ys[iy + 1], z), db, ab)
            if ix == len(xs) - 2:
                mesh.quad(mesh.vertex(xs[ix + 1], ys[iy], z), bb, cb, mesh.vertex(xs[ix + 1], ys[iy + 1], z))
            if iy == 0:
                mesh.quad(mesh.vertex(xs[ix], ys[iy], z), ab, bb, mesh.vertex(xs[ix + 1], ys[iy], z))
            if iy == len(ys) - 2:
                mesh.quad(mesh.vertex(xs[ix], ys[iy + 1], z), mesh.vertex(xs[ix + 1], ys[iy + 1], z), cb, db)

            if ix + 1 < len(xs) - 1:
                nz = heights[ix + 1][iy]
                if abs(z - nz) > 1e-12:
                    x = xs[ix + 1]
                    low, high = sorted((z, nz))
                    mesh.quad(
                        mesh.vertex(x, ys[iy], low),
                        mesh.vertex(x, ys[iy + 1], low),
                        mesh.vertex(x, ys[iy + 1], high),
                        mesh.vertex(x, ys[iy], high),
                    )
            if iy + 1 < len(ys) - 1:
                nz = heights[ix][iy + 1]
                if abs(z - nz) > 1e-12:
                    y = ys[iy + 1]
                    low, high = sorted((z, nz))
                    mesh.quad(
                        mesh.vertex(xs[ix], y, low),
                        mesh.vertex(xs[ix + 1], y, low),
                        mesh.vertex(xs[ix + 1], y, high),
                        mesh.vertex(xs[ix], y, high),
                    )

    mesh.write(
        path,
        [
            "boss_pocket_plate.obj",
            "units: arbitrary; base_top=0.2 boss_top=0.7 pocket_floor=-0.25 bottom=-0.45",
            "contains one rectangular raised boss and one rectangular recessed pocket",
        ],
    )


def main() -> None:
    tilted_annular_hole_plate(ROOT / "tilted_coaxial_hole_plate.obj")
    elliptical_hole_plate(
        ROOT / "elliptical_hole_plate.obj",
        major_radius=0.8,
        minor_radius=0.45,
        half_thickness=0.5,
        outer_radius=2.0,
        segments=40,
        name="elliptical_hole_plate",
    )
    elliptical_hole_plate(
        ROOT / "near_circular_hole_plate.obj",
        major_radius=0.62,
        minor_radius=0.59,
        half_thickness=0.5,
        outer_radius=2.0,
        segments=40,
        name="near_circular_hole_plate",
    )
    annular_hole_plate(
        ROOT / "eccentric_hole_plate.obj",
        top_hole_center=(0.18, 0.0),
        bottom_hole_center=(-0.12, 0.0),
        outer_radius=2.0,
        hole_radius=0.55,
        half_thickness=0.5,
        segments=32,
        name="eccentric_hole_plate",
    )
    stepped_boss_pocket(ROOT / "boss_pocket_plate.obj")


if __name__ == "__main__":
    main()
