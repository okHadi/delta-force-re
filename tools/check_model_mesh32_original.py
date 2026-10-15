#!/usr/bin/env python3
"""Compare a complete mesh draw with original DF1 x86, using fixed bindings."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess

import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW


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
        u.reg_write(UC_X86_REG_FPCW, 0x037f)
        u.emu_start(address, 0x6000000, count=2000000)
        if u.reg_read(UC_X86_REG_EIP) != 0x6000000:
            raise ValueError(f'Original mesh draw did not return: {address:x}')

    call(0x477510)
    call(0x4686d0)
    camera = bytearray(80)
    struct.pack_into('<8I', camera, 0x20, 0x3021000, 32, 24, 128, 0, 0, 31, 23)
    struct.pack_into('<2I', camera, 0x48, 90, 32)
    u.mem_write(0x2007000, bytes(camera))
    call(0x44a820, 0x8cfa20, 0x2007000)
    word(0x8ee1e0, 0x8cfa20)
    word(0x8ee1e8, 0)
    for y in range(24):
        word(0x8cfa20 + 0x68 + y * 4, 0x3021000 + y * 32 * 4)
        word(0x8cfa20 + 0xc68 + y * 4, 0x3020000 + y * 32 * 2)
    word(0xbfc104, 0x3010000)
    word(0xbfc108, 0x3024000)
    u.mem_write(0x3010000, b''.join(struct.pack('<I', (i * 0x01010101 ^ 0xffa5c3e7) & 0xffffffff) for i in range(64 * 256)))
    # Prime all original self-modifying kernels with valid 2x2 dimensions.
    u.mem_write(0x2009000, struct.pack('<12I', 0, 0, 0, 2, 2, 1, 1,
                                     0x3020000, 0x3021000, 0x3000000, 0x3010000, 0))
    for setup in [0x468a6c, 0x468e56, 0x469258, 0x4696b3]:
        call(setup, 0x2009000)
    identity = [4194304, 0, 0, 0, 0, 4194304, 0, 0, 0, 0, 4194304, 0, 0, 0, 0, 0]
    u.mem_write(0x2003000, struct.pack('<16i', *identity))
    rng = random.Random(461770)
    cases = []
    for test in range(count):
        # These are stored 3DI 24.8 vertices, before camera transform.
        points = [[rng.randrange(-64, 4097), rng.randrange(-3072, 3073), rng.randrange(-2304, 2305)] for _ in range(3)]
        if test % 8 == 0:
            points = [[512, 128, 128], [512, 128, -128], [512, -128, 128]]
        flags = (1 if test & 1 else 0) | (2 if test & 2 else 0) | (0x200 if test & 4 and test & 1 else 0)
        flags |= 0x40 if test & 8 else 0
        flags |= 0x20 if test & 16 else 0
        normals = [[rng.randrange(-32768, 32768) for _ in range(3)] for _ in range(3)]
        light = [rng.randrange(-0x400000, 0x400001) for _ in range(6)] + [rng.randrange(30000) for _ in range(3)]
        uv = [rng.randrange(-0x10000, 0x20000) for _ in range(6)]
        head = [test, flags, test & 1, rng.randrange(9), rng.randrange(9), rng.randrange(0x100000000),
                (test >> 1) & 1, rng.randrange(0x20000), rng.randrange(0x10000), rng.randrange(5)]
        cases.append((head, points, normals, uv, light))
    text = ''.join(' '.join(map(str, [*head, *(v for point in points for v in point),
                                      *(v for normal in normals for v in normal), *uv, *light])) + '\n'
                   for head, points, normals, uv, light in cases)
    native = subprocess.check_output([str(Path(probe).resolve()), 'mesh32'], input=text.encode()).splitlines()
    if len(native) != count:
        raise ValueError('Native mesh count differs from input')
    changed = clipped = 0
    for number, ((head, points, normals, uv, light), line) in enumerate(zip(cases, native)):
        test, flags, smooth, bx, by, flat, math, specular, constant, power = head
        initial_colours = [(i * 1234567 ^ 0xabcdef90) & 0xffffffff for i in range(768)]
        initial_depths = [(i * 317 + test * 17) & 65535 for i in range(768)]
        u.mem_write(0x3021000, struct.pack('<768I', *initial_colours))
        u.mem_write(0x3020000, struct.pack('<768H', *initial_depths))
        u.mem_write(0x3000000, bytes((i * 37 + test * 13) & 255 for i in range(1 << (bx + by))))
        word(0x3024000 + (flat & 255) * 4, flat)
        material = bytearray(76)
        struct.pack_into('<III', material, 16, flags, flat & 255, 0)
        struct.pack_into('<4H', material, 32, bx, by, 1 << bx, 1 << by)
        struct.pack_into('<iiB', material, 56, specular, constant, power)
        struct.pack_into('<I', material, 68, 0x3000000)
        u.mem_write(0x2006000, bytes(material))
        face = bytearray(72)
        struct.pack_into('<HH6i6h', face, 0, smooth, 0, *uv, 0, 1, 2, 0, 1, 2)
        struct.pack_into('<I', face, 68, 0x2006000)
        u.mem_write(0x2002000, bytes(face))
        u.mem_write(0x2004000, b''.join(struct.pack('<4h', *p, 0) for p in points))
        u.mem_write(0x2005000, b''.join(struct.pack('<4h', *n, 0) for n in normals))
        u.mem_write(0x2001000, struct.pack('<7I', 0, 3, 0x2004000, 1, 0x2002000, 3, 0x2005000) + bytes(84))
        u.mem_write(0xad3d40, struct.pack('<3i', *light[:3]))
        u.mem_write(0xad3d50, struct.pack('<3i', *light[3:6]))
        u.mem_write(0xad3d60, struct.pack('<3i', *light[6:]))
        for address, value in [(0xad3d2c, math), (0xad3d7c, 0), (0xad3dc4, 0), (0xad3d28, 0),
                               (0xad3d78, 0), (0xad3d74, 0), (0xad3dbc, 0)]:
            word(address, value)
        try:
            call(0x46b1d0, 0x2001000, 0x2003000, 0x3e, 0)
        except unicorn.UcError as error:
            raise ValueError(f'Original mesh failed at case {number}, EIP={u.reg_read(UC_X86_REG_EIP):x}') from error
        colours = struct.unpack('<768I', u.mem_read(0x3021000, 768 * 4))
        depths = struct.unpack('<768H', u.mem_read(0x3020000, 768 * 2))
        expected = [value for pair in zip(colours, depths) for value in pair]
        if list(map(int, line.split())) != expected:
            first = next(i for i, (a, b) in enumerate(zip(map(int, line.split()), expected)) if a != b)
            raise ValueError(f'Mesh32 mismatch at case {number}, target field {first}, flags {flags:x}, math {math}')
        changed += sum(a != b or c != d for a, b, c, d in zip(colours, initial_colours, depths, initial_depths))
        clipped += struct.unpack('<I', u.mem_read(0xad3db4, 4))[0] > 3
    return dict(source_sha256=source_hash, function='0x46b1d0', cases=count,
                target_pixels=count * 768, changed_pixels=changed, split_meshes=clipped, mismatches=0,
                shader_setup='original kernels primed with a 2x2 texture before variable dimensions',
                scope='original full mesh call; scalar/MMX vertices; camera clipping and projection; local light; flat/smooth shades; face assembly; flat/textured/cutout/tiny pixel paths; supplied palette bindings; all colour/depth pairs')


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
