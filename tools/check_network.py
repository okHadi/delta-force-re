#!/usr/bin/env python3
"""Compare the direct datagram encoder with original x86 code; stub all sockets."""

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
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP,
    UC_X86_REG_EIP, UC_X86_REG_EFLAGS,
)

SOURCE_HASH = 'fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2'
FUNCTION = 0x10005020
STACK, DATA, STOP = 0x2000000, 0x3000000, 0x4000000


def corpus():
    pairs = []
    for length in (0, 1, 2, 3, 4, 7, 15, 31, 32, 33, 63, 64, 255, 256, 512, 1024, 2048):
        pairs.extend((bytes(length), b'\xff' * length,
                      bytes(i & 255 for i in range(length)),
                      bytes((0xaa, 0x55)[i & 1] for i in range(length))))
    rng = random.Random(0xdf1a01)
    for _ in range(400):
        pairs.append(bytes(rng.randrange(256) for _ in range(rng.randrange(0, 2049))))
    return pairs


def original_emulator(data):
    pe = pefile.PE(data=data)
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    base = pe.OPTIONAL_HEADER.ImageBase
    size = (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095
    machine.mem_map(base, size)
    machine.mem_write(base, pe.get_memory_mapped_image())
    pe.close()
    machine.mem_map(STACK, 4096)
    machine.mem_map(DATA, 16384)
    machine.mem_map(STOP, 4096)
    machine.mem_write(0x10019854, struct.pack('<I', DATA + 4096))
    machine.mem_write(0x1001985c, struct.pack('<I', 8192))
    machine.mem_write(0x10019868, struct.pack('<H', 12345))
    machine.mem_write(0x100096b4, struct.pack('<I', 42))
    capture = []

    def finish_call(argument_bytes, value):
        sp = machine.reg_read(UC_X86_REG_ESP)
        address = struct.unpack('<I', machine.mem_read(sp, 4))[0]
        machine.reg_write(UC_X86_REG_ESP, sp + 4 + argument_bytes)
        machine.reg_write(UC_X86_REG_EAX, value)
        machine.reg_write(UC_X86_REG_EIP, address)

    def htons_stub(uc, address, size, user):
        sp = uc.reg_read(UC_X86_REG_ESP)
        value = struct.unpack('<I', uc.mem_read(sp + 4, 4))[0] & 0xffff
        finish_call(4, ((value & 255) << 8) | (value >> 8))

    def sendto_stub(uc, address, size, user):
        sp = uc.reg_read(UC_X86_REG_ESP)
        socket, buffer, length, flags, destination, address_size = struct.unpack(
            '<6I', uc.mem_read(sp + 4, 24))
        if socket != 42 or length > 8192 or flags != 0 or address_size != 16:
            raise RuntimeError('Unexpected original sendto arguments')
        capture.append(bytes(uc.mem_read(buffer, length)))
        finish_call(24, length)

    machine.hook_add(UC_HOOK_CODE, htons_stub, begin=0x1000592e, end=0x1000592e)
    machine.hook_add(UC_HOOK_CODE, sendto_stub, begin=0x10005952, end=0x10005952)

    def run(payload):
        capture.clear()
        machine.mem_write(DATA, payload or b'\0')
        machine.mem_write(DATA + 4096, bytes(4096))
        sp = STACK + 2048
        machine.mem_write(sp, struct.pack('<4I', STOP, DATA, len(payload), 0x0100007f))
        for reg in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
            machine.reg_write(reg, 0)
        machine.reg_write(UC_X86_REG_ESP, sp)
        machine.reg_write(UC_X86_REG_EFLAGS, 2)
        machine.emu_start(FUNCTION, STOP, timeout=1000000, count=1000000)
        if machine.reg_read(UC_X86_REG_EIP) != STOP or machine.reg_read(UC_X86_REG_EAX) != 0:
            raise RuntimeError('Original encoder did not return success')
        if not payload and not capture:
            return b''
        if len(capture) != 1 or len(capture[0]) != len(payload) + 4:
            raise RuntimeError('Unexpected original datagram length or send count')
        return capture[0]
    return run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('network_dll', type=Path)
    parser.add_argument('probe', type=Path)
    args = parser.parse_args()
    try:
        data = args.network_dll.read_bytes()
        if hashlib.sha256(data).hexdigest() != SOURCE_HASH:
            raise ValueError('Expected the documented Netsocknl.dll SHA-256')
        original = original_emulator(data)
        cases = corpus()
        result = subprocess.run([str(args.probe.resolve())],
                                input=''.join(payload.hex() + '\n' for payload in cases),
                                capture_output=True, text=True, check=True)
        lines = result.stdout.splitlines()
        if len(lines) != len(cases):
            raise ValueError('Probe returned an unexpected number of results')
        mismatches = []
        for index, (payload, line) in enumerate(zip(cases, lines)):
            actual, expected = bytes.fromhex(line), original(payload)
            if actual != expected:
                mismatches.append({'case': index, 'length': len(payload)})
        print(json.dumps({'source_sha256': SOURCE_HASH, 'procedure_va': hex(FUNCTION),
                          'unicorn_version': unicorn.__version__, 'cases': len(cases),
                          'mismatch_count': len(mismatches), 'mismatches': mismatches[:10],
                          'scope': 'Datagram bytes; htons/sendto stubbed; no network traffic'}, indent=2))
        return int(bool(mismatches))
    except (OSError, ValueError, RuntimeError, pefile.PEFormatError, unicorn.UcError,
            subprocess.CalledProcessError) as error:
        print(f'check_network: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
