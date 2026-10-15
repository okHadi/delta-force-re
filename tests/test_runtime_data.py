import struct
import sys
import tempfile
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from prepare_runtime_data import extract_virtual, prepare, write_local


def sample():
    data = bytearray(1024)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 128)
    struct.pack_into('<I', data, 128, 0x4550)
    struct.pack_into('<H', data, 134, 2)
    struct.pack_into('<H', data, 148, 224)
    struct.pack_into('<H', data, 152, 0x10b)
    struct.pack_into('<I', data, 180, 0x400000)
    struct.pack_into('<4I', data, 388, 0x1000, 512, 512, 0)
    struct.pack_into('<I', data, 412, 0x40)
    struct.pack_into('<4I', data, 428, 0x3000, 4096, 0, 0)
    struct.pack_into('<I', data, 452, 0x80)
    data[512:520] = b'lookup!!'
    return data


class RuntimeDataTests(unittest.TestCase):
    def test_raw_range_and_unbacked_section(self):
        data = sample()
        self.assertEqual(extract_virtual(data, 0x401000, 8), b'lookup!!')
        for address, count in [(0x400fff, 1), (0x4011ff, 2), (0x403000, 8), (0x401000, -1)]:
            with self.subTest(address=address, count=count), self.assertRaises(ValueError):
                extract_virtual(data, address, count)

    def test_malformed_and_overlapping_sections(self):
        data = sample()
        for truncated in [data[:1], data[:60], data[:181], data[:453], data[:519]]:
            with self.assertRaises(ValueError):
                extract_virtual(truncated, 0x401000, 8)
        struct.pack_into('<I', data, 428, 0x1000)
        struct.pack_into('<I', data, 436, 512)
        struct.pack_into('<I', data, 452, 0x40)
        with self.assertRaises(ValueError):
            extract_virtual(data, 0x401000, 8)

    def test_output_and_original_folder(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            target = folder / 'data.bin'
            write_local(target, b'checked')
            write_local(target, b'checked')
            with self.assertRaises(ValueError):
                write_local(target, b'different')
            self.assertEqual(target.read_bytes(), b'checked')
            with self.assertRaises(ValueError):
                prepare(folder, folder / 'cache')


if __name__ == '__main__':
    unittest.main()
