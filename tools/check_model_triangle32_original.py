#!/usr/bin/env python3
"""Compare complete flat and textured triangle draws with original DF1 x86."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess

import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP


def check(executable, probe, count):
    source = Path(executable).read_bytes()
    source_hash = hashlib.sha256(source).hexdigest()
    if source_hash != 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff':
        raise ValueError('Unsupported original executable hash')
    pe = pefile.PE(data=source)
    u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    u.mem_map(0x400000, 0x1000000)
    u.mem_write(0x400000, pe.get_memory_mapped_image())
    u.mem_map(0x2000000, 0x10000)
    u.mem_map(0x3000000, 0x30000)
    u.mem_map(0x6000000, 4096)

    def word(address, value):
        u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def call(address, *values):
        u.mem_write(0x2000800, struct.pack('<' + 'I' * (len(values) + 1), 0x6000000, *values))
        u.reg_write(UC_X86_REG_ESP, 0x2000800)
        u.emu_start(address, 0x6000000, count=2000000)
        if u.reg_read(UC_X86_REG_EIP) != 0x6000000:
            raise ValueError(f'Original triangle draw did not return: {address:x}')

    call(0x4686d0)
    word(0x8ee1e0, 0x2001000)
    word(0x8ee1e8, 0)
    u.mem_write(0x2001034, struct.pack('<4i', 0, 0, 31, 23))
    word(0x2001050, 32)
    for y in range(24):
        word(0x2001068 + y * 4, 0x3021000 + y * 32 * 4)
        word(0x2001c68 + y * 4, 0x3020000 + y * 32 * 2)
    word(0xbfc104, 0x3010000)
    word(0xbfc108, 0x3024000)
    u.mem_write(0x3010000, b''.join(struct.pack('<I', (i * 0x01010101 ^ 0xffa5c3e7) & 0xffffffff) for i in range(64 * 256)))
    rng = random.Random(462990)
    cases = []
    modes = [4, 8, 0x14, 0x18, 0x34, 0x38]
    for test in range(count):
        mode = modes[test % len(modes)]
        points = [[rng.randrange(-10, 42) * 65536 + rng.randrange(65536),
                   rng.randrange(-10, 34) * 65536 + rng.randrange(65536),
                   rng.randrange(-0x80000000, 0x80000000), rng.randrange(-0x1000000, 0x1000000),
                   rng.randrange(-0x1000000, 0x1000000), rng.randrange(0x400000)] for _ in range(3)]
        if not mode & 8:
            for point in points[1:]:
                point[5] = points[0][5]
        if test % 10 == 0:
            x, y = rng.randrange(-1, 33), rng.randrange(-1, 25)
            points[0][:2] = [x * 65536, y * 65536]
            points[1][:2] = [x * 65536, (y + (test % 3)) * 65536]
            points[2][:2] = [(x + (test % 3)) * 65536, y * 65536]
        cases.append((test, mode, rng.randrange(9), rng.randrange(9), rng.randrange(0x100000000), points))
    text = ''.join(' '.join(map(str, [*case[:5], *(v for point in case[5] for v in point)])) + '\n' for case in cases)
    native = subprocess.check_output([str(Path(probe).resolve()), 'triangle32'], input=text.encode()).splitlines()
    if len(native) != len(cases):
        raise ValueError('Native triangle count differs from input')
    changed = 0
    for number, (case, line) in enumerate(zip(cases, native)):
        test, mode, bx, by, flat, points = case
        initial_colours = [(i * 1234567 ^ 0xabcdef90) & 0xffffffff for i in range(768)]
        initial_depths = [(i * 317 + test * 17) & 65535 for i in range(768)]
        u.mem_write(0x3021000, struct.pack('<768I', *initial_colours))
        u.mem_write(0x3020000, struct.pack('<768H', *initial_depths))
        u.mem_write(0x3000000, bytes((i * 37 + test * 13) & 255 for i in range(1 << (bx + by))))
        material = bytearray(76)
        struct.pack_into('<I', material, 20, flat & 255)
        word(0x3024000 + (flat & 255) * 4, flat)
        struct.pack_into('<4H', material, 32, bx, by, 1 << bx, 1 << by)
        struct.pack_into('<I', material, 68, 0x3000000)
        u.mem_write(0x2002000, bytes(material))
        u.mem_write(0x2003000, b''.join(struct.pack('<8i', *point, point[0] // 65536, point[1] // 65536) for point in points))
        try:
            call(0x462df0, mode, 0x2003000, 0x2002000)
        except unicorn.UcError as error:
            raise ValueError(f'Original triangle failed at case {number}, mode {mode:x}, EIP={u.reg_read(UC_X86_REG_EIP):x}') from error
        colours = struct.unpack('<768I', u.mem_read(0x3021000, 768 * 4))
        depths = struct.unpack('<768H', u.mem_read(0x3020000, 768 * 2))
        expected = [value for pair in zip(colours, depths) for value in pair]
        if list(map(int, line.split())) != expected:
            first = next(i for i, (a, b) in enumerate(zip(map(int, line.split()), expected)) if a != b)
            raise ValueError(f'Triangle32 mismatch at case {number}, target field {first}, mode {mode:x}')
        changed += sum(a != b or c != d for a, b, c, d in zip(colours, initial_colours, depths, initial_depths))
    return dict(source_sha256=source_hash, function='0x462df0', cases=count,
                modes=[hex(m) for m in modes], target_pixels=count * 768, changed_pixels=changed,
                mismatches=0, code_sha256=hashlib.sha256(pe.get_data(0x62df0, 0xcd)).hexdigest(),
                scope='full original 32-bit triangle dispatch; normalized UV scaling; clipping; tiny, flat, textured, smooth and cutout pixels; original spans and pixel selector; all colour/depth pairs; supplied palette bindings')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable')
    parser.add_argument('--probe', required=True)
    parser.add_argument('--cases', type=int, default=1200)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.cases < 1:
        parser.error('--cases must be positive')
    result = check(args.executable, args.probe, args.cases)
    output = json.dumps(result, indent=2)
    print(output)
    if args.report:
        args.report.write_text(output + '\n')
