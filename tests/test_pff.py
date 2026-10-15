"""Synthetic PFF3 tests. No game assets are used."""

import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('pff', Path(__file__).parents[1] / 'tools/pff.py')
pff = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = pff
SPEC.loader.exec_module(pff)


def fixture(name=b'TEST.BIN', flags=0, offset=20, size=4):
    return (struct.pack('<I4sIII', 20, b'PFF3', 1, 32, 24) + b'test'
            + struct.pack('<IIII16s', flags, offset, size, 123, name))


class PffTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.source = Path(self.temp.name) / 'sample.pff'
        self.output = Path(self.temp.name) / 'out.bin'

    def read(self, data):
        self.source.write_bytes(data)
        return pff.Archive(self.source)

    def test_list_and_extract(self):
        archive = self.read(fixture())
        self.assertEqual(archive.manifest()['entries'][0], {
            'name': 'TEST.BIN', 'offset': 20, 'size': 4,
            'timestamp': 123, 'deleted': False})
        archive.extract('test.bin', self.output)
        self.assertEqual(self.output.read_bytes(), b'test')
        self.assertEqual(self.source.read_bytes(), fixture())

    def test_refuses_overwrite_and_symlink(self):
        archive = self.read(fixture())
        with self.assertRaises(FileExistsError):
            archive.extract('TEST.BIN', self.source)
        self.output.symlink_to(self.source)
        with self.assertRaises(FileExistsError):
            archive.extract('TEST.BIN', self.output)
        self.assertEqual(self.source.read_bytes(), fixture())

    def test_bad_headers(self):
        for data in (b'', fixture()[:19], b'\0' * 20,
                     struct.pack('<I4sIII', 20, b'PFF3', 0xffffffff, 32, 24),
                     struct.pack('<I4sIII', 20, b'PFF3', 0, 32, 19),
                     struct.pack('<I4sIII', 20, b'PFF3', 0, 32, 21),
                     fixture()[:-1]):
            with self.subTest(data=data[:20]), self.assertRaises(pff.PffError):
                self.read(data)

    def test_bad_entries(self):
        for kwargs in ({'offset': 19}, {'offset': 25}, {'size': 5},
                       {'size': 0xffffffff}, {'flags': 2}, {'flags': 1},
                       {'name': b'../TEST'}, {'name': b'C:TEST'},
                       {'name': b'A\\TEST'}, {'name': b''},
                       {'name': b'\xff'}, {'name': b'A' * 16}):
            with self.subTest(kwargs=kwargs), self.assertRaises(pff.PffError):
                self.read(fixture(**kwargs))

    def test_deleted_and_missing_entries(self):
        archive = self.read(fixture(name=b'<DEAD SPACE>', flags=1))
        self.assertTrue(archive.entries[0].deleted)
        for name in ('<DEAD SPACE>', 'MISSING.BIN', '../TEST'):
            with self.subTest(name=name), self.assertRaises(pff.PffError):
                archive.extract(name, self.output)
        self.assertFalse(self.output.exists())

    def test_duplicate_names_are_ambiguous(self):
        data = fixture()
        data = struct.pack('<I4sIII', 20, b'PFF3', 2, 32, 24) + data[20:] + data[24:]
        archive = self.read(data)
        with self.assertRaises(pff.PffError):
            archive.extract('TEST.BIN', self.output)


if __name__ == '__main__':
    unittest.main()
