"""Check byte offsets and PE file-to-memory address mapping."""

from pathlib import Path
from types import SimpleNamespace
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from pe_report import ascii_strings, mapped_rva


class OffsetTests(unittest.TestCase):
    def test_strings_preserve_byte_offsets(self):
        self.assertEqual(list(ascii_strings(b"\0hello\xff\0archive.pff\0end", 4)),
                         [(1, "hello"), (8, "archive.pff")])

    def test_raw_padding_maps_but_overlay_does_not(self):
        pe = SimpleNamespace(
            OPTIONAL_HEADER=SimpleNamespace(SizeOfHeaders=512),
            __data__=bytes(4096),
            sections=[SimpleNamespace(PointerToRawData=512, SizeOfRawData=1024,
                                      VirtualAddress=4096, Misc_VirtualSize=900,
                                      Characteristics=0x20),
                      SimpleNamespace(PointerToRawData=0, SizeOfRawData=8192,
                                      VirtualAddress=8192, Misc_VirtualSize=0,
                                      Characteristics=0x80)])
        self.assertEqual(mapped_rva(pe, 64), 64)
        self.assertEqual(mapped_rva(pe, 512), 4096)
        self.assertEqual(mapped_rva(pe, 1535), 5119)
        self.assertIsNone(mapped_rva(pe, 1536))
        self.assertIsNone(mapped_rva(pe, -1))


if __name__ == "__main__":
    unittest.main()
