"""Compare 32-bit terrain color mips with original DF1 x86 code."""
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
parser.add_argument('--report', type=Path, default=Path('.local/reports/terrain-colormips32-differential.json'))
args = parser.parse_args()
source = (args.game / 'Df.exe').read_bytes()
source_hash = hashlib.sha256(source).hexdigest()
assert source_hash == 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
pe = pefile.PE(data=source)
u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
u.mem_map(0x400000, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
u.mem_write(0x400000, pe.get_memory_mapped_image())
for address, size in ((0x2000000, 65536), (0x3000000, 0x700000), (0x4000000, 4096)):
    u.mem_map(address, size)
color = bytes((x * 5 + y * 11) & 255 for y in range(1024) for x in range(1024))
palette = bytes(i * multiplier & 255 for i in range(256) for multiplier in (37, 67, 101))
u.mem_write(0xbd5480, palette)
u.mem_write(0x500b0c, struct.pack('<I', 0x3600000))
def run(address, *parameters):
    u.mem_write(0x2008000, struct.pack('<' + 'I' * (len(parameters) + 1), 0x4000000, *parameters))
    u.reg_write(UC_X86_REG_ESP, 0x2008000)
    u.emu_start(address, 0x4000000, count=1000000000, timeout=50000000)
    assert u.reg_read(UC_X86_REG_EIP) == 0x4000000, hex(address)

def fingerprint(data):
    value = 0xcbf29ce484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return f'{value:016x}'
results = []
for profile in range(2):
    u.mem_write(0x3600000, bytes(i if profile else i * i // 255 for i in range(256)))
    run(0x480600, 0xbd5480)
    u.mem_write(0x3000000, color)
    expected = [fingerprint(color)]
    address = 0x3000000
    width = 1024
    for lod in range(1, 10):
        run(0x47d7d0, address, 0x3200000, width)
        width //= 2
        data = bytes(u.mem_read(0x3200000, width * width))
        expected.append(fingerprint(data))
        u.mem_write(0x3000000, data)
    actual = json.loads(subprocess.check_output([str(args.probe.resolve()), '--color-mips32-fixture', str(profile)]))
    assert actual == expected, (profile, actual, expected)
    results.append(expected)
report = dict(source_sha256=source_hash, procedures=['0x480600', '0x47d7d0'], profiles=2,
              mip_bytes=2 * sum((1024 >> lod) ** 2 for lod in range(10)),
              unicorn_version=unicorn.__version__, mismatches=0, hashes=results)
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
