#!/usr/bin/env python3
"""Compare model face assembly with original DF1 x86 instructions."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess

import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP


def check(executable, probe, count):
    source = Path(executable).read_bytes()
    source_hash = hashlib.sha256(source).hexdigest()
    if source_hash != 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff':
        raise ValueError('Unsupported original executable hash')
    pe = pefile.PE(data=source)
    machine = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    machine.mem_map(0x400000, 0x1000000)
    machine.mem_write(0x400000, pe.get_memory_mapped_image())
    machine.mem_map(0x2000000, 0x20000)
    machine.mem_map(0x6000000, 4096)
    # Inputs start after vertex transform, projection, and local light conversion.
    mocked = [0x46b140, 0x469dc0, 0x46a040]
    for address in mocked:
        machine.mem_write(address, b'\xc3')
    rng = random.Random(461730)
    flag_bits = [1, 2, 8, 0x20, 0x40, 0x80, 0x200, 0x800, 0x4000, 0x8000]
    cases = []
    for test in range(count):
        flags = sum(bit for i, bit in enumerate(flag_bits) if test & (1 << i))
        start = rng.randrange(4)
        materials = [[flags, 255, rng.randrange(0x20000), rng.randrange(0x10000), rng.randrange(5)]]
        for _ in range(7):
            materials.append([rng.choice(flag_bits) | 2, rng.choice([0, 1, 10, 11, 127, 255]),
                              rng.randrange(0x20000), rng.randrange(0x10000), rng.randrange(5)])
        if test >= 1024:
            materials[0][1] = rng.choice([0, 1, 10, 11, 127, 255])
        counters = [rng.randrange(3) for _ in range(11)]
        normals = [[rng.randrange(-32768, 32768) for _ in range(3)] for _ in range(8)]
        lighting = [rng.randrange(-0x400000, 0x400001) for _ in range(6)] + [rng.randrange(40000) for _ in range(3)]
        points = [[rng.randrange(-100, 741) * 65536 + rng.randrange(65536),
                   rng.randrange(-100, 581) * 65536 + rng.randrange(65536),
                   rng.randrange(4096, 0x4000000), rng.randrange(-0x4000000, 0x4000000),
                   rng.randrange(-0x4000000, 0x4000000)] for _ in range(3)]
        if test % 31 == 0:
            points[2][:2] = points[0][:2]  # Retain the original degenerate-face rule.
        if test % 13 == 0:
            for point in points:
                point[:2] = [rng.randrange(-0x80000000, 0x80000000) for _ in range(2)]
        cases.append(dict(flags=test & 1, flat=rng.randrange(8 - start), start=start,
                          indices=[rng.randrange(start, 8) for _ in range(3)], counters=counters,
                          materials=materials, normals=normals, lighting=lighting, points=points))
    lines = []
    for c in cases:
        row = [c['flags'], c['flat'], 0, c['start'], *c['indices'], *c['counters'], len(c['materials'])]
        row += [v for material in c['materials'] for v in material]
        row += [len(c['normals']), *(v for normal in c['normals'] for v in normal), *c['lighting']]
        row += [v for point in c['points'] for v in point]
        lines.append(' '.join(map(str, row)))
    native = subprocess.check_output([str(Path(probe).resolve()), 'faces'], input=('\n'.join(lines) + '\n').encode()).splitlines()
    if len(native) != len(cases):
        raise ValueError('Native face count differs from input')

    def word(address, value):
        machine.mem_write(address, struct.pack('<I', value & 0xffffffff))

    accepted = hidden_or_backface = reversed_faces = varying = biased = animated = 0
    for test, (c, line) in enumerate(zip(cases, native)):
        face = bytearray(72)
        struct.pack_into('<HH', face, 0, c['flags'], c['flat'])
        struct.pack_into('<3i', face, 4, *(p[3] for p in c['points']))
        struct.pack_into('<3i', face, 16, *(p[4] for p in c['points']))
        struct.pack_into('<6h', face, 28, 0, 1, 2, *(n - c['start'] for n in c['indices']))
        struct.pack_into('<I', face, 68, 0x2006000)
        machine.mem_write(0x2001000, bytes(112))
        for offset, value in [(12, 1), (16, 0x2002000), (24, 0x2004000 + c['start'] * 8)]:
            word(0x2001000 + offset, value)
        machine.mem_write(0x2002000, bytes(face))
        for i, normal in enumerate(c['normals']):
            machine.mem_write(0x2004000 + i * 8, struct.pack('<4h', *normal, 0))
        for i, (flags, parameter, specular, constant, power) in enumerate(c['materials']):
            material = bytearray(76)
            struct.pack_into('<I', material, 16, flags)
            material[31] = parameter
            struct.pack_into('<iiB', material, 56, specular, constant, power)
            machine.mem_write(0x2006000 + i * 76, bytes(material))
        machine.mem_write(0xad3cf0, struct.pack('<11I', *c['counters']))
        machine.mem_write(0xad3d80, struct.pack('<3i', *c['lighting'][:3]))
        machine.mem_write(0xad3d90, struct.pack('<3i', *c['lighting'][3:6]))
        machine.mem_write(0xad3da0, struct.pack('<3i', *c['lighting'][6:]))
        for i, point in enumerate(c['points']):
            machine.mem_write(0xaaacb0 + i * 32, struct.pack('<8i', point[2], 0, 0, 0, 0, 0, *point[:2]))
        for address, value in [(0xad3d7c, 1), (0xad3dc4, 0), (0xad3d78, 0), (0xad3d74, 0)]:
            word(address, value)
        machine.mem_write(0xa760f0, bytes(104))
        machine.mem_write(0x2000800, struct.pack('<5I', 0x6000000, 0x2001000, 0x2003000, 0, 0))
        machine.reg_write(UC_X86_REG_ESP, 0x2000800)
        machine.emu_start(0x46b1d0, 0x6000000, count=100000)
        if machine.reg_read(UC_X86_REG_EIP) != 0x6000000:
            raise ValueError(f'Original face assembly did not return at case {test}')
        drawn = struct.unpack('<I', machine.mem_read(0xad3d78, 4))[0]
        expected = [0]
        if drawn:
            accepted += 1
            flags = struct.unpack('<I', machine.mem_read(0xa760f0, 4))[0]
            material = (struct.unpack('<I', machine.mem_read(0xa76154, 4))[0] - 0x2006000) // 76
            expected = [1, flags, material]
            for i in range(3):
                expected.extend(struct.unpack('<6i', machine.mem_read(0xa760f4 + i * 32, 24)))
            varying += bool(flags & 8)
            biased += bool(c['materials'][material][0] & 0x40)
            animated += material != 0
            reversed_faces += expected[9:11] == c['points'][2][:2] and c['points'][1][:2] != c['points'][2][:2]
        else:
            hidden_or_backface += 1
        actual = list(map(int, line.split()))
        if actual != expected:
            raise ValueError(f'Face mismatch at case {test}: actual={actual}, expected={expected}, input={lines[test]}')
    return dict(source_sha256=source_hash, function='0x46b1d0',
                code_sha256=hashlib.sha256(pe.get_data(0x6b1d0, 0x5a4)).hexdigest(),
                cases=len(cases), accepted=accepted, hidden_or_backface=hidden_or_backface,
                reversed=reversed_faces, varying_shade=varying, biased=biased, animated=animated,
                mismatches=0, mocked_functions=[hex(a) for a in mocked],
                scope='preprojected vertices and mesh-local light; material frames; hidden and back faces; wrapped determinant; depth bias; flags; flat/smooth normals; reversed vertex order; normalized UV')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable')
    parser.add_argument('--probe', required=True)
    parser.add_argument('--cases', type=int, default=4096)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.cases < 1:
        parser.error('--cases must be positive')
    result = check(args.executable, args.probe, args.cases)
    output = json.dumps(result, indent=2)
    print(output)
    if args.report:
        args.report.write_text(output + '\n')
