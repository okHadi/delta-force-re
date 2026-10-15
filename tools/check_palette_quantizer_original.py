#!/usr/bin/env python3
"""Compare palette boxes and colors with the original DF1 x86 code."""
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
    pe = pefile.PE(data=source)
    u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    u.mem_map(0x400000, 0x1000000)
    u.mem_write(0x400000, pe.get_memory_mapped_image())
    u.mem_map(0x2000000, 0x10000)
    u.mem_map(0x3000000, 0x100000)
    u.mem_map(0x6000000, 4096)
    u.mem_write(0x2004000, bytes(range(256)))
    u.mem_write(0x500b10, struct.pack('<I', 0x2004000))

    def call(address, *values):
        u.mem_write(0x200f000, struct.pack('<' + 'I' * (len(values) + 1), 0x6000000,
                                         *(v & 0xffffffff for v in values)))
        u.reg_write(UC_X86_REG_ESP, 0x200f000)
        u.reg_write(UC_X86_REG_FPCW, 0x037f)
        u.emu_start(address, 0x6000000, count=300000000)
        if u.reg_read(UC_X86_REG_EIP) != 0x6000000:
            raise ValueError(f'Original palette call did not return: {address:x}')

    rng = random.Random(0x47edd0)
    cases = [(4, []), (254, []), (1, [(255, 0, 0, 65536)]),
             (254, [(i % 256, (i * 37) % 256, (i * 113) % 256, 65536)
                    for i in range(512)]),
             (254, [])]
    for i in range(count):
        colors = [2, 4, 8, 16, 32, 64, 254][i % 7]
        entries = [(rng.randrange(256), rng.randrange(256), rng.randrange(256),
                    rng.choice([0, 1, 4095, 4096, 65536, 1048576]))
                   for _ in range(rng.randrange(1, 130))]
        cases.append((colors, entries))
        if i % 7 == 6:
            cases.append((254, []))  # Retain unused boxes from the previous bank.
    payload = ''.join(' '.join(map(str, [colors, len(entries),
                                       *(v for entry in entries for v in entry)])) + '\n'
                      for colors, entries in cases)
    lines = subprocess.check_output([str(Path(probe).resolve()), '--boxes'],
                                    input=payload.encode()).splitlines()
    if len(lines) != len(cases):
        raise ValueError('Native palette case count differs')
    total_boxes = 0
    for number, ((colors, entries), line) in enumerate(zip(cases, lines)):
        call(0x47ec10, 0x3000000)
        for entry in entries:
            call(0x47e660, *entry)
        call(0x47edd0, 0, 1, colors)
        active = struct.unpack('<I', u.mem_read(0x500b00, 4))[0]
        expected = [active, struct.unpack('<I', u.mem_read(0x500b08, 4))[0],
                    struct.unpack('<I', u.mem_read(0xc22adc, 4))[0]]
        expected.extend(u.mem_read(0xbfbe03, colors * 3))
        for i in range(active):
            raw = u.mem_read(0xbf6f00 + i * 76, 76)
            expected.extend(struct.unpack('<6Q6iI', raw))
        actual = list(map(int, line.split()))
        if actual != expected:
            first = next((i for i, (a, b) in enumerate(zip(actual, expected)) if a != b),
                         min(len(actual), len(expected)))
            raise ValueError(f'Case {number}, field {first}: native={actual[first:first+1]}, '
                             f'original={expected[first:first+1]}')
        total_boxes += active
        print(f'case {number + 1}/{len(cases)}: {active} boxes', flush=True)
    return {'source_sha256': digest, 'cases': len(cases), 'boxes': total_boxes,
            'mismatches': 0, 'addresses': ['47EC10', '47E660', '47EDD0', '47EF30']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('executable')
    parser.add_argument('probe')
    parser.add_argument('--count', type=int, default=21)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = check(args.executable, args.probe, args.count)
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
