#!/usr/bin/env python3
"""Compare the send-compressor port with original Netsocknl.dll + Cmprssh0.dll.

Runs the original send selector FUN_10005860 (0x10005860) and the original
extended encoder FUN_10005120 (0x10005120) under Unicorn. The selector's
compress_buffer call ([0x100096a8]) is answered by running the original
Cmprssh0.dll compress_buffer (0x100012e0) in a second Unicorn machine, so the
whole chain is original x86 code. sendto/htons are stubbed and captured; no
sockets are opened and both DLLs are read read-only.

Compared against tools/network_send_probe: status, wire path, flag, wire
length and every datagram byte. Exit code 0 only when everything matches.
"""

import argparse
import hashlib
import json
import random
import struct
import subprocess
import sys
from pathlib import Path

import pefile
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP,
    UC_X86_REG_EIP, UC_X86_REG_EFLAGS,
)

NETSOCK_HASH = 'fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2'
CODEC_HASH = '4dbc4de8458b92c4b07933fe6ac6132912f51e3490c511c65731ac12e13539a6'
SELECTOR = 0x10005860
EXTENDED = 0x10005120
SIMPLE = 0x10005020
SENDTO = 0x10005952
HTONS = 0x1000592E
WSAGETLASTERROR = 0x10005916
COMPRESS_PTR = 0x100096A8
COMPRESS_LOADED = 0x10019860
BUFFER_PTR = 0x10019854
BUFFER_CAPACITY = 0x1001985C
PORT_GLOBAL = 0x10019868
SOCKET = 0x100096B4
CODEC_INIT = 0x10001150
CODEC_COMPRESS = 0x100012E0
STACK, DATA, STOP, STUB = 0x2000000, 0x3000000, 0x4000000, 0x5000000
SRC = DATA
BUF = DATA + 0x20000
CSTACK, CDATA, COUT, CSTOP = 0x20000000, 0x30000000, 0x31000000, 0x40000000
SOCKET_ID = 42
BUFFER_BYTES = 8192
OUT_CAPACITY = 0x10000
INSTRUCTION_BUDGET = 20_000_000
TIME_BUDGET_US = 10_000_000


class Codec:
    """Original Cmprssh0.dll compress_buffer in its own machine."""

    def __init__(self, data):
        pe = pefile.PE(data=data)
        self.machine = Uc(UC_ARCH_X86, UC_MODE_32)
        base = pe.OPTIONAL_HEADER.ImageBase
        size = (pe.OPTIONAL_HEADER.SizeOfImage + 0xFFF) & ~0xFFF
        self.machine.mem_map(base, size)
        self.machine.mem_write(base, pe.get_memory_mapped_image())
        pe.close()
        for region, span in ((CSTACK, 0x10000), (CDATA, 0x200000), (COUT, 0x20000), (CSTOP, 0x1000)):
            self.machine.mem_map(region, span)
        if self._call(CODEC_INIT, []) is None:
            raise RuntimeError('original init_compression did not return')

    def _call(self, fn, arguments):
        machine = self.machine
        sp = CSTACK + 0x8000
        machine.mem_write(sp, struct.pack('<%dI' % (len(arguments) + 1), CSTOP, *arguments))
        for register in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                         UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
            machine.reg_write(register, 0)
        machine.reg_write(UC_X86_REG_ESP, sp)
        machine.reg_write(UC_X86_REG_EFLAGS, 2)
        machine.emu_start(fn, CSTOP, timeout=TIME_BUDGET_US, count=INSTRUCTION_BUDGET)
        if machine.reg_read(UC_X86_REG_EIP) != CSTOP:
            return None
        return machine.reg_read(UC_X86_REG_EAX) & 0xFFFFFFFF

    def compress(self, payload):
        """Returns (bits, stream bytes) exactly like the original call."""
        self.machine.mem_write(CDATA, payload + bytes(16))
        self.machine.mem_write(COUT, bytes(OUT_CAPACITY + 16))
        value = self._call(CODEC_COMPRESS, [CDATA, len(payload), COUT, OUT_CAPACITY])
        if value is None:
            raise RuntimeError('original compress_buffer did not return')
        bits = value
        wire = (bits + 7) // 8
        if wire > OUT_CAPACITY:
            raise RuntimeError('original compress_buffer exceeded its window')
        return bits, bytes(self.machine.mem_read(COUT, wire))


class Sender:
    """Original Netsocknl.dll selector/encoder with the codec answered for real."""

    def __init__(self, data, codec):
        self.codec = codec
        pe = pefile.PE(data=data)
        self.machine = Uc(UC_ARCH_X86, UC_MODE_32)
        base = pe.OPTIONAL_HEADER.ImageBase
        size = (pe.OPTIONAL_HEADER.SizeOfImage + 0xFFF) & ~0xFFF
        self.machine.mem_map(base, size)
        self.machine.mem_write(base, pe.get_memory_mapped_image())
        pe.close()
        for region, span in ((STACK, 0x10000), (DATA, 0x40000), (STOP, 0x1000), (STUB, 0x1000)):
            self.machine.mem_map(region, span)
        self.machine.mem_write(BUFFER_PTR, struct.pack('<I', BUF))
        self.machine.mem_write(BUFFER_CAPACITY, struct.pack('<I', BUFFER_BYTES))
        self.machine.mem_write(SOCKET, struct.pack('<I', SOCKET_ID))
        self.machine.mem_write(COMPRESS_PTR, struct.pack('<I', STUB))
        self.captured = []
        self.last_bits = 0
        self.port = 0x3039
        machine = self.machine
        machine.hook_add(UC_HOOK_CODE, self._htons_stub, begin=HTONS, end=HTONS)
        machine.hook_add(UC_HOOK_CODE, self._sendto_stub, begin=SENDTO, end=SENDTO)
        machine.hook_add(UC_HOOK_CODE, self._wsa_stub, begin=WSAGETLASTERROR, end=WSAGETLASTERROR)
        machine.hook_add(UC_HOOK_CODE, self._compress_stub, begin=STUB, end=STUB)

    def _finish_call(self, argument_bytes, value):
        machine = self.machine
        sp = machine.reg_read(UC_X86_REG_ESP)
        address = struct.unpack('<I', machine.mem_read(sp, 4))[0]
        machine.reg_write(UC_X86_REG_ESP, sp + 4 + argument_bytes)
        machine.reg_write(UC_X86_REG_EAX, value)
        machine.reg_write(UC_X86_REG_EIP, address)

    def _htons_stub(self, uc, address, size, user):
        sp = uc.reg_read(UC_X86_REG_ESP)
        value = struct.unpack('<I', uc.mem_read(sp + 4, 4))[0] & 0xFFFF
        self._finish_call(4, ((value & 255) << 8) | (value >> 8))

    def _sendto_stub(self, uc, address, size, user):
        sp = uc.reg_read(UC_X86_REG_ESP)
        socket, buffer, length, flags, destination, address_size = struct.unpack(
            '<6I', uc.mem_read(sp + 4, 24))
        if socket != SOCKET_ID or length > BUFFER_BYTES + 8 or flags != 0 or address_size != 16:
            raise RuntimeError('Unexpected original sendto arguments')
        self.captured.append(bytes(uc.mem_read(buffer, length)))
        self._finish_call(24, length)

    def _wsa_stub(self, uc, address, size, user):
        self._finish_call(0, 0)

    def _compress_stub(self, uc, address, size, user):
        sp = uc.reg_read(UC_X86_REG_ESP)
        source, length, target, capacity = struct.unpack('<4I', uc.mem_read(sp + 4, 16))
        if capacity != OUT_CAPACITY:
            raise RuntimeError('Unexpected compress capacity')
        payload = bytes(uc.mem_read(source, length))
        bits, stream = self.codec.compress(payload)
        uc.mem_write(target, stream)
        self.last_bits = bits
        # cdecl: the selector adds 0x10 after the call, so only pop the return address.
        self._finish_call(0, bits)

    def _run(self, entry, arguments):
        machine = self.machine
        sp = STACK + 0x8000
        machine.mem_write(sp, struct.pack('<%dI' % (len(arguments) + 1), STOP, *arguments))
        for register in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                         UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
            machine.reg_write(register, 0)
        machine.reg_write(UC_X86_REG_ESP, sp)
        machine.reg_write(UC_X86_REG_EFLAGS, 2)
        machine.emu_start(entry, STOP, timeout=TIME_BUDGET_US, count=INSTRUCTION_BUDGET)
        if machine.reg_read(UC_X86_REG_EIP) != STOP:
            raise RuntimeError('Original function did not return')
        return machine.reg_read(UC_X86_REG_EAX) & 0xFFFFFFFF

    def _prepare(self, payload):
        self.machine.mem_write(SRC, bytes(len(payload) + 16))
        self.machine.mem_write(SRC, payload)
        self.machine.mem_write(BUF, bytes(BUFFER_BYTES + 8))
        self.machine.mem_write(PORT_GLOBAL, struct.pack('<H', self.port))
        self.captured.clear()
        self.last_bits = 0

    def select(self, payload, ip, compression_loaded):
        self._prepare(payload)
        self.machine.mem_write(COMPRESS_LOADED, struct.pack('<I', 1 if compression_loaded else 0))
        status = self._run(SELECTOR, [SRC, len(payload), ip])
        return status, list(self.captured), self.last_bits

    def extended(self, payload, ip, flag):
        self._prepare(payload)
        status = self._run(EXTENDED, [SRC, len(payload), ip, 1 if flag else 0])
        return status, list(self.captured), 0


def expected_view(mode, payload_length, bits, status, captured):
    """Derive (format, wire, flag, datagram) and validate the original send."""
    if status not in (0, 0x6A, 0x2738):
        raise RuntimeError('unexpected original status %#x' % status)
    if status != 0:
        if captured:
            raise RuntimeError('original sent on failure status %#x' % status)
        return 'none', 0, False, b''
    if len(captured) > 1:
        raise RuntimeError('original sent %d datagrams' % len(captured))
    if mode == 'select':
        wire = (bits + 7) // 8
        simple = payload_length + 4 <= wire + 10
        if not captured:
            if payload_length != 0:
                raise RuntimeError('original did not send for %d bytes' % payload_length)
            return 'none', 0, False, b''
        if simple:
            if len(captured[0]) != payload_length + 4:
                raise RuntimeError('simple send length mismatch')
            return 'simple', len(captured[0]), False, captured[0]
        if len(captured[0]) != wire + 10:
            raise RuntimeError('extended send length mismatch')
        return 'extended', len(captured[0]), (captured[0][8] & 1) == 1, captured[0]
    if not captured:
        if payload_length != 0:
            raise RuntimeError('original did not send for %d bytes' % payload_length)
        return 'none', 0, False, b''
    if len(captured[0]) != payload_length + 10:
        raise RuntimeError('extended encode length mismatch')
    return 'extended', len(captured[0]), (captured[0][8] & 1) == 1, captured[0]


def corpus():
    cases = []

    def add(label, payload):
        cases.append((label, bytes(payload)))

    add('empty', b'')
    for size in range(1, 41):
        add('ramp.%d' % size, bytes((i * 7) & 0xFF for i in range(size)))
    rng = random.Random(0xDF1A10)
    for size in (31, 32, 33, 63, 64, 65, 100, 127, 128, 255, 256, 511, 512, 1000, 2000):
        add('zeros.%d' % size, bytes(size))
        add('ones.%d' % size, bytes([0xFF]) * size)
        add('rand.%d' % size, bytes(rng.randrange(256) for _ in range(size)))
    for size in (64, 128, 256, 512, 1024, 2048):
        add('alt2.%d' % size, (b'\x01\x02' * (size // 2 + 1))[:size])
    for size in (4000, 8000, 8188, 8189, 9000, 12000, 20000):
        add('zeros.big.%d' % size, bytes(size))
        add('rand.big.%d' % size, bytes(rng.randrange(256) for _ in range(size)))
    add('text', b'GET / HTTP/1.1\r\nHost: game\r\n\r\n')
    return cases


def extended_corpus():
    cases = []
    for size in (0, 1, 2, 3, 4, 5, 31, 32, 33, 63, 64, 255, 1000, 8188, 8189):
        for flag in (0, 1):
            cases.append(('ramp.%d.%d' % (size, flag),
                          bytes((i * 7) & 0xFF for i in range(size)), flag))
    return cases


def probe_run(probe, args, lines):
    result = subprocess.run([str(probe.resolve())] + args,
                            input=''.join(line + '\n' for line in lines),
                            capture_output=True, text=True, check=False, timeout=600)
    if result.returncode != 0:
        raise RuntimeError('probe failed: %s' % result.stderr.strip())
    return [json.loads(line) for line in result.stdout.splitlines()]


def compare(sender, probe, report, mismatches, mode, cases, compression_loaded=True):
    lines = []
    expected = []
    for case in cases:
        label, payload = case[0], case[1]
        ip, port = 0x0100007F, 0x3039
        sender.port = port
        if mode == 'extended':
            flag = case[2]
            status, captured, bits = sender.extended(payload, ip, flag)
        else:
            status, captured, bits = sender.select(payload, ip, compression_loaded)
        fmt, wire, flag_bit, datagram = expected_view(mode, len(payload), bits, status, captured)
        expected.append({'label': label, 'status': status, 'format': fmt, 'wire': wire,
                         'flag': flag_bit, 'datagram': datagram})
        if mode == 'extended':
            lines.append('%08x %04x %d %s' % (ip, port, flag, payload.hex()))
        else:
            lines.append('%08x %04x %s' % (ip, port, payload.hex()))
    args = [mode]
    if mode == 'select' and not compression_loaded:
        args.append('--no-compression')
    got = probe_run(probe, args, lines)
    if len(got) != len(cases):
        raise RuntimeError('%s probe returned %d of %d results' % (mode, len(got), len(cases)))
    count_key = 'select-no-compression' if len(args) > 1 else mode
    report['probe_counts'][count_key] = len(got)
    for expectation, line in zip(expected, got):
        names = []
        if line['status'] != expectation['status']:
            names.append('status')
        if line['format'] != expectation['format']:
            names.append('format')
        if line['wire'] != expectation['wire']:
            names.append('wire')
        if bool(line['flag']) != expectation['flag']:
            names.append('flag')
        if bytes.fromhex(line['datagram']) != expectation['datagram']:
            names.append('datagram')
        if line['ip'] != '0100007f' or line['port'] != '3039':
            names.append('target')
        report['paths'][line['format']] = report['paths'].get(line['format'], 0) + 1
        if names:
            mismatches.append({'mode': mode, 'case': expectation['label'], 'fields': names,
                               'expected': {key: expectation[key] for key in ('status', 'format', 'wire')},
                               'got': {key: line[key] for key in ('status', 'format', 'wire', 'flag')}})
    return len(got)


def freeze_vectors(sender, probe):
    rng = random.Random(0xDF1A11)
    vectors = []

    def add(name, payload, mode, flag=None):
        vectors.append({'name': name, 'payload': bytes(payload), 'mode': mode, 'flag': flag})

    add('extended-hello.0', b'Hello', 'extended', 0)
    add('extended-hello.1', b'Hello', 'extended', 1)
    add('extended-40.0', bytes(rng.randrange(256) for _ in range(40)), 'extended', 0)
    add('extended-40.1', bytes(rng.randrange(256) for _ in range(40)), 'extended', 1)
    add('select-random.100', bytes(rng.randrange(256) for _ in range(100)), 'select')
    add('select-zeros.4000', bytes(4000), 'select')
    add('select-zeros.20000', bytes(20000), 'select')
    add('select-random.9000', bytes(rng.randrange(256) for _ in range(9000)), 'select')
    frozen = []
    for vector in vectors:
        sender.port = 0x3039
        if vector['mode'] == 'extended':
            status, captured, bits = sender.extended(vector['payload'], 0x0100007F, vector['flag'])
        else:
            status, captured, bits = sender.select(vector['payload'], 0x0100007F, True)
        fmt, wire, flag_bit, datagram = expected_view(vector['mode'], len(vector['payload']), bits,
                                                      status, captured)
        line = probe_run(probe, [vector['mode']], [
            '%08x %04x %s' % (0x0100007F, 0x3039,
                              ('%d %s' % (vector['flag'], vector['payload'].hex()))
                              if vector['mode'] == 'extended' else vector['payload'].hex())])[0]
        if (line['status'], line['format'], line['wire'], bool(line['flag']),
                bytes.fromhex(line['datagram'])) != (status, fmt, wire, flag_bit, datagram):
            raise RuntimeError('freeze vector %s disagrees with the probe' % vector['name'])
        frozen.append({
            'name': vector['name'],
            'mode': vector['mode'],
            'flag': vector['flag'],
            'payload': vector['payload'].hex() if len(vector['payload']) <= 256 else None,
            'payload_length': len(vector['payload']),
            'status': status,
            'format': fmt,
            'wire': wire,
            'datagram': datagram.hex() if wire <= 512 else None,
            'cmp_flag': flag_bit,
        })
    return frozen


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('netsock_dll', type=Path)
    parser.add_argument('codec_dll', type=Path)
    parser.add_argument('probe', type=Path)
    args = parser.parse_args()
    try:
        netsock = args.netsock_dll.read_bytes()
        codec_data = args.codec_dll.read_bytes()
        if hashlib.sha256(netsock).hexdigest() != NETSOCK_HASH:
            raise ValueError('Expected the documented Netsocknl.dll SHA-256')
        if hashlib.sha256(codec_data).hexdigest() != CODEC_HASH:
            raise ValueError('Expected the documented Cmprssh0.dll SHA-256')
        codec = Codec(codec_data)
        sender = Sender(netsock, codec)
        report = {'netsock_sha256': NETSOCK_HASH, 'codec_sha256': CODEC_HASH,
                  'selector_va': hex(SELECTOR), 'extended_va': hex(EXTENDED),
                  'simple_va': hex(SIMPLE), 'unicorn_version': unicorn.__version__,
                  'paths': {}, 'cases': {}, 'probe_counts': {}}
        mismatches = []
        select_cases = corpus()
        extended_cases = extended_corpus()
        no_compression_cases = select_cases[:24]
        report['cases']['select'] = compare(sender, args.probe, report, mismatches, 'select',
                                            select_cases, True)
        report['cases']['select_no_compression'] = compare(sender, args.probe, report, mismatches,
                                                           'select', no_compression_cases, False)
        report['cases']['extended'] = compare(sender, args.probe, report, mismatches, 'extended',
                                              extended_cases, True)
        if report['paths'].get('simple', 0) == 0 or report['paths'].get('extended', 0) == 0:
            mismatches.append({'problem': 'corpus did not exercise both wire paths',
                               'paths': report['paths']})
        report['mismatch_count'] = len(mismatches)
        report['mismatches'] = mismatches[:20]
        report['vectors'] = freeze_vectors(sender, args.probe)
        print(json.dumps(report, indent=2))
        return int(bool(mismatches))
    except (OSError, ValueError, RuntimeError, pefile.PEFormatError, unicorn.UcError,
            subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        print('check_network_send: %s' % error, file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
