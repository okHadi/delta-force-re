"""Compare terrain palette lookup tables with original DF1 x86 instructions."""
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
parser.add_argument('--report', type=Path, default=Path('.local/reports/terrain-palette-differential.json'))
args = parser.parse_args()
source = (args.game / 'Df.exe').read_bytes()
source_hash = hashlib.sha256(source).hexdigest()
assert source_hash == 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
pe = pefile.PE(data=source)
results = []
for seed in range(3):
    u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    u.mem_map(0x400000, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    u.mem_write(0x400000, pe.get_memory_mapped_image())
    for address, size in ((0x2000000, 65536), (0x3000000, 0x600000), (0x4000000, 4096)):
        u.mem_map(address, size)
    palette = bytes((i if seed == 1 else 0 if seed == 2 else i * multiplier) & 255
                    for i in range(256) for multiplier in (37, 67, 101))
    u.mem_write(0x3500000, palette)
    u.mem_write(0x3400000, bytes(range(256)))
    u.mem_write(0x500b0c, struct.pack('<I', 0x3400000))
    u.mem_write(0xc22ad0, bytes((230, 210, 190, 0)))
    def run(address, *parameters):
        u.mem_write(0x2008000, struct.pack('<' + 'I' * (len(parameters) + 1), 0x4000000, *parameters))
        u.reg_write(UC_X86_REG_ESP, 0x2008000)
        u.emu_start(address, 0x4000000, count=1000000000, timeout=50000000)
        assert u.reg_read(UC_X86_REG_EIP) == 0x4000000, (seed, hex(address), hex(u.reg_read(UC_X86_REG_EIP)))
    run(0x480600, 0x3500000)
    run(0x47ffc0, 0x3500000, 0x3100000)
    run(0x4803e0, 0x3500000, 0x3110000)
    run(0x4800e0, 0x3500000, 0x3112000)
    expected = {}
    for name, address, length in (('blend', 0x3100000, 65536), ('shade', 0x3110000, 16384)):
        fingerprint = 0xcbf29ce484222325
        for byte in u.mem_read(address, length):
            fingerprint = ((fingerprint ^ byte) * 0x100000001b3) & 0xffffffffffffffff
        expected[name] = f'{fingerprint:016x}'
    remap = bytearray(256)
    from unicorn.x86_const import UC_X86_REG_EAX
    for index in range(1, 256):
        run(0x480a30, *palette[index * 3:index * 3 + 3])
        remap[index] = u.reg_read(UC_X86_REG_EAX) & 255
    value = 0xcbf29ce484222325
    for byte in remap:
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    expected['remap'] = f'{value:016x}'
    actual = json.loads(subprocess.check_output([str(args.probe.resolve()), '--palette-fixture', str(seed)]))
    assert actual == expected, (seed, actual, expected)
    results.append(dict(seed=seed, hashes=expected, candidate_entries=struct.unpack('<I', u.mem_read(0xc22ac8, 4))[0]))
report = dict(source_sha256=source_hash, procedures=['0x480600', '0x47ffc0', '0x4803e0', '0x4800e0', '0x480a30'],
              unicorn_version=unicorn.__version__, cases=3, byte_samples=3 * (65536 + 16384 + 256), mismatches=0, results=results)
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
