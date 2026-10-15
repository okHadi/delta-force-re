"""Synthetic mission/archive links. No game assets are used."""

from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from mission import MissionError, Resolver, map_names, terrain_name


def bms(name=b'TEST.TRN'):
    data = bytearray(100)
    data[:4] = b'BMS\r'
    data[84:84 + len(name)] = name
    return bytes(data)


def pcx():
    data = bytearray(128)
    data[:4] = bytes((10, 5, 1, 8))
    data[65] = 1
    struct.pack_into('<H', data, 66, 2)
    return bytes(data) + b'\x01\x00'


TRN = b'color_map TEST_C\nelev_map TEST_D\ndetail_map TEST_M\n'


def archive(path, members):
    records = []
    body = bytearray()
    for name, data in members:
        records.append(struct.pack('<IIII16s', 0, 20 + len(body), len(data), 0, name.encode()))
        body.extend(data)
    path.write_bytes(struct.pack('<I4sIII', 20, b'PFF3', len(records), 32, 20 + len(body))
                     + body + b''.join(records))


class MissionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.maps = [('TEST_C.PCX', pcx()), ('TEST_D.PCX', pcx()), ('TEST_M.PCX', pcx())]

    def resolver(self, extra=(), trn=TRN):
        archive(self.folder / 'Df.pff', [('TEST.TRN', trn)] + self.maps + list(extra))
        archive(self.folder / 'Dfbase.pff', [('TEST.BMS', bms())])
        return Resolver(self.folder)

    def test_link_resolution(self):
        result = self.resolver().resolve('test.bms')
        self.assertEqual(result['terrain']['member'], 'TEST.TRN')
        self.assertEqual(result['maps']['elev_map']['member'], 'TEST_D.PCX')
        self.assertEqual(result['maps']['elev_map']['width'], 1)
        self.assertEqual(result['maps']['elev_map']['height'], 1)

    def test_invalid_bms(self):
        for data in (b'', bms()[:99], b'NOPE' + bms()[4:],
                     bms(b'../BAD.TRN'), bms(b'BAD.PCX'), bms(b'\xff.TRN'),
                     bms(b'A' * 16)):
            with self.subTest(data=data[84:]), self.assertRaises(MissionError):
                terrain_name(data)

    def test_invalid_trn(self):
        for data in (b'', TRN.replace(b'elev_map TEST_D\n', b''),
                     TRN + b'color_map OTHER\n', TRN.replace(b'TEST_C', b'../BAD'),
                     TRN.replace(b'TEST_C', b'"TEST_C"'), TRN + b'\xff',
                     TRN + b'\0', TRN + b'\x01', b'A' * 65537):
            with self.subTest(data=data[:20]), self.assertRaises(MissionError):
                map_names(data)

    def test_comments(self):
        self.assertEqual(map_names(TRN + b'// color_map OTHER\n')['color_map'], 'TEST_C.PCX')

    def test_missing_and_duplicate_members(self):
        resolver = self.resolver(extra=[('test_c.pcx', pcx())])
        with self.assertRaises(MissionError):
            resolver.resolve('TEST.BMS')
        self.maps.pop()
        with self.assertRaises(MissionError):
            self.resolver().resolve('TEST.BMS')

    def test_invalid_selected_name(self):
        resolver = self.resolver()
        for name in ('../TEST.BMS', 'TEST.TRN', 'NONE.BMS'):
            with self.subTest(name=name), self.assertRaises(MissionError):
                resolver.resolve(name)


if __name__ == '__main__':
    unittest.main()
