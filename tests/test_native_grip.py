# SPDX-License-Identifier: GPL-3.0-only
import math
import struct
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from native_grip import ITEM, with_canteen_grip


def resource_pack(resources):
    header = bytearray(struct.pack("<4sII", b"TVPK", 1, len(resources)))
    offset = len(header) + sum(10 + len(key) for key in resources)
    payload = bytearray()
    for key, data in resources.items():
        header += struct.pack("<H", len(key)) + key
        header += struct.pack("<II", offset + len(payload), len(data))
        payload += data
    return header + payload


class NativeGrip(unittest.TestCase):
    def test_held_and_squeezed_curls_survive_host_melee_overrides(self):
        table = b"g 0 1hm 0\nk weapknife 0\n"
        melee = b"for 20\nset 1hm\nBip01 R Finger11 0.000651 0.000219 0.665762 0.746164\nset 2hm\n"
        resource = resource_pack({b"grips-pose.txt": table, b"melee-fingers.txt": melee})
        poses = (ROOT / "assets/canteen-grip.txt").read_bytes()
        result = with_canteen_grip(bytes(resource), poses)
        offset, size = struct.unpack_from("<II", result, 14 + len(b"grips-pose.txt"))
        augmented = result[offset:offset + size]
        self.assertTrue(augmented.startswith(table))
        group, *rest = augmented[len(table):].splitlines()
        _, number, name, _ = group.split()
        self.assertNotIn(b"set " + name, melee.splitlines())
        self.assertEqual(rest[-1].split(), [b"k", ITEM, number])
        for mode in (b"h", b"f"):
            entries = [r.split()[1:] for r in rest if r.startswith(mode + b" ")]
            self.assertEqual(len(entries), 30)
            for side in (b"L", b"R"):
                q, = [row[3:] for row in entries if row[:3] == [b"Bip01", side, b"Finger11"]]
                angle = 2 * math.degrees(math.acos(float(q[3])))
                self.assertAlmostEqual(angle, 45.15, places=2)


if __name__ == "__main__":
    unittest.main()
