"""Compare projection tables with original DF1 x86 instructions."""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path

import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('game', type=Path)
parser.add_argument('probe', type=Path)
parser.add_argument('--report', type=Path, default=Path('.local/reports/terrain-projection-differential.json'))
parser.add_argument('--golden', type=Path, help='Write derived numeric test results; includes no game bytes')
args = parser.parse_args()
source = (args.game / 'Df.exe').read_bytes()
source_hash = hashlib.sha256(source).hexdigest()
assert source_hash == 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
pe = pefile.PE(data=source)
u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
u.mem_map(0x400000, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
u.mem_write(0x400000, pe.get_memory_mapped_image())
for address, size in ((0x2000000, 65536), (0x3000000, 4096), (0x4000000, 4096)):
    u.mem_map(address, size)
for offset, value in ((0x24, 640), (0x28, 480), (0x30, 640), (0x34, 0), (0x38, 0),
                      (0x3c, 639), (0x40, 479), (0x4c, 90), (0x58, 320), (0x5c, 240)):
    u.mem_write(0x3000000 + offset, struct.pack('<I', value))
u.mem_write(0xc94aa0, struct.pack('<I', 0x3000000))
u.mem_write(0xbd5270, struct.pack('<I', 4))
compared = 0
golden = []
for seed in range(64):
    focal = 80 + (seed % 32) * 17
    detail = 3.2 if seed % 2 else 2.2
    coarse = 2.2 if seed % 3 else 3.2
    factor = 1.0 if seed % 5 else 0.75
    u.mem_write(0x3000054, struct.pack('<I', focal))
    u.mem_write(0x500d5c, struct.pack('<I', 0))
    u.mem_write(0x2008000, struct.pack('<Ifff', 0x4000000, detail, coarse, factor))
    u.reg_write(UC_X86_REG_ESP, 0x2008000)
    u.reg_write(UC_X86_REG_FPCW, 0x037f)
    u.emu_start(0x489440, 0x4000000, count=10000000, timeout=10000000)
    assert u.reg_read(UC_X86_REG_EIP) == 0x4000000
    expected = []
    for i in range(14):
        row = list(struct.unpack_from('<Ididiiiii', u.mem_read(0xcb89c4 + 48 * i, 48)))
        row[7] = (row[7] - 0xc985a8) // 4
        expected.append(row)
    count = expected[-1][7]
    depths = list(struct.unpack('<' + 'I' * count, u.mem_read(0xc985a8, count * 4)))
    actual = json.loads(subprocess.check_output([str(args.probe.resolve()), '--projection-fixture', str(seed)]))
    assert actual['levels'] == expected, ('levels', seed, actual['levels'], expected)
    assert actual['depths'] == depths, ('depths', seed)
    compared += count
    fingerprint = 0xcbf29ce484222325
    numeric_bytes = b''.join(struct.pack('<Ididiiiii', *row) for row in expected)
    numeric_bytes += b''.join(struct.pack('<I', value) for value in depths)
    for byte in numeric_bytes:
        fingerprint = ((fingerprint ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    golden.append(fingerprint)
report = dict(source_sha256=source_hash, procedure='0x489440', unicorn_version=unicorn.__version__,
              cases=64, lod_records=64 * 14, depth_entries=compared, mismatches=0)
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(report, indent=2) + '\n')
if args.golden:
    args.golden.write_text('\n'.join(f'0x{value:016x}ull,' for value in golden) + '\n')
print(json.dumps(report))
