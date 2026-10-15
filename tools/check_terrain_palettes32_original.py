"""Compare terrain coarse, detail and sky color tables with original DF1 x86 code."""
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
parser.add_argument('--report', type=Path, default=Path('.local/reports/terrain-palettes32-differential.json'))
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
color_palette = bytes(i * multiplier & 255 for i in range(256) for multiplier in (37, 67, 101))
detail_palette = bytes(i * multiplier & 255 for i in range(256) for multiplier in (17, 29, 43))
sky_palette = bytes((i * 11 + channel * 31) & 255 for i in range(256) for channel in range(3))
u.mem_write(0xbd5480, color_palette)
u.mem_write(0xbd5780, detail_palette)
u.mem_write(0xbd5a80, sky_palette[:51])
light = ((sky_palette[48] + 2 * sky_palette[49] + sky_palette[50]) // 4) << 24 | sky_palette[48] << 16 | sky_palette[49] << 8 | sky_palette[50]
for address, value in ((0x500b10, 0x4cebe0), (0xbd4f94, 32), (0xbd63c4, 0x3100000),
                       (0xbd67c8, light), (0xbd67cc, light), (0xbd5248, 0), (0xbd527c, 0)):
    u.mem_write(address, struct.pack('<I', value))
def run(address, *parameters):
    u.mem_write(0x2008000, struct.pack('<' + 'I' * (len(parameters) + 1), 0x4000000, *parameters))
    u.reg_write(UC_X86_REG_ESP, 0x2008000)
    u.emu_start(address, 0x4000000, count=100000000, timeout=50000000)
    assert u.reg_read(UC_X86_REG_EIP) == 0x4000000

def fingerprint(data):
    value = 0xcbf29ce484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return f'{value:016x}'
results = []
for seed in range(9):
    gamma = bytes(i if seed % 2 else i * i // 255 for i in range(256))
    u.mem_write(0x3500000, gamma)
    u.mem_write(0x500b0c, struct.pack('<I', 0x3500000))
    u.mem_write(0xc22ad8, bytes(gamma[value] for value in sky_palette[:3]))
    controls = (128, 128, 128, 128, 128) if seed // 3 == 0 else (64, 0, 255, 64, 192) if seed // 3 == 1 else (255, 255, 255, 128, 1)
    run(0x480bb0, *controls)
    run(0x47d080)
    expected = dict(coarse=fingerprint(u.mem_read(0x3100000, 20480)), detail=fingerprint(u.mem_read(0xbd63c8, 1024)),
                    sky=fingerprint(u.mem_read(0xbd67f8, 131072)), light=light)
    actual = json.loads(subprocess.check_output([str(args.probe.resolve()), '--palettes32-fixture', str(seed)]))
    assert actual == expected, (seed, actual, expected)
    results.append(dict(seed=seed, controls=controls, hashes=expected))
report = dict(source_sha256=source_hash, procedure='0x47d080', body_entry='0x47c6a0', unicorn_version=unicorn.__version__,
              cases=9, byte_samples=9 * (20480 + 1024 + 131072), mismatches=0,
              scope='32-bit coarse, detail and sky color tables; water disabled', results=results)
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
