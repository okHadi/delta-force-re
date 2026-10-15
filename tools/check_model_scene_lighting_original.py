#!/usr/bin/env python3
"""Compare camera-space light and half-vector setup with original x86."""
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
    digest = hashlib.sha256(source).hexdigest()
    if digest != 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff':
        raise ValueError('Unsupported original executable hash')
    u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    u.mem_map(0x400000, 0x1000000)
    u.mem_write(0x400000, pefile.PE(data=source).get_memory_mapped_image())
    u.mem_map(0x2000000, 0x10000)
    u.mem_map(0x6000000, 4096)
    rng = random.Random(0x46bb20)
    cases = []
    for test in range(count):
        matrix = [rng.randrange(-0x400000, 0x400001) for _ in range(12)]
        direction = [rng.randrange(-65536, 65537) for _ in range(3)]
        if test % 4 == 0:
            matrix = [4194304, 0, 0, 1234, 0, 4194304, 0, -5678, 0, 0, 4194304, 9012]
            direction = [[-46340, 0, 46340], [65536, 0, 0], [65535, 0, 0], [65534, 0, 0]][(test // 4) % 4]
        strengths = [rng.randrange(65537) for _ in range(3)]
        cases.append((matrix, direction, strengths))
    payload = ''.join(' '.join(map(str, [*matrix, *direction, *strengths])) + '\n'
                      for matrix, direction, strengths in cases)
    lines = subprocess.check_output([str(Path(probe).resolve()), 'scene-lighting'],
                                    input=payload.encode()).splitlines()
    if len(lines) != count:
        raise ValueError('Native case count differs')
    zero = 0
    for number, ((matrix, direction, strengths), line) in enumerate(zip(cases, lines)):
        u.mem_write(0x8d1340, struct.pack('<12i', *matrix))
        u.mem_write(0x2003000, struct.pack('<3i', *direction))
        u.mem_write(0x200f000, struct.pack('<7I', 0x6000000, 0x2003000, *strengths, 0, 0))
        u.reg_write(UC_X86_REG_ESP, 0x200f000)
        u.emu_start(0x46bb20, 0x6000000, count=100000)
        if u.reg_read(UC_X86_REG_EIP) != 0x6000000:
            raise ValueError('Original light setup did not return')
        expected = [*struct.unpack('<3i', u.mem_read(0xad3d40, 12)),
                    *struct.unpack('<3i', u.mem_read(0xad3d50, 12)),
                    *struct.unpack('<3i', u.mem_read(0xad3d60, 12))]
        actual = list(map(int, line.split()))
        if actual != expected:
            raise ValueError(f'Case {number}: native={actual}, original={expected}')
        zero += expected[3:6] == [0, 0, 0]
    return {'source_sha256': digest, 'cases': count, 'zero_half_vectors': zero,
            'mismatches': 0, 'addresses': ['46BB20', '46BA50', '45CF30', '477620']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('executable')
    parser.add_argument('probe')
    parser.add_argument('--count', type=int, default=4096)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = check(args.executable, args.probe, args.count)
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
