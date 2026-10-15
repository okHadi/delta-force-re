#!/usr/bin/env python3
"""Compare terrain row coverage with original 489260. No game process or sound."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP

SHA = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'


def check(executable, probe, count):
    source = executable.read_bytes()
    if hashlib.sha256(source).hexdigest() != SHA:
        raise ValueError('Unsupported original executable')
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0x400000, 0x1000000)
    cpu.mem_write(0x400000, pefile.PE(data=source).get_memory_mapped_image())
    cpu.mem_map(0x2000000, 0x10000)
    cpu.mem_map(0x3000000, 0x1000)
    cpu.mem_map(0x4000000, 0x1000)

    def word(address, value):
        cpu.mem_write(address, struct.pack('<I', value & 0xffffffff))

    word(0xc94aa0, 0x3000000)
    rng = random.Random(0x489260)
    inputs, expected = [], []
    for case in range(count):
        height = rng.choice([1, 2, 8, 48, 480, 768])
        top = rng.randrange(height)
        bottom = rng.randrange(top, height)
        entries = [(rng.randrange(top, bottom + 2), rng.randrange(0x3ff0001))
                   for _ in range(case % 15)]
        inputs.append(' '.join(map(str, [height, top, bottom, len(entries),
                                        *[v for entry in entries for v in entry]])))
        word(0x3000038, top)
        word(0x3000040, bottom)
        word(0xcb89c0, len(entries))
        cpu.mem_write(0xc93ea0, bytes(768 * 4))
        for index, (row, depth) in enumerate(entries):
            word(0xcb89f0 + index * 0x30, row)
            word(0xcb89d0 + index * 0x30, depth)
        word(0x2008000, 0x4000000)
        cpu.reg_write(UC_X86_REG_ESP, 0x2008000)
        cpu.emu_start(0x489260, 0x4000000, timeout=1000000, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != 0x4000000:
            raise ValueError(f'Original case {case} did not return')
        expected.append(list(struct.unpack('<' + 'I' * height, cpu.mem_read(0xc93ea0, height * 4))))
    run = subprocess.run([str(probe.resolve()), '--row-depths32'],
                         input='\n'.join(inputs) + '\n', text=True, capture_output=True,
                         check=True, timeout=60)
    actual = [list(map(int, line.split())) for line in run.stdout.splitlines()]
    if len(actual) != len(expected):
        raise ValueError('Probe case count differs')
    for case, (got, want) in enumerate(zip(actual, expected)):
        if got != want:
            raise ValueError(f'Row coverage differs in case {case}')
    producer_inputs, producer_expected = [], []
    cpu.mem_write(0xcb79a8, b'\0')
    for case in range(count):
        stride = rng.choice([1, 24, 128, 640, 1024, 65535])
        offsets = [rng.randrange(-stride * 8, min(0x7fffffff, stride * 768))
                   for _ in range(case % 33)]
        if case % 17 == 0:
            offsets = [-2147483648, 2147483647]
        depth = rng.randrange(0x3ff0001)
        producer_inputs.append(' '.join(map(str, [stride, depth, len(offsets), *offsets])))
        word(0x3000030, stride)
        word(0x3000034, 0)
        word(0x300003c, 2 * len(offsets) - 1)
        for index, offset in enumerate(offsets):
            word(0xcb8d8c + index * 0x30, offset)
        cpu.mem_write(0x2008000, struct.pack('<II', 0x4000000, 0))
        cpu.reg_write(UC_X86_REG_ESP, 0x2008000)
        cpu.emu_start(0x4892e0, 0x4000000, timeout=1000000, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != 0x4000000:
            raise ValueError(f'Original producer case {case} did not return')
        producer_expected.append([struct.unpack('<i', cpu.mem_read(0xcb89f0, 4))[0], depth])
    run = subprocess.run([str(probe.resolve()), '--cache-row32'],
                         input='\n'.join(producer_inputs) + '\n', text=True,
                         capture_output=True, check=True, timeout=60)
    producer_actual = [list(map(int, line.split())) for line in run.stdout.splitlines()]
    if producer_actual != producer_expected:
        raise ValueError('Terrain cache row producer differs')
    return dict(source_sha256=SHA, addresses=['489260', '4892E0'], cases=count,
                producer_cases=count,
                row_values=sum(map(len, expected)), mismatches=0)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--cases', type=int, default=512)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.cases <= 0:
        parser.error('cases must be positive')
    result = check(args.executable, args.probe, args.cases)
    if args.report:
        args.report.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
