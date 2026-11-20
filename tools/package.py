# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

root = Path(__file__).resolve().parents[1]
build = root / "build"
name = "TTWVRPhysicalCanteen"
with ZipFile(build / "TTWVR-Physical-Canteen-0.1.2.zip", "w", ZIP_DEFLATED) as archive:
    archive.write(build / f"{name}.esp", f"{name}.esp")
    archive.write(build / f"{name}.dll", f"NVSE/Plugins/{name}.dll")
    for file in ("README.md", "LICENSE"):
        archive.write(root / file, file)
