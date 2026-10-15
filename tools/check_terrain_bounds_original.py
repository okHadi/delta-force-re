"""Compare height-bound mips with original DF1 x86 instructions."""
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
parser.add_argument('--report', type=Path, default=Path('.local/reports/terrain-bounds-differential.json'))
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
u.mem_write(0x3000000, bytes((x * 13 + y * 7 + (x ^ y)) & 255 for y in range(1024) for x in range(1024)))
u.mem_write(0x3100000, bytes((x + y * 3) & 255 for y in range(1024) for x in range(1024)))
u.mem_write(0x3200000, struct.pack('<256I', *[tile << 16 for tile in range(256)]))
for address, value in ((0xbd5284, 0x3000000), (0xbd5330, 0x3100000), (0xbd532c, 0x3200000)):
    u.mem_write(address, struct.pack('<I', value))
width = 1024
out = 0x3300000
buffers = []
for level in range(10):
    u.mem_write(0xbd52d4 + level * 4, struct.pack('<I', out))
    buffers.append((out, width * width))
    out += width * width
    width //= 2
u.mem_write(0x2008000, struct.pack('<I', 0x4000000))
u.reg_write(UC_X86_REG_ESP, 0x2008000)
u.emu_start(0x47d9e0, 0x4000000, count=1000000000, timeout=50000000)
assert u.reg_read(UC_X86_REG_EIP) == 0x4000000, hex(u.reg_read(UC_X86_REG_EIP))
expected = []
for address, length in buffers:
    fingerprint = 0xcbf29ce484222325
    for byte in u.mem_read(address, length):
        fingerprint = ((fingerprint ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    expected.append(f'{fingerprint:016x}')
actual = json.loads(subprocess.check_output([str(args.probe.resolve()), '--bounds-fixture']))
assert actual == expected, (actual, expected)
report = dict(source_sha256=source_hash, procedure='0x47d9e0', unicorn_version=unicorn.__version__,
              levels=10, byte_samples=sum(length for _, length in buffers), mismatches=0)
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
