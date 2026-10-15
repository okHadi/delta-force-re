#!/usr/bin/env python3
"""Compare state callbacks and timing with the original DF1 x86 dispatcher."""

import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess
import sys

import pefile
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP

SOURCE_HASH = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
FUNCTION = 0x461270
STACK, STATES, STUBS = 0x2000000, 0x3000000, 0x4000000


def emulator(data):
    pe = pefile.PE(data=data)
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    uc.mem_map(0x400000, 0x1000000)
    uc.mem_write(0x400000, pe.get_memory_mapped_image())
    pe.close()
    for address in (STACK, STATES, STUBS):
        uc.mem_map(address, 4096)
    write = lambda address, value: uc.mem_write(address, struct.pack('<I', value))
    read = lambda address: struct.unpack('<I', uc.mem_read(address, 4))[0]
    write(0x4c6784, STUBS + 0x10)  # GetTickCount
    write(0x4c6814, STUBS + 0x20)  # Sleep
    context = {}

    def callback(machine, address, size, user):
        cleanup, result = 0, 0
        if address == STUBS + 0x10:
            at = context['at']
            if at >= len(context['ticks']):
                raise ValueError('Original exhausted the clock fixture')
            result = context['ticks'][at]
            context['at'] += 1
        elif address == STUBS + 0x20:
            if read(machine.reg_read(UC_X86_REG_ESP) + 4) != 0:
                raise ValueError('Original sleep argument differs')
            context['trace'].append('y,')
            cleanup = 4
        else:
            index, kind = divmod(address - STUBS - 0x100, 16)
            if kind not in range(4) or index >= len(context['limits']):
                raise ValueError('Unexpected callback address')
            context['trace'].append(f'{"eurl"[kind]}{index},')
            if kind == 1:
                context['calls'][index] += 1
                result = int(context['calls'][index] == context['limits'][index])
            elif kind == 3:
                write(0x8ee1d4, STATES + (index + 1) * 64
                      if index + 1 < len(context['limits']) else 0)
        sp = machine.reg_read(UC_X86_REG_ESP)
        machine.reg_write(UC_X86_REG_EIP, read(sp))
        machine.reg_write(UC_X86_REG_ESP, sp + 4 + cleanup)
        machine.reg_write(UC_X86_REG_EAX, result)

    uc.hook_add(UC_HOOK_CODE, callback, begin=STUBS + 1, end=STUBS + 4095)

    def run(ticks, limits):
        context.update(ticks=ticks, limits=limits, calls=[0] * len(limits), at=0, trace=[])
        for index, limit in enumerate(limits):
            for kind in range(4):
                write(STATES + index * 64 + 32 + kind * 4,
                      0 if kind == 1 and limit == 0 else STUBS + 0x100 + index * 16 + kind)
        write(0x8ee1dc, 0)
        sp = STACK + 2048
        uc.mem_write(sp, struct.pack('<II', STUBS, STATES if limits else 0))
        uc.reg_write(UC_X86_REG_ESP, sp)
        uc.emu_start(FUNCTION, STUBS, timeout=2000000, count=1000000)
        if uc.reg_read(UC_X86_REG_EIP) != STUBS:
            raise ValueError('Original dispatcher did not return')
        return f'{"".join(context["trace"])} {read(0x8ee1dc)} {context["at"]}'
    return run


def cases():
    yield [], []
    yield [100, 100, 101, 115, 116, 148, 500, 500], [3, 1]
    yield [0xfffffff0, 0x10, 0x20], [4]
    yield [1, 2, 3, 4], [0, 0]
    rng = random.Random(0xdf1461270)
    for _ in range(200):
        limits = [rng.randrange(0, 30) for _ in range(rng.randrange(1, 10))]
        now = rng.choice([0, 0xfffffff0, rng.getrandbits(32)])
        ticks = [now]
        for _ in range(4096):
            now = (now + rng.choice([0, 1, 7, 15, 16, 17, 32, 128])) & 0xffffffff
            ticks.append(now)
        yield ticks, limits


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('probe', type=Path)
    args = parser.parse_args()
    try:
        data = args.executable.read_bytes()
        if hashlib.sha256(data).hexdigest() != SOURCE_HASH:
            raise ValueError('Expected the documented Df.exe SHA-256')
        fixtures = list(cases())
        result = subprocess.run([str(args.probe.resolve())], check=True, capture_output=True,
                                text=True, input=''.join(' '.join(map(str, ticks)) + '\n' +
                                ' '.join(map(str, limits)) + '\n' for ticks, limits in fixtures))
        lines = result.stdout.splitlines()
        if len(lines) != len(fixtures):
            raise ValueError('Probe result count differs')
        original = emulator(data)
        mismatches = [index for index, ((ticks, limits), actual) in
                      enumerate(zip(fixtures, lines)) if original(ticks, limits) != actual]
        print(json.dumps({'source_sha256': SOURCE_HASH, 'procedure_va': hex(FUNCTION),
                          'unicorn_version': unicorn.__version__, 'cases': len(fixtures),
                          'mismatch_count': len(mismatches), 'mismatches': mismatches[:10],
                          'scope': 'Callback order, tick reads, yield calls, elapsed counter; '
                                   'clock and state callbacks stubbed'}, indent=2))
        return int(bool(mismatches))
    except (OSError, ValueError, pefile.PEFormatError, unicorn.UcError,
            subprocess.CalledProcessError) as error:
        print(f'check_state_loop: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
