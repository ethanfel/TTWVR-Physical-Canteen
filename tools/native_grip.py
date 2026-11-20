# SPDX-License-Identifier: GPL-3.0-only
"""Add the canteen's measured hold to a local TTWVR 1.1.7 resource pack."""
import struct

TABLE = b"grips-pose.txt"
ITEM = b"ttwvrheldcanteen"
# The host replaces matching set names in both hold and trigger poses with
# melee-fingers.txt after parsing grips-pose.txt. This is a pose-table label.
CANTEEN_SET = b"canteen"


def with_canteen_grip(pack, finger_rows):
    magic, version, count = struct.unpack_from("<4sII", pack)
    if (magic, version) != (b"TVPK", 1):
        raise ValueError("Unsupported TTWVR resource pack")
    cursor = 12
    entries = {}
    for _ in range(count):
        length, = struct.unpack_from("<H", pack, cursor)
        cursor += 2
        name = pack[cursor:cursor + length]
        cursor += length
        pos, size = struct.unpack_from("<II", pack, cursor)
        if name in entries or pos + size > len(pack):
            raise ValueError("Invalid TTWVR resource directory")
        entries[name] = (cursor, pos, size)
        cursor += 8
    if TABLE not in entries or any(offset < cursor for _, offset, _ in entries.values()):
        raise ValueError("Missing grip table or invalid resource offset")
    entry, offset, size = entries[TABLE]
    table = pack[offset:offset + size]
    lines = table.splitlines()
    groups = [int(line.split()[1]) for line in lines if line.startswith(b"g ")]
    if not groups or any(line.lower().split()[:2] == [b"k", ITEM] for line in lines):
        raise ValueError("Expected an original grip table without a canteen entry")
    group = str(max(groups) + 1).encode()
    bones = [row.split() for row in finger_rows.splitlines() if row.strip()]
    if len(bones) != 30 or len({tuple(row[:3]) for row in bones}) != 30:
        raise ValueError("Expected 15 local finger rotations for each hand")

    if b"melee-fingers.txt" not in entries:
        raise ValueError("Missing TTWVR melee finger table")
    _, melee_start, melee_size = entries[b"melee-fingers.txt"]
    for line in pack[melee_start:melee_start + melee_size].splitlines():
        fields = line.lower().split()
        if fields == [b"set", CANTEEN_SET]:
            raise ValueError("The host has a melee override for the canteen pose set")
    extra = [b" ".join((b"g", group, CANTEEN_SET, b"0"))]
    for mode in (b"h", b"f"):
        extra.extend(b" ".join([mode, *bone]) for bone in bones)
    extra.append(b"k " + ITEM + b" " + group)
    augmented = table.rstrip(b"\r\n") + b"\n" + b"\n".join(extra) + b"\n"
    # Redirect this one directory entry. Every original resource stays at its original offset.
    result = bytearray(pack)
    struct.pack_into("<II", result, entry, len(pack), len(augmented))
    result.extend(augmented)
    return result
