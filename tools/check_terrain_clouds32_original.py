"""Compare 32-bit cloud pixels and projection row rays with original DF1 x86 code."""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path

import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('game', type=Path)
parser.add_argument('probe', type=Path)
parser.add_argument('--report', type=Path, default=Path('.local/reports/terrain-clouds32-differential.json'))
args = parser.parse_args()
source = (args.game / 'Df.exe').read_bytes()
source_hash = hashlib.sha256(source).hexdigest()
assert source_hash == 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
pe = pefile.PE(data=source)
image = pe.get_memory_mapped_image()
base_clouds = bytes((x * 3 + y * 5) & 63 for y in range(512) for x in range(512))
mips = [base_clouds]
for level in range(1, 9):
    width = 512 >> (level - 1)
    data = mips[-1]
    mips.append(bytes(max(0, (data[y * width + x] + data[y * width + x + 1] +
                            data[(y + 1) * width + x] + data[(y + 1) * width + x + 1] + 2) // 4 - 1)
                      for y in range(0, width, 2) for x in range(0, width, 2)))

def fingerprint(data):
    value = 0xcbf29ce484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return f'{value:016x}'

for seed in range(64):
    u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    u.mem_map(0x400000, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    u.mem_write(0x400000, image)
    for address, size in ((0x2000000, 65536), (0x3000000, 0x500000), (0x4000000, 4096)):
        u.mem_map(address, size)
    def word(address, value):
        u.mem_write(address, struct.pack('<I', value & 0xffffffff))
    def run(address, *parameters):
        u.mem_write(0x2008000, struct.pack('<' + 'I' * (len(parameters) + 1), 0x4000000, *parameters))
        u.reg_write(UC_X86_REG_ESP, 0x2008000)
        u.reg_write(UC_X86_REG_FPCW, 0x037f)
        u.emu_start(address, 0x4000000, count=10000000, timeout=10000000)
        assert u.reg_read(UC_X86_REG_EIP) == 0x4000000, hex(address)
    run(0x477510)
    sine = struct.unpack('<2561i', u.mem_read(0xbd0068, 2561 * 4))
    def sincos(angle):
        index, fraction = angle >> 21, angle & 0x1fffff
        return tuple(sine[i] + ((sine[i + 1] - sine[i]) * fraction >> 21) for i in (index, index + 512))
    word(0xc94aa0, 0x3000000)
    for offset, value in ((4, seed * 123456), (8, -seed * 34567), (12, 10 * 65536), (0x10, seed * 0x01234567),
                          (0x14, seed * 0x04000000), (0x20, 0x3200000), (0x24, 64), (0x28, 48), (0x30, 64),
                          (0x34, 0), (0x38, 0), (0x3c, 63), (0x40, 47), (0x4c, 90), (0x50, 32),
                          (0x54, 32), (0x58, 32), (0x5c, 24)):
        word(0x3000000 + offset, value)
    for row in range(48):
        word(0x3000068 + row * 4, 0x3100000 + row * 64 * 4)
    word(0xbd5270, 4)
    run(0x489440, *struct.unpack('<3I', struct.pack('<3f', 2.2, 2.2, 1.0)))
    expected = dict(rays=fingerprint(u.mem_read(0xc94da4, 48 * 12)))
    sy, cy = sincos(seed * 0x01234567)
    sp, cp = sincos(seed * 0x04000000)
    for offset, value in ((0x1868, cy), (0x186c, sy), (0x1870, cp), (0x1874, sp)):
        word(0x3000000 + offset, value)
    word(0xbd4f94, 32)
    word(0xbd5240, 1236)
    word(0xbd5264, 65536)
    word(0xcbeddc, seed * 17)
    address = 0x3300000
    for level, data in enumerate(mips):
        word(0xbd5438 + level * 4, address)
        u.mem_write(address, data)
        address += len(data)
    u.mem_write(0xbd67f8, struct.pack('<32768I', *[(i * 0x01020304) & 0xffffffff for i in range(32768)]))
    u.mem_write(0x3100000, b'\xaa' * (64 * 48 * 4))
    run(0x48c990)
    horizon = u.reg_read(UC_X86_REG_EAX)
    if horizon < 0x80000000:
        # Compare the cloud branch; the flat sky branch has a separate frame test.
        u.mem_write(0x48c5c0, b'\xc3')
        run(0x48c650, 0, min(horizon, 47))
    expected['pixels'] = fingerprint(u.mem_read(0x3100000, 64 * 48 * 4))
    actual = json.loads(subprocess.check_output([str(args.probe.resolve()), '--clouds32-fixture', str(seed)]))
    assert actual == expected, (seed, actual, expected)
report = dict(source_sha256=source_hash, procedures=['0x489440', '0x48c650', '0x459532'], cases=64,
              pixel_bytes=64 * 64 * 48 * 4, row_ray_values=64 * 48 * 3,
              unicorn_version=unicorn.__version__, mismatches=0)
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
