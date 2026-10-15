"""Synthetic PCX fixtures. No game assets are used."""

import importlib.util
from pathlib import Path
import struct
import sys
import unittest

SPEC = importlib.util.spec_from_file_location('pcx', Path(__file__).parents[1] / 'tools/pcx.py')
pcx = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = pcx
SPEC.loader.exec_module(pcx)


def fixture(width=2, height=2, stride=2, body=b'\xc2\x07\x08\x09', palette=False):
    header = bytearray(128)
    header[:4] = bytes((10, 5, 1, 8))
    struct.pack_into('<HHHH', header, 4, 0, 0, width - 1, height - 1)
    header[65] = 1
    struct.pack_into('<H', header, 66, stride)
    return bytes(header) + body + (b'\x0c' + bytes(range(256)) * 3 if palette else b'')


class PcxTests(unittest.TestCase):
    def test_rle_and_palette(self):
        image = pcx.decode(fixture(palette=True))
        self.assertEqual((image.width, image.height), (2, 2))
        self.assertEqual(image.pixels, bytes((7, 7, 8, 9)))
        self.assertEqual(len(image.palette), 768)

    def test_row_padding(self):
        image = pcx.decode(fixture(width=1, body=b'\x01\x02\x03\x04'))
        self.assertEqual(image.pixels, b'\x01\x03')
        self.assertIsNone(image.palette)

    def test_bad_rle(self):
        for body in (b'', b'\xc2', b'\xc0\x01', b'\xc3\x01', b'\x01'):
            with self.subTest(body=body), self.assertRaises(pcx.PcxError):
                pcx.decode(fixture(body=body))

    def test_bad_headers(self):
        cases = [b'', b'\0' * 128, fixture()[:127],
                 fixture(width=3), fixture(stride=3),
                 fixture(width=32769, height=1, stride=32770),
                 fixture(width=32768, height=32768, stride=32768)]
        bad_bounds = bytearray(fixture())
        struct.pack_into('<HH', bad_bounds, 4, 3, 0)
        cases.append(bytes(bad_bounds))
        for data in cases:
            with self.subTest(data=data[:16]), self.assertRaises(pcx.PcxError):
                pcx.decode(data)

    def test_bad_palette(self):
        for tail in (b'X', b'\x0c' + b'\0' * 767, b'\x0b' + b'\0' * 768):
            with self.subTest(length=len(tail)), self.assertRaises(pcx.PcxError):
                pcx.decode(fixture() + tail)


if __name__ == '__main__':
    unittest.main()
