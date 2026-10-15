#!/usr/bin/env python3
"""Compare six palette banks with original loaded texture and LOD registries."""
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
    u.mem_map(0x3000000, 0x800000)
    u.mem_map(0x6000000, 4096)

    def word(address, value):
        u.mem_write(address, struct.pack('<I', value))

    def call(address, *values):
        u.mem_write(0x200f000, struct.pack('<' + 'I' * (len(values) + 1), 0x6000000, *values))
        u.reg_write(UC_X86_REG_ESP, 0x200f000)
        u.reg_write(UC_X86_REG_FPCW, 0x037f)
        u.emu_start(address, 0x6000000, count=300000000)
        if u.reg_read(UC_X86_REG_EIP) != 0x6000000:
            raise ValueError(f'Original call did not return: {address:x}')

    word(0x500b0c, 0x2005000)
    word(0x500b10, 0x2004000)
    u.mem_write(0x2004000, bytes(range(256)))
    rng = random.Random(0x402940)
    cases = []
    payload = []
    for case in range(count):
        level = case % 4
        gamma = list(range(256)) if case % 2 else [i * i // 255 for i in range(256)]
        light = [rng.randrange(256) for _ in range(3)]
        models = []
        row = [case % 7, level, *gamma, *light]
        for number in range(case % 7):
            bank = (number + case) % 6
            textures = []
            for i in range((number + case) % 4):
                flags = rng.choice([1, 2, 4, 8, 15, 0x11, 0x2f])
                sequence = i if i % 2 else i + 256
                palette = [rng.randrange(256) for _ in range(768)]
                pixels = [rng.randrange(256) for _ in range([3, 16, 64][i % 3])]
                textures.append((flags, sequence, palette, pixels))
            materials = [(rng.choice([0, 1, 0x8000, 0x8001]),
                          [rng.randrange(256) for _ in range(12)] + [0, 1, 2, 255])
                         for _ in range((number + case) % 5)]
            models.append((bank, textures, materials))
            row.extend([bank, len(textures), len(materials)])
            for flags, sequence, palette, pixels in textures:
                row.extend([flags, sequence, len(pixels), *palette, *pixels])
            for flags, data in materials:
                row.extend([flags, *data])
        if case == 0:
            level = 0
            texture = (1, 7, [(i * 7 + channel * 19) % 256
                              for i in range(256) for channel in range(3)], [1] * 16)
            data = [100, 30, 22] * 4 + [7] * 4
            models = [(2, [texture], [(0, data), (1, data)])]
            flags, sequence, palette, pixels = texture
            row = [1, level, *gamma, *light, 2, 1, 2, flags, sequence, len(pixels),
                   *palette, *pixels, 0, *data, 1, *data]
        cases.append((level, gamma, models))
        payload.append(' '.join(map(str, row)))
    lines = subprocess.check_output([str(Path(probe).resolve())],
                                    input=('\n'.join(payload) + '\n').encode()).splitlines()
    if len(lines) != count:
        raise ValueError('Native case count differs')
    for case, ((level, gamma, models), line) in enumerate(zip(cases, lines)):
        u.mem_write(0xbf6f00, bytes(256 * 76))
        # The original loader retains selected textures and writes the loader bank.
        texture_index = 0
        heap = 0x3100000
        word(0xc80d10, level)
        u.mem_write(0x2005000, bytes(gamma))
        word(0xc65dcc, len(models))
        for model_index, (bank, textures, materials) in enumerate(models):
            selected = []
            for flags, sequence, palette, pixels in textures:
                if not flags & (1 << level):
                    continue
                selected.append(sequence)
                descriptor = bytearray(36)
                struct.pack_into('<I', descriptor, 16, len(pixels))
                struct.pack_into('<HBB', descriptor, 20, sequence, flags, bank)
                struct.pack_into('<II', descriptor, 28, heap, heap + len(pixels))
                u.mem_write(0xc54488 + texture_index * 36, bytes(descriptor))
                u.mem_write(heap, bytes(pixels) + bytes(palette))
                heap += len(pixels) + 768
                texture_index += 1
            material_ptr = heap
            for flags, data in materials:
                raw = bytearray(76)
                if flags & 1 and data[12 + level] not in selected:
                    flags &= ~1
                struct.pack_into('<I', raw, 16, flags)
                struct.pack_into('<H', raw, 26, bank)
                raw[40:56] = bytes(data)
                u.mem_write(heap, bytes(raw))
                heap += 76
            word(0xc65dd0 + model_index * 184 + 168, len(materials))
            word(0xc65dd0 + model_index * 184 + 172, material_ptr)
        word(0xc54480, texture_index)
        expected = []
        for bank in range(6):
            call(0x47ec10, 0x3000000)
            call(0x486670, bank)
            call(0x47edd0, bank, 1, 254)
            call(0x47e6d0, bank, 0, 0, 0, 0)
            call(0x47e6d0, bank, 255, 255, 255, 255)
            expected.extend(u.mem_read(0xbfbe00 + bank * 0x614, 768))
        actual = list(map(int, line.split()))
        if actual != expected:
            first = next(i for i, (a, b) in enumerate(zip(actual, expected)) if a != b)
            raise ValueError(f'Case {case}, byte {first}: native={actual[first]}, original={expected[first]}')
        print(f'case {case + 1}/{count}: six banks match', flush=True)
    return {'source_sha256': digest, 'cases': count, 'banks': count * 6,
            'mismatches': 0, 'addresses': ['486670', '47EDD0', '47E6D0']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('executable')
    parser.add_argument('probe')
    parser.add_argument('--count', type=int, default=8)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = check(args.executable, args.probe, args.count)
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
