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
        self.assertEqual([t for t, _, _ in rows], [b"TES4", b"SPEL", b"SPEL", b"SPEL", b"STAT", b"WEAP"])
        self.assertEqual(struct.unpack("<fII", dict(rows[0][2])[b"HEDR"])[1:], (8, 0x805))
        self.assertEqual(dict(rows[-1][2])[b"WNAM"], struct.pack("<I", rows[-2][1]))
        self.assertEqual([r[1] for r in rows[-2:]], [0x01000801, 0x01000800])
        for level, (_, form, fields) in enumerate(rows[1:4], 1):
            self.assertEqual(form, 0x01000801 + level)
            self.assertEqual(struct.unpack("<4I", dict(fields)[b"SPIT"]), (4, 0, 0, 0))
            effects = [struct.unpack("<I", v)[0] for k, v in fields if k == b"EFID"]
            self.assertEqual(effects, [0x66989, 0x66988, 0x66EB2])
            values = [struct.unpack("<5I", v) for k, v in fields if k == b"EFIT"]
            self.assertEqual(values, [(level, 0, 0, 0, 9), (1, 0, 0, 0, 5), (1, 0, 0, 0, 8)])


if __name__ == "__main__":
    main()
