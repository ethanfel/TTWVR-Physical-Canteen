# SPDX-License-Identifier: GPL-3.0-only
import os
from pathlib import Path
from native_grip import with_canteen_grip
from zipfile import ZIP_DEFLATED, ZipFile

root = Path(__file__).resolve().parents[1]
build = root / "build"
name = "TTWVRPhysicalCanteen"
source = os.environ.get("TTWVR_DATA")
if not source:
    raise SystemExit("Set TTWVR_DATA to the installed TTWVR 1.1.7 TTWVR.dat to build the release ZIP.")
native = with_canteen_grip(Path(source).read_bytes(), (root / "assets/canteen-grip.txt").read_bytes())
with ZipFile(build / "TTWVR-Physical-Canteen-0.1.12.zip", "w", ZIP_DEFLATED) as archive:
    archive.write(build / f"{name}.esp", f"{name}.esp")
    archive.write(build / f"{name}.dll", f"NVSE/Plugins/{name}.dll")
    archive.write(root / f"assets/{name}.ini", f"NVSE/Plugins/{name}.ini")
    archive.writestr("NVSE/Plugins/TTWVR/TTWVR.dat", native)
    for file in ("README.md", "LICENSE"):
        archive.write(root / file, file)
