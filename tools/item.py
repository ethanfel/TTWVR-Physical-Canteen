# SPDX-License-Identifier: GPL-3.0-only
"""Build the physical canteen and its temporary beer-style abilities."""
from pathlib import Path
from struct import pack, pack_into

MESH = r"NVDLCPre1\clutter\canteen\v13canteen.NIF"
ICON = r"NVDLCPre1\interface\icons\pipboyimages\items\item_vault13_canteen.dds"
HELD, FIRST_PERSON = 0x01000800, 0x01000801


def string(value):
    return value.encode("ascii") + b"\0"


def record(tag, number, fields, flags=0):
    data = b"".join(pack("<4sH", name, len(value)) + value for name, value in fields)
    return pack("<4sIIIIHH", tag, len(data), flags, number, 0, 15, 0) + data


def group(tag, data):
    return pack("<4sI4sIII", b"GRUP", 24 + len(data), tag, 0, 0, 0) + data


def build():
    header = record(b"TES4", 0, [
        (b"HEDR", pack("<fII", 1.34, 8, 0x805)), (b"CNAM", string("TTWVR Physical Canteen")),
        (b"MAST", string("FalloutNV.esm")), (b"DATA", bytes(8)),
    ])
    bounds = pack("<6h", -7, -2, 0, 7, 3, 14)
    model = record(b"STAT", FIRST_PERSON, [
        (b"EDID", string("TTWVRFirstPersonCanteen")), (b"OBND", bounds), (b"MODL", string(MESH)),
    ])
    # WEAP gives the tool the game's normal ready, holster and hotkey handling.
    # One-hand animation, no ammo, zero attack reach and damage, quiet, player only.
    tuning = bytearray(204)
    pack_into("<IffBBBB", tuning, 0, 1, 1.0, 0.0, 0x08, 255, 0, 0)
    tuning[41] = 255
    pack_into("<IIf", tuning, 52, 3, 0x41, 1.0)
    pack_into("<i", tuning, 104, 38)  # Melee Weapons actor value
    pack_into("<f", tuning, 116, 1.0)  # Limb multiplier
    pack_into("<i", tuning, 120, -1)  # No resistance actor value
    tool = record(b"WEAP", HELD, [
        (b"EDID", string("TTWVRHeldCanteen")), (b"OBND", bounds),
        (b"FULL", string("Vault 13 Canteen (Drink)")), (b"MODL", string(MESH)),
        (b"ICON", string(ICON)), (b"ETYP", pack("<I", 3)),
        (b"WNAM", pack("<I", FIRST_PERSON)), (b"DATA", pack("<IIfHB", 0, 100, 0.0, 0, 0)),
        (b"DNAM", tuning), (b"CRDT", bytes(16)), (b"VNAM", pack("<I", 2)),
    ], flags=0x400)  # Quest item: the zero-weight representation stays with the original.
    # Use the three SPECIAL effects from FalloutNV.esm's Beer (00015197).
    # Ability duration is controlled by the canteen's recovery timer.
    abilities = []
    for level, label in enumerate(("Tipsy", "Drunk", "Very Drunk"), start=1):
        fields = [(b"EDID", string(f"TTWVRCanteenBuzz{level}")),
                  (b"FULL", string(f"Canteen - {label}")),
                  (b"SPIT", pack("<4I", 4, 0, 0, 0))]
        for effect, magnitude, actor_value in (
                (0x66989, level, 9), (0x66988, 1, 5), (0x66EB2, 1, 8)):
            fields.extend(((b"EFID", pack("<I", effect)),
                           (b"EFIT", pack("<5I", magnitude, 0, 0, 0, actor_value))))
        abilities.append(record(b"SPEL", FIRST_PERSON + level, fields))
    return header + group(b"SPEL", b"".join(abilities)) + group(b"STAT", model) + group(b"WEAP", tool)


if __name__ == "__main__":
    import sys
    Path(sys.argv[1]).write_bytes(build())
