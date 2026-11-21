# SPDX-License-Identifier: GPL-3.0-only
"""Build the physical canteen and its drinking blur and compatibility records."""
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


def image_modifier(form_id, editor_id, *, blur=0, refocus=False):
    # Non-animated FNV IMAD. Static channels use two equal keys.
    # Multiplicative channels are identity and additive channels are zero.
    counts = [2] * 61
    for idx in (0, 1, 50, 51, 52, 56):
        counts[idx] = 0
    if refocus:
        # JIP updates the live value and first key for trait 46. One key
        # keeps the running modifier at that value until the next update.
        counts[46] = 1
    data = bytearray(pack("<61I", *counts))
    pack_into("<2f", data, 51 * 4, 0.5, 0.5)
    fields = [(b"EDID", string(editor_id)), (b"DNAM", data)]
    def curve(tag, value):
        fields.append((tag, pack("<4f", 0, value, 1, value)))
    curve(b"BNAM", blur)
    if refocus:
        fields.append((b"VNAM", pack("<2f", 0, 0)))
    else:
        curve(b"VNAM", 0)
    for tag in (b"TNAM", b"NAM3"):
        fields.append((tag, pack("<10f", 0, 0, 0, 0, 0, 1, 0, 0, 0, 0)))
    for tag in (b"RNAM", b"SNAM", b"UNAM", b"NAM1", b"NAM2", b"WNAM", b"XNAM", b"YNAM", b"NAM4"):
        curve(tag, 0)
    for channel in range(21):
        curve(bytes([channel]) + b"IAD", 1)
        curve(bytes([channel + 64]) + b"IAD", 0)
    return record(b"IMAD", form_id, fields)


def build():
    header = record(b"TES4", 0, [
        (b"HEDR", pack("<fII", 1.34, 11, 0x807)), (b"CNAM", string("TTWVR Physical Canteen")),
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
    # Keep the original ability IDs and effect lists so removing a 0.1.10
    # ability on load can unwind its existing active effects. Never grant these.
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
    blur = image_modifier(0x01000805, "TTWVRCanteenBlur", blur=2.4)
    double_vision = image_modifier(0x01000806, "TTWVRCanteenDoubleVision", refocus=True)
    return (header + group(b"SPEL", b"".join(abilities)) + group(b"STAT", model)
            + group(b"WEAP", tool) + group(b"IMAD", blur + double_vision))


if __name__ == "__main__":
    import sys
    Path(sys.argv[1]).write_bytes(build())
