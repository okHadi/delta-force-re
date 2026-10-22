#!/usr/bin/env python3
"""Compare the mission steady gate with original 484740 instructions."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    source = args.executable.read_bytes()
    digest = hashlib.sha256(source).hexdigest()
    if digest != 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff':
        raise SystemExit('Unsupported executable hash')
    pe = pefile.PE(data=source)
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    machine.mem_map(0x400000, 0x1000000)
    machine.mem_write(0x400000, pe.get_memory_mapped_image())
    machine.mem_map(0x2000000, 0x2000)
    stack, stop = 0x2001000, 0x2000000

    def put(address, value):
        machine.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def run(address):
        put(stack, stop)
        machine.reg_write(UC_X86_REG_ESP, stack)
        machine.emu_start(address, stop, count=1000)
        if machine.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original function did not return')
        return machine.reg_read(UC_X86_REG_EAX)

    rng = random.Random(0x484740)
    cases = [(scope, control, weapon, aim, toggle) for weapon in range(39)
             for scope in (0, 1, 6, 0xffffffff) for control in (0, 1, 2, 0x400, 0x41e, 0x800)
             for aim in (0, 1) for toggle in (0, 1)]
    cases += [(rng.getrandbits(32), rng.getrandbits(32), rng.randrange(39), rng.randrange(2), rng.randrange(2))
              for _ in range(1000)]
    command = ''.join(' '.join(map(str, row))+'\n' for row in cases)
    native = subprocess.check_output([str(args.probe.resolve()), '--steady'], input=command, text=True)
    results = list(map(int, native.splitlines()))
    if len(results) != len(cases):
        raise RuntimeError('Native result count differs')
    put(0xad3ed0, 0x4f9934)
    put(0xc239b8, 0)
    toggles = 0
    for index, (scope, control, weapon, aim, toggle) in enumerate(cases):
        for address, value in ((0xc23a24, scope), (0xc23a00, control),
                               (0xc239c0, weapon), (0xc23b64, aim)):
            put(address, value)
        if toggle:
            run(0x4847c0)
            toggles += 1
            toggled = struct.unpack('<I', machine.mem_read(0xc23a24, 4))[0]
            if toggled != scope ^ 1:
                raise RuntimeError('Scope toggle differs')
        expected = run(0x484740)
        if results[index] != expected:
            raise RuntimeError(f'Case {index}: original {expected}, port {results[index]}')
    report = dict(source_sha256=digest, gate_cases=len(cases), toggle_cases=toggles,
                  mocked_functions=[], mismatches=0)
    if args.report:
        args.report.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
