# SPDX-License-Identifier: GPL-3.0-only
import struct
import sys
from pathlib import Path
from unittest import TestCase, main

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import item


def records(data):
    pos = 0
    while pos < len(data):
        tag, size, _, form = struct.unpack_from("<4sIII", data, pos)
        if tag == b"GRUP":
            yield from records(data[pos + 24:pos + size])
            pos += size
            continue
        end = pos + 24 + size
        pos += 24
        fields = []
        while pos < end:
            key, length = struct.unpack_from("<4sH", data, pos)
            pos += 6
            fields.append((key, data[pos:pos + length]))
            pos += length
        assert pos == end
        yield tag, form, fields


class CanteenRecords(TestCase):
    def test_local_abilities_and_stable_item_ids(self):
        rows = list(records(item.build()))
        self.assertEqual([t for t, _, _ in rows], [b"TES4", b"SPEL", b"SPEL", b"SPEL", b"STAT", b"WEAP", b"IMAD"])
        self.assertEqual(struct.unpack("<fII", dict(rows[0][2])[b"HEDR"])[1:], (10, 0x806))
        self.assertEqual(dict(rows[5][2])[b"WNAM"], struct.pack("<I", rows[4][1]))
        self.assertEqual([r[1] for r in rows[4:6]], [0x01000801, 0x01000800])
        for level, (_, form, fields) in enumerate(rows[1:4], 1):
            self.assertEqual(form, 0x01000801 + level)
            self.assertEqual(struct.unpack("<4I", dict(fields)[b"SPIT"]), (4, 0, 0, 0))
            effects = [struct.unpack("<I", v)[0] for k, v in fields if k == b"EFID"]
            self.assertEqual(effects, [0x66989, 0x66988, 0x66EB2])
            values = [struct.unpack("<5I", v) for k, v in fields if k == b"EFIT"]
            self.assertEqual(values, [(level, 0, 0, 0, 9), (1, 0, 0, 0, 5), (1, 0, 0, 0, 8)])

    def test_private_blur_has_neutral_non_blur_channels(self):
        tag, form, fields = list(records(item.build()))[-1]
        self.assertEqual((tag, form), (b"IMAD", 0x01000805))
        data = dict(fields)
        counts = struct.unpack("<61I", data[b"DNAM"])
        self.assertEqual((counts[0], counts[45]), (0, 2))
        tags = [bytes([n + add]) + b"IAD" for n in range(21) for add in (0, 64)]
        indices = dict(zip(tags, range(2, 44)))
        indices.update({b"TNAM":44, b"BNAM":45, b"VNAM":46, b"RNAM":47, b"SNAM":48,
                        b"UNAM":49, b"WNAM":53, b"XNAM":54, b"YNAM":55, b"NAM1":57,
                        b"NAM2":58, b"NAM3":59, b"NAM4":60})
        self.assertEqual(set(data), set(indices) | {b"DNAM", b"EDID"})
        for key, idx in indices.items():
            color = key in (b"TNAM", b"NAM3")
            width = 5 if color else 2
            self.assertEqual(len(data[key]), counts[idx] * width * 4)
            floats = struct.unpack(f"<{2*width}f", data[key])
            value = 2.4 if key == b"BNAM" else int(key in tags and key[0] < 64)
            self.assertEqual(floats[0::width], (0., 1.))
            for k in range(2):
                for v in floats[k*width+1:(k+1)*width]:
                    self.assertAlmostEqual(v, value, places=5)


if __name__ == "__main__":
    main()
