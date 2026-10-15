"""Compare 32-bit terrain shade tables with original DF1 x86 instructions."""
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
parser.add_argument('--report', type=Path, default=Path('.local/reports/terrain-shades32-differential.json'))
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
u.mem_write(0x500b10, struct.pack('<I', 0x4cebe0))
u.mem_write(0xc22ad0, bytes((230, 210, 190, 0)))
results = []
def run(address, *parameters):
    u.mem_write(0x2008000, struct.pack('<' + 'I' * (len(parameters) + 1), 0x4000000, *parameters))
    u.reg_write(UC_X86_REG_ESP, 0x2008000)
    u.emu_start(address, 0x4000000, count=100000000, timeout=50000000)
    assert u.reg_read(UC_X86_REG_EIP) == 0x4000000
for seed in range(9):
    palette = bytes((i if seed % 3 == 1 else 0 if seed % 3 == 2 else i * multiplier) & 255
                    for i in range(256) for multiplier in (37, 67, 101))
    controls = (128, 128, 128, 128, 128) if seed // 3 == 0 else (64, 0, 255, 64, 192) if seed // 3 == 1 else (255, 255, 255, 128, 1)
    u.mem_write(0x3500000, palette)
    run(0x480bb0, *controls)
    run(0x480490, 0x3500000, 0x3100000)
    run(0x4801b0, 0x3500000, 0x3108000)
    fingerprint = 0xcbf29ce484222325
    for byte in u.mem_read(0x3100000, 65536):
        fingerprint = ((fingerprint ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    expected = dict(shades=f'{fingerprint:016x}')
    actual = json.loads(subprocess.check_output([str(args.probe.resolve()), '--shades32-fixture', str(seed)]))
    assert actual == expected, (seed, actual, expected)
    results.append(dict(seed=seed, controls=controls, hashes=expected))
report = dict(source_sha256=source_hash, procedures=['0x480bb0', '0x480490', '0x4801b0'],
              unicorn_version=unicorn.__version__, cases=9, byte_samples=9 * 65536, mismatches=0, results=results)
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
