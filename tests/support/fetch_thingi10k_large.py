"""Download reproducible large STL fixtures from the Thingi10K mirror.

The raw models remain outside the repository by default.  The generated JSON
manifest records the per-model license and digest so a local benchmark can be
audited without redistributing the source meshes.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import pathlib
import struct
import tempfile
from typing import Dict, List, Optional
import urllib.request


HF_ROOT = "https://huggingface.co/datasets/Thingi10K/Thingi10K/resolve/main"
GEOMETRY_URL = f"{HF_ROOT}/metadata/geometry_data.csv"
SUMMARY_URL = f"{HF_ROOT}/metadata/input_summary.csv"
USER_AGENT = "ManuMesh-large-mesh-validation"
HASH_BLOCK_BYTES = 1024 * 1024


def fetch_bytes(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request) as response:
        return response.read()


def read_csv(url: str) -> List[Dict[str, str]]:
    return list(csv.DictReader(fetch_bytes(url).decode("utf-8-sig").splitlines()))


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while True:
            block = source.read(HASH_BLOCK_BYTES)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def validate_binary_stl(path: pathlib.Path, expected_faces: int) -> None:
    with path.open("rb") as source:
        header = source.read(84)
    if len(header) != 84:
        raise RuntimeError(f"{path} is too small to contain a binary STL header")
    declared_faces = struct.unpack_from("<I", header, 80)[0]
    if declared_faces != expected_faces:
        raise RuntimeError(
            f"{path} declares {declared_faces} faces; metadata declares {expected_faces}"
        )
    required_bytes = 84 + declared_faces * 50
    if path.stat().st_size < required_bytes:
        raise RuntimeError(
            f"{path} is truncated: {path.stat().st_size} bytes, expected at least {required_bytes}"
        )


def download(url: str, destination: pathlib.Path) -> str:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        return sha256_file(destination)

    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with tempfile.NamedTemporaryFile(
        mode="wb", prefix=destination.name + ".", suffix=".part", dir=destination.parent, delete=False
    ) as temporary:
        temporary_path = pathlib.Path(temporary.name)
        try:
            with urllib.request.urlopen(request) as response:
                digest = hashlib.sha256()
                while True:
                    block = response.read(HASH_BLOCK_BYTES)
                    if not block:
                        break
                    temporary.write(block)
                    digest.update(block)
            temporary.flush()
            temporary.close()
            temporary_path.replace(destination)
            return digest.hexdigest()
        except Exception:
            temporary.close()
            try:
                temporary_path.unlink()
            except FileNotFoundError:
                pass
            raise


def choose_rows(min_faces: int, limit: Optional[int]) -> List[Dict[str, object]]:
    geometry = {
        int(row["file_id"]): row
        for row in read_csv(GEOMETRY_URL)
        if int(row.get("num_faces", 0)) >= min_faces
    }
    summary = {int(row["ID"]): row for row in read_csv(SUMMARY_URL)}
    selected = []
    for file_id, row in geometry.items():
        source = summary.get(file_id, {})
        link = source.get("Link", "")
        if not link.lower().endswith(".stl"):
            continue
        selected.append(
            {
                "file_id": file_id,
                "vertices": int(row["num_vertices"]),
                "faces": int(row["num_faces"]),
                "license": source.get("License", "unknown"),
                "source_url": link,
                "mirror_url": f"{HF_ROOT}/raw_meshes/{file_id}.stl?download=true",
                "closed": int(row.get("num_boundary_edges", 0)) == 0,
                "edge_manifold": int(row.get("edge_manifold", 0)) == 1,
            }
        )
    selected.sort(key=lambda item: (item["faces"], item["file_id"]), reverse=True)
    return selected[:limit] if limit is not None else selected


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=pathlib.Path, default=pathlib.Path("output/thingi10k_large"))
    parser.add_argument("--min-faces", type=int, default=2_000_000)
    parser.add_argument("--limit", type=int, default=3)
    args = parser.parse_args()

    rows = choose_rows(args.min_faces, args.limit)
    if not rows:
        raise SystemExit("no STL entries matched the requested face count")

    manifest = {
        "dataset": "Thingi10K",
        "metadata": {
            "geometry": GEOMETRY_URL,
            "input_summary": SUMMARY_URL,
        },
        "models": [],
    }
    for row in rows:
        filename = f"thingi10k_{row['file_id']}_{row['faces']}_faces.stl"
        path = args.output_dir / filename
        print(f"downloading {row['file_id']} ({row['faces']} faces) -> {path}")
        row["path"] = str(path)
        row["sha256"] = download(row["mirror_url"], path)
        validate_binary_stl(path, row["faces"])
        row["bytes"] = path.stat().st_size
        manifest["models"].append(row)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = args.output_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
    print(f"wrote {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
