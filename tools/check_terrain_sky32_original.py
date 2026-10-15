"""Compare the 32-bit horizon and gradient rows with original DF1 x86 code."""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path

import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('game', type=Path)
parser.add_argument('probe', type=Path)
parser.add_argument('--report', type=Path, default=Path('.local/reports/terrain-sky32-differential.json'))
args = parser.parse_args()
source = (args.game / 'Df.exe').read_bytes()
source_hash = hashlib.sha256(source).hexdigest()
assert source_hash == 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
pe = pefile.PE(data=source)
for seed in range(64):
    u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    u.mem_map(0x400000, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    u.mem_write(0x400000, pe.get_memory_mapped_image())
    for address, size in ((0x2000000, 65536), (0x3000000, 0x100000), (0x4000000, 4096)):
        u.mem_map(address, size)
    u.mem_write(0xc94aa0, struct.pack('<I', 0x3000000))
    u.mem_write(0xbd4f94, struct.pack('<I', 32))
    u.mem_write(0xbd5264, struct.pack('<I', 65536))
    u.mem_write(0xc94aa4, bytes(480))
    context = bytearray(0x100)
    for offset, value in ((0x14, seed * 0x04000000), (0x20, 0x3010000), (0x24, 640), (0x28, 480),
                          (0x34, 0), (0x38, 0), (0x3c, 639), (0x40, 479), (0x4c, 22 if seed % 2 else 90),
                          (0x54, 1646 if seed % 2 else 320), (0x5c, 240)):
        struct.pack_into('<I', context, offset, value)
    u.mem_write(0x3000000, bytes(context))
    u.mem_write(0x2008000, struct.pack('<I', 0x4000000))
    u.reg_write(UC_X86_REG_ESP, 0x2008000)
    u.emu_start(0x48c990, 0x4000000, count=10000000)
    assert u.reg_read(UC_X86_REG_EIP) == 0x4000000
    row = u.reg_read(UC_X86_REG_EAX)
    expected = dict(row=row if row < 0x80000000 else row - 0x100000000,
                    gradient=list(u.mem_read(0xc94aa4, 480)))
    actual = json.loads(subprocess.check_output([str(args.probe.resolve()), '--horizon32-fixture', str(seed)]))
    assert actual == expected, (seed, actual['row'], expected['row'], [(i,a,b) for i,(a,b) in enumerate(zip(actual['gradient'], expected['gradient'])) if a != b][:8])
report = dict(source_sha256=source_hash, procedures=['0x48c990'], cases=64, gradient_bytes=64 * 480,
              unicorn_version=unicorn.__version__, mismatches=0)
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
