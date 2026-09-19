"""Vendors the randomizer data into the apworld and packages tp_dusklight.apworld.

usage: python tools/build_apworld.py [--install <Archipelago worlds dir>] [--out <file.apworld>]
"""
from __future__ import annotations

import argparse
import json
import shutil
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "apworld" / "tp_dusklight"
DATA = ROOT / "generator" / "data"
FILES = ["items.yaml", "locations.yaml", "macros.yaml", "settings_list.yaml"]


def sync_data() -> None:
    dst = SRC / "data"
    if dst.exists():
        shutil.rmtree(dst)
    dst.mkdir(parents=True)
    for f in FILES:
        shutil.copyfile(DATA / f, dst / f)
    shutil.copytree(DATA / "world", dst / "world")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--install", type=Path, help="copy the world folder into this worlds/ dir")
    ap.add_argument("--out", type=Path, default=ROOT / "build" / "tp_dusklight.apworld")
    args = ap.parse_args()

    sync_data()
    manifest = json.loads((SRC / "archipelago.json").read_text(encoding="utf-8"))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.out, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("archipelago.json", json.dumps(manifest, indent=2))
        for path in sorted(SRC.rglob("*")):
            if path.is_dir() or "__pycache__" in path.parts:
                continue
            z.write(path, Path("tp_dusklight") / path.relative_to(SRC))
    print(f"wrote {args.out}")

    if args.install:
        target = args.install / "tp_dusklight"
        if target.exists():
            shutil.rmtree(target)
        shutil.copytree(SRC, target, ignore=shutil.ignore_patterns("__pycache__"))
        print(f"installed into {target}")


if __name__ == "__main__":
    main()
