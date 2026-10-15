#!/usr/bin/env python3
"""Replay original texture and flat-color gathering through palette output."""
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
    digest = hashlib.sha256(source).hexdigest()
    if digest != 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff':
        raise ValueError('Unsupported original executable hash')
    u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    u.mem_map(0x400000, 0x1000000)
    u.mem_write(0x400000, pefile.PE(data=source).get_memory_mapped_image())
    u.mem_map(0x2000000, 0x20000)
    u.mem_map(0x3000000, 0x100000)
    u.mem_map(0x6000000, 4096)
    u.mem_write(0x2004000, bytes(range(256)))
    u.mem_write(0x500b10, struct.pack('<I', 0x2004000))
    u.mem_write(0x500b0c, struct.pack('<I', 0x2005000))

    def call(address, *values):
        u.mem_write(0x200f000, struct.pack('<' + 'I' * (len(values) + 1), 0x6000000,
                                         *(v & 0xffffffff for v in values)))
        u.reg_write(UC_X86_REG_ESP, 0x200f000)
        u.reg_write(UC_X86_REG_FPCW, 0x037f)
        u.emu_start(address, 0x6000000, count=300000000)
        if u.reg_read(UC_X86_REG_EIP) != 0x6000000:
            raise ValueError(f'Original palette call did not return: {address:x}')

    rng = random.Random(0x486670)
    cases = []
    for i in range(count):
        gamma = list(range(256)) if i % 3 == 0 else [rng.randrange(256) for _ in range(256)]
        flats = [([rng.randrange(256) for _ in range(3)], rng.choice([0, 4096, 65536]))
                 for _ in range(i % 9)]
        textures = []
        for j in range(i % 4):
            palette = [rng.randrange(256) for _ in range(768)]
            size = [1, 3, 16, 64, 256, 1024][(i + j) % 6]
            pixels = [0] * size if i % 2 else [rng.randrange(256) for _ in range(size)]
            textures.append((palette, pixels))
        cases.append(([4, 16, 64, 254][i % 4], gamma, flats, textures))
    payload = []
    for colors, gamma, flats, textures in cases:
        row = [colors, len(flats), len(textures), *gamma]
        for rgb, weight in flats:
            row.extend([*rgb, weight])
        for palette, pixels in textures:
            row.extend([len(pixels), *palette, *pixels])
        payload.append(' '.join(map(str, row)))
    native = subprocess.check_output([str(Path(probe).resolve())],
                                    input=('\n'.join(payload) + '\n').encode()).splitlines()
    if len(native) != count:
        raise ValueError('Native case count differs')
    total_boxes = total_textures = 0
    for number, ((colors, gamma, flats, textures), line) in enumerate(zip(cases, native)):
        call(0x47ec10, 0x3000000)
        u.mem_write(0x2005000, bytes(gamma))
        for rgb, weight in flats:
            u.mem_write(0x2006000, bytes(rgb))
            call(0x47e8e0, 0x2006000, 1, weight, 10)
        for palette, pixels in textures:
            u.mem_write(0x2006000, bytes(palette))
            u.mem_write(0x2010000, bytes(pixels))
            call(0x47eaa0, 0x2006000, 0x2010000, len(pixels), 65536, 10)
        call(0x47edd0, 0, 1, colors)
        active = struct.unpack('<I', u.mem_read(0x500b00, 4))[0]
        expected = [active, struct.unpack('<I', u.mem_read(0x500b08, 4))[0],
                    struct.unpack('<I', u.mem_read(0xc22adc, 4))[0]]
        expected.extend(u.mem_read(0xbfbe03, colors * 3))
        for i in range(active):
            expected.extend(struct.unpack('<6Q6iI', u.mem_read(0xbf6f00 + i * 76, 76)))
        actual = list(map(int, line.split()))
        if actual != expected:
            first = next((i for i, (a, b) in enumerate(zip(actual, expected)) if a != b),
                         min(len(actual), len(expected)))
            raise ValueError(f'Case {number}, field {first}: native={actual[first:first+1]}, '
                             f'original={expected[first:first+1]}')
        total_boxes += active
        total_textures += len(textures)
        print(f'case {number + 1}/{count}: {active} boxes, {len(textures)} textures', flush=True)
    return {'source_sha256': digest, 'cases': count, 'textures': total_textures,
            'boxes': total_boxes, 'mismatches': 0,
            'addresses': ['47E8E0', '47EAA0', '47EDD0']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('executable')
    parser.add_argument('probe')
    parser.add_argument('--count', type=int, default=16)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = check(args.executable, args.probe, args.count)
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
