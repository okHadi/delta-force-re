#!/usr/bin/env python3
"""Compare the rebuilt filename helper with the original DF1 x86 instructions."""

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
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP,
    UC_X86_REG_EIP, UC_X86_REG_EFLAGS,
)

SOURCE_HASH = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
FUNCTION = 0x456560
FUNCTION_BYTES = 144
STACK = 0x2000000
DATA = 0x3000000
STOP = 0x4000000


def corpus():
    pairs = [(name, extension) for name in
             ('', 'a', 'test', '.hidden', 'a.', 'a.b', 'a.b.c', 'dir.name/file',
              'dir/file', 'A_B-9', 'file\\name', 'a' * 255)
             for extension in ('', 'pcx', 'PCX', 'trn', 'x.y', '.pcx')]
    rng = random.Random(0xdf1998)
    alphabet = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-./\\ :!@#[]'
    for _ in range(2000):
        name = ''.join(rng.choice(alphabet) for _ in range(rng.randrange(0, 256)))
        extension = ''.join(rng.choice(alphabet) for _ in range(rng.randrange(0, 33)))
        pairs.append((name, extension))
    return pairs


def original_emulator(data):
    pe = pefile.PE(data=data)
    offset = pe.get_offset_from_rva(FUNCTION - pe.OPTIONAL_HEADER.ImageBase)
    code = data[offset:offset + FUNCTION_BYTES]
    pe.close()
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    machine.mem_map(FUNCTION & ~0xfff, 4096)
    machine.mem_write(FUNCTION, code)
    machine.mem_map(STACK, 4096)
    machine.mem_map(DATA, 8192)
    machine.mem_map(STOP, 4096)

    def run(name, extension):
        machine.mem_write(DATA, b'\xa5' * 512)
        machine.mem_write(DATA, name.encode('ascii') + b'\0')
        machine.mem_write(DATA + 4096, extension.encode('ascii') + b'\0')
        sp = STACK + 2048
        machine.mem_write(sp, struct.pack('<III', STOP, DATA, DATA + 4096))
        for reg in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
            machine.reg_write(reg, 0)
        machine.reg_write(UC_X86_REG_ESP, sp)
        machine.reg_write(UC_X86_REG_EFLAGS, 2)
        machine.emu_start(FUNCTION, STOP, timeout=1000000, count=100000)
        if machine.reg_read(UC_X86_REG_EIP) != STOP:
            raise RuntimeError('Original helper did not return within the instruction limit')
        output = bytes(machine.mem_read(DATA, 512))
        if b'\0' not in output:
            raise RuntimeError('Original output has no terminator')
        return output.split(b'\0', 1)[0].decode('ascii')
    return run, hashlib.sha256(code).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_exe', type=Path)
    parser.add_argument('probe', type=Path)
    args = parser.parse_args()
    try:
        data = args.game_exe.read_bytes()
        if hashlib.sha256(data).hexdigest() != SOURCE_HASH:
            raise ValueError('Expected the documented DF1 executable SHA-256')
        original, code_hash = original_emulator(data)
        pairs = corpus()
        result = subprocess.run([str(args.probe.resolve())],
                                input=''.join(f'{n}\t{e}\n' for n, e in pairs),
                                text=True, capture_output=True, check=True)
        rebuilt = result.stdout.splitlines()
        if len(rebuilt) != len(pairs):
            raise ValueError('Probe returned an unexpected number of results')
        mismatches = []
        for (name, extension), actual in zip(pairs, rebuilt):
            expected = original(name, extension)
            if actual != expected:
                mismatches.append({'name': name, 'extension': extension,
                                   'original': expected, 'rebuilt': actual})
        print(json.dumps({'source_sha256': SOURCE_HASH, 'procedure_va': hex(FUNCTION),
                          'procedure_bytes': FUNCTION_BYTES, 'procedure_sha256': code_hash,
                          'unicorn_version': unicorn.__version__, 'cases': len(pairs),
                          'mismatch_count': len(mismatches), 'mismatches': mismatches[:10],
                          'scope': 'ASCII strings without NUL; output bytes only'}, indent=2))
        return int(bool(mismatches))
    except (OSError, ValueError, RuntimeError, pefile.PEFormatError,
            unicorn.UcError, subprocess.CalledProcessError) as error:
        print(f'check_filename: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
