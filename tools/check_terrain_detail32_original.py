"""Compare 32-bit detail mips and all four cache levels with original DF1 x86 code."""
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
parser.add_argument('--report', type=Path, default=Path('.local/reports/terrain-detail32-differential.json'))
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
height = bytes((x * 13 + y * 7 + (x ^ y)) & 255 for y in range(1024) for x in range(1024))
character = bytes((x + y * 3) & 255 for y in range(1024) for x in range(1024))
color = bytes((x * 5 + y * 11) & 255 for y in range(1024) for x in range(1024))
words = [((tile + at) & 255) | (((tile * 3 + at * 7) & 255) << 8) |
         (((tile * 17 + at * 3) & 255) << 16) for tile in range(256) for at in range(256)]
palette = bytes(i * multiplier & 255 for i in range(256) for multiplier in (17, 29, 43))
for address, data in ((0x3000000, height), (0x3100000, character), (0x3500000, color), (0xbd5780, palette)):
    u.mem_write(address, data)
for address, value in ((0xbd5284, 0x3000000), (0xbd5330, 0x3100000), (0xbd52ac, 0x3500000),
                       (0xbd63c4, 0x3300000), (0x500b0c, 0x3610000)):
    u.mem_write(address, struct.pack('<I', value))
mip_addresses = [0x3200000, 0x3240000, 0x3250000, 0x3254000, 0x3255000]
mip_lengths = [256 * (16 >> lod) ** 2 * 4 for lod in range(5)]
for lod, address in enumerate(mip_addresses):
    u.mem_write(0xbd531c + lod * 4, struct.pack('<I', address))
for lod in range(4):
    u.mem_write(0xbd52fc + lod * 4, struct.pack('<I', 0x3400000))
    u.mem_write(0xbd530c + lod * 4, struct.pack('<I', 0x3410000))
def run(address, *parameters):
    u.mem_write(0x2008000, struct.pack('<' + 'I' * (len(parameters) + 1), 0x4000000, *parameters))
    u.reg_write(UC_X86_REG_ESP, 0x2008000)
    u.emu_start(address, 0x4000000, count=100000000, timeout=50000000)
    assert u.reg_read(UC_X86_REG_EIP) == 0x4000000, hex(address)

def fingerprint(data):
    value = 0xcbf29ce484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return f'{value:016x}'
mip_profiles = []
for profile in range(2):
    u.mem_write(0x3610000, bytes(i if profile else i * i // 255 for i in range(256)))
    u.mem_write(mip_addresses[0], struct.pack('<65536I', *words))
    run(0x480600, 0xbd5780)
    run(0x47d2f0)
    mip_profiles.append([bytes(u.mem_read(address, length)) for address, length in zip(mip_addresses, mip_lengths)])
for seed in range(64):
    for address, data in zip(mip_addresses, mip_profiles[seed % 2]):
        u.mem_write(address, data)
    state = seed
    def next_value():
        global state
        state = (state * 1664525 + 1013904223) & 0xffffffff
        return state
    coarse = [next_value() for _ in range(512)]
    detail = [next_value() for _ in range(256)]
    light = next_value()
    x, y = next_value(), next_value()
    u.mem_write(0x3300000, struct.pack('<512I', *coarse))
    u.mem_write(0xbd63c8, struct.pack('<256I', *detail))
    u.mem_write(0xbd67cc, struct.pack('<I', light))
    function = [0x489ff0, 0x48a9c0, 0x48b420, 0x48bd20][seed % 4]
    run(function, x, y, 0)
    expected = dict(mips=[fingerprint(data) for data in mip_profiles[seed % 2]],
                    height=list(struct.unpack('<256H', u.mem_read(0x3400000, 512))),
                    color=list(struct.unpack('<256I', u.mem_read(0x3410000, 1024))))
    actual = json.loads(subprocess.check_output([str(args.probe.resolve()), '--detail32-fixture', str(seed)]))
    assert actual == expected, (seed, actual, expected)
report = dict(source_sha256=source_hash, procedures=['0x47d2f0', '0x489ff0', '0x48a9c0', '0x48b420', '0x48bd20'],
              unicorn_version=unicorn.__version__, cache_cases=64, mip_profiles=2, mip_bytes=sum(mip_lengths) * 2,
              height_samples=64 * 256, color_samples=64 * 256, mismatches=0)
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
