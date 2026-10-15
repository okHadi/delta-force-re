"""Compare the native detail tile builder with original DF1 x86 instructions."""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path

import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('game', type=Path)
parser.add_argument('probe', type=Path)
parser.add_argument('--report', type=Path, default=Path('.local/reports/terrain-detail-differential.json'))
args = parser.parse_args()
source = (args.game / 'Df.exe').read_bytes()
source_hash = hashlib.sha256(source).hexdigest()
assert source_hash == 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
pe = pefile.PE(data=source)
u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
u.mem_map(0x400000, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
u.mem_write(0x400000, pe.get_memory_mapped_image())
for address, size in ((0x2000000, 65536), (0x3000000, 0x600000), (0x4000000, 4096)):
    u.mem_map(address, size)
height = bytes((x * 13 + y * 7 + (x ^ y)) & 255 for y in range(1024) for x in range(1024))
character = bytes((x + y * 3) & 255 for y in range(1024) for x in range(1024))
color = bytes((x * 5 + y * 11) & 255 for y in range(1024) for x in range(1024))
detail = [((tile + at) & 255) | (((tile * 3 + at * 7) & 255) << 8) |
          (((tile * 17 + at * 3) & 255) << 16) for tile in range(256) for at in range(256)]
blend = bytes((a * 13 + b * 17) & 255 for a in range(256) for b in range(256))
shade = bytes((s * 7 + c * 3) & 255 for s in range(64) for c in range(256))
for address, data in ((0x3000000, height), (0x3100000, character), (0x3200000, struct.pack('<65536I', *detail)),
                      (0x3300000, color), (0x3400000, blend), (0x3410000, shade),
                      (0x500d68, struct.pack('<256I', *[(i * 7) % 12 for i in range(256)]))):
    u.mem_write(address, data)
for address, value in ((0xbd5284, 0x3000000), (0xbd5330, 0x3100000), (0xbd531c, 0x3200000),
                       (0xbd52ac, 0x3300000), (0xbfc100, 0x3400000), (0xbfc104, 0x3410000),
                       (0xbd52fc, 0x3500000), (0xbd530c, 0x3510000)):
    u.mem_write(address, struct.pack('<I', value))
for seed in range(64):
    x = (seed * 1664525 + 1013904223) & 0xffffffff
    y = (x * 1664525 + 1013904223) & 0xffffffff
    u.mem_write(0x2008000, struct.pack('<IIII', 0x4000000, x, y, 0))
    u.reg_write(UC_X86_REG_ESP, 0x2008000)
    u.emu_start(0x489cc0, 0x4000000, count=1000000, timeout=10000000)
    assert u.reg_read(UC_X86_REG_EIP) == 0x4000000
    expected = dict(height=list(struct.unpack('<256H', u.mem_read(0x3500000, 512))),
                    color=list(u.mem_read(0x3510000, 256)))
    actual = json.loads(subprocess.check_output([str(args.probe.resolve()), '--detail-fixture', str(seed)]))
    assert actual == expected, (seed, actual, expected)
report = dict(source_sha256=source_hash, procedure='0x489cc0', unicorn_version=unicorn.__version__,
              cases=64, height_samples=64 * 256, color_samples=64 * 256, mismatches=0,
              scope='base detail tile builder with generated palette lookup inputs and blend pattern')
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
