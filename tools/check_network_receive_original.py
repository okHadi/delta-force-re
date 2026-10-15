#!/usr/bin/env python3
"""Compare the native receive parser with original Netsocknl.dll code; stub all sockets.

Sections
  decode   emulates FUN_100052e0 (0x100052e0) with recvfrom/htons/filter/expander
           stubs and compares every output of tools/network_probe.cpp decode mode.
  filter   emulates FUN_10005280 (0x10005280) against network_probe filter mode.
  wrapper  checks the recvfrom error branches of FUN_100052e0 (would-block vs error).

Packet builders reuse the original encoders FUN_10005020 (simple) and FUN_10005120
(extended header). No sockets are opened; recvfrom/sendto/htons/WSAGetLastError are
stubbed inside Unicorn only. The printed "vectors" block is frozen into
tests/native_network.cpp.
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

SOURCE_HASH = 'fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2'
RECEIVE = 0x100052E0
ENCODE_SIMPLE = 0x10005020
ENCODE_EXTENDED = 0x10005120
FILTER = 0x10005280
RECVFROM = 0x1000595E
WSAGETLASTERROR = 0x10005916
HTONS = 0x1000592E
NTOHL = 0x10005958
SENDTO = 0x10005952
EXPAND_PTR = 0x100096AC
COMPRESS_LOADED = 0x10019860
FILTER_COUNT = 0x100096B8
FILTER_TABLE = 0x100076A8
RECV_POINTER = 0x10019854
RECV_CAPACITY = 0x1001985C
SOCKET = 0x100096B4
PORT_GLOBAL = 0x10019868
STACK, DATA, STOP, STUB = 0x2000000, 0x3000000, 0x4000000, 0x5000000
SRC = DATA
DST = DATA + 0x1000
RECV = DATA + 0x3000
OUT = DATA + 0x5000
DST_SIZE = 8192
RECV_SIZE = 8192
SOCKET_ID = 42
PORT = 12345
EXPAND_CAPACITY = 0x10000


def crc32_mpeg2(data):
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte << 24
        for _ in range(8):
            crc = ((crc << 1) ^ (0x04C11DB7 if crc & 0x80000000 else 0)) & 0xFFFFFFFF
    return crc


class Scanner:
    """One Unicorn machine with all stubs installed."""

    def __init__(self, data):
        pe = pefile.PE(data=data)
        self.machine = Uc(UC_ARCH_X86, UC_MODE_32)
        base = pe.OPTIONAL_HEADER.ImageBase
        size = (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095
        self.machine.mem_map(base, size)
        self.machine.mem_write(base, pe.get_memory_mapped_image())
        pe.close()
        self.machine.mem_map(STACK, 0x1000)
        self.machine.mem_map(DATA, 0x8000)
        self.machine.mem_map(STOP, 0x1000)
        self.machine.mem_map(STUB, 0x1000)
        self.machine.mem_write(RECV_POINTER, struct.pack('<I', RECV))
        self.machine.mem_write(RECV_CAPACITY, struct.pack('<I', RECV_SIZE))
        self.machine.mem_write(SOCKET, struct.pack('<I', SOCKET_ID))
        self.machine.mem_write(PORT_GLOBAL, struct.pack('<H', PORT))
        self.recv = {'mode': 'data', 'packet': b'', 'ip': 0, 'port': 0, 'wsa': 0}
        self.expander_bytes = None
        self.expander_input_length = 0
        self.captured = []
        self.expander_seen = {}
        machine = self.machine
        machine.hook_add(UC_HOOK_CODE, self._htons_stub, begin=HTONS, end=HTONS)
        machine.hook_add(UC_HOOK_CODE, self._ntohl_stub, begin=NTOHL, end=NTOHL)
        machine.hook_add(UC_HOOK_CODE, self._sendto_stub, begin=SENDTO, end=SENDTO)
        machine.hook_add(UC_HOOK_CODE, self._recvfrom_stub, begin=RECVFROM, end=RECVFROM)
        machine.hook_add(UC_HOOK_CODE, self._wsa_stub, begin=WSAGETLASTERROR, end=WSAGETLASTERROR)
        machine.hook_add(UC_HOOK_CODE, self._expand_stub, begin=STUB, end=STUB)

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

    def _ntohl_stub(self, uc, address, size, user):
        sp = uc.reg_read(UC_X86_REG_ESP)
        value = struct.unpack('<I', uc.mem_read(sp + 4, 4))[0]
        self._finish_call(4, ((value >> 24) | ((value >> 8) & 0xFF00) |
                              ((value << 8) & 0xFF0000) | (value << 24)) & 0xFFFFFFFF)

    def _sendto_stub(self, uc, address, size, user):
        sp = uc.reg_read(UC_X86_REG_ESP)
        socket, buffer, length, flags, destination, address_size = struct.unpack(
            '<6I', uc.mem_read(sp + 4, 24))
        if socket != SOCKET_ID or length > RECV_SIZE or flags != 0 or address_size != 16:
            raise RuntimeError('Unexpected original sendto arguments')
        self.captured.append(bytes(uc.mem_read(buffer, length)))
        self._finish_call(24, length)

    def _recvfrom_stub(self, uc, address, size, user):
        sp = uc.reg_read(UC_X86_REG_ESP)
        socket, buffer, length, flags, from_pointer, fromlen_pointer = struct.unpack(
            '<6I', uc.mem_read(sp + 4, 24))
        if socket != SOCKET_ID or length != RECV_SIZE or flags != 0:
            raise RuntimeError('Unexpected original recvfrom arguments')
        if self.recv['mode'] == 'error':
            self._finish_call(24, 0xFFFFFFFF)
            return
        packet = self.recv['packet']
        uc.mem_write(buffer, bytes(RECV_SIZE))
        uc.mem_write(buffer, packet)
        sockaddr = struct.pack('<HH4s8s', 2, self.recv['port'],
                               struct.pack('<I', self.recv['ip']), bytes(8))
        uc.mem_write(from_pointer, sockaddr)
        uc.mem_write(fromlen_pointer, struct.pack('<I', 16))
        self._finish_call(24, len(packet))

    def _wsa_stub(self, uc, address, size, user):
        self._finish_call(0, self.recv['wsa'])

    def _expand_stub(self, uc, address, size, user):
        sp = uc.reg_read(UC_X86_REG_ESP)
        source, target, capacity = struct.unpack('<3I', uc.mem_read(sp + 4, 12))
        if capacity != EXPAND_CAPACITY:
            raise RuntimeError('Unexpected expander capacity')
        uc.mem_write(target, self.expander_bytes)
        self.expander_seen['source'] = source
        self.expander_seen['input'] = bytes(uc.mem_read(source, self.expander_input_length))
        self.expander_seen['calls'] = self.expander_seen.get('calls', 0) + 1
        # cdecl: the original adds 0xc after the call, so only pop the return address.
        self._finish_call(0, len(self.expander_bytes))

    def _reset_registers(self, stack_pointer):
        for register in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                         UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
            self.machine.reg_write(register, 0)
        self.machine.reg_write(UC_X86_REG_ESP, stack_pointer)
        self.machine.reg_write(UC_X86_REG_EFLAGS, 2)

    def _run(self, entry, arguments):
        sp = STACK + 0x800
        self.machine.mem_write(sp, struct.pack('<%dI' % (len(arguments) + 1), STOP, *arguments))
        self._reset_registers(sp)
        self.machine.emu_start(entry, STOP, timeout=8000000, count=4000000)
        if self.machine.reg_read(UC_X86_REG_EIP) != STOP:
            raise RuntimeError('Original function did not return')
        return self.machine.reg_read(UC_X86_REG_EAX)

    def encode(self, entry, payload, ip=0x0100007F, flag=None):
        if not payload:
            return b''
        self.captured.clear()
        self.machine.mem_write(SRC, bytes(0x8000 - 0x6000))
        self.machine.mem_write(SRC, payload)
        arguments = [SRC, len(payload), ip]
        if flag is not None:
            arguments.append(flag)
        result = self._run(entry, arguments)
        if result != 0:
            raise RuntimeError('Original encoder returned %#x' % result)
        header = 4 if entry == ENCODE_SIMPLE else 10
        if len(self.captured) != 1 or len(self.captured[0]) != len(payload) + header:
            raise RuntimeError('Unexpected original datagram length')
        return self.captured[0]

    def _write_filter(self, ranges):
        self.machine.mem_write(FILTER_COUNT, struct.pack('<I', len(ranges)))
        table = b''.join(struct.pack('<II', start, span) for start, span in ranges)
        self.machine.mem_write(FILTER_TABLE, table + bytes(max(0, 0x100 - len(table))))

    def receive(self, packet, ip, port, capacity, ranges, expander):
        self._write_filter(ranges)
        self.machine.mem_write(COMPRESS_LOADED, struct.pack('<I', 1 if expander is not None else 0))
        self.machine.mem_write(EXPAND_PTR, struct.pack('<I', STUB if expander is not None else 0))
        self.expander_bytes = expander
        self.expander_seen.clear()
        if expander is not None and len(packet) >= 4:
            header = packet[9] if (len(packet) >= 10 and packet[:4] == bytes(4)) else 4
            self.expander_input_length = max(0, len(packet) - header)
        else:
            self.expander_input_length = 0
        self.machine.mem_write(DST, bytes(DST_SIZE))
        self.machine.mem_write(RECV, bytes(RECV_SIZE))
        self.machine.mem_write(OUT, bytes(16))
        self.recv.update(mode='data', packet=packet, ip=ip, port=port, wsa=0)
        result = self._run(RECEIVE, [DST, capacity, OUT, OUT + 4, OUT + 8])
        ip_out, port_out = struct.unpack('<IH', self.machine.mem_read(OUT, 6))
        size_out = struct.unpack('<I', self.machine.mem_read(OUT + 8, 4))[0]
        return {
            'status': result,
            'ip': ip_out,
            'port': port_out,
            'size': size_out,
            'dst': bytes(self.machine.mem_read(DST, DST_SIZE)),
            'expander': dict(self.expander_seen),
        }

    def receive_error(self, wsa):
        self.machine.mem_write(OUT, bytes(16))
        self.recv.update(mode='error', wsa=wsa)
        result = self._run(RECEIVE, [DST, DST_SIZE, OUT, OUT + 4, OUT + 8])
        return result, struct.unpack('<IHI', self.machine.mem_read(OUT, 10))

    def filter_hit(self, ip, ranges):
        self._write_filter(ranges)
        return self._run(FILTER, [ip])


def build_retry_packet(seed):
    """Construct one extended packet that only decodes on the second CRC attempt."""
    rng = random.Random(seed)
    while True:
        payload = bytearray(rng.randrange(256) & 0xFE for _ in range(40))
        payload[0:6] = bytes([0, 0, 0, 0, 0, 10])
        crc = crc32_mpeg2(bytes(payload))
        if not (crc >> 5) & 1:
            break
    # First attempt packs the low bits of the 16-byte region into bytes 4..7;
    # the second attempt packs them into bytes 0..3 after re-scrambling.
    packed = (crc & 1) | (((crc >> 1) & 1) << 8) | (((crc >> 2) & 1) << 16) | \
             (((crc >> 3) & 1) << 24)
    wire = bytearray(len(payload) + 4)
    struct.pack_into('<I', wire, 4, packed)
    wire[8] = (crc >> 4) & 1
    wire[9] = 10
    for index in range(32):
        wire[10 + index] = (payload[6 + index] & 0xFE) | ((crc >> (6 + index)) & 1)
    for index in range(38, len(payload)):
        wire[4 + index] = payload[index]
    return bytes(wire), bytes(payload)


def simple_payloads():
    payloads = []
    for length in (1, 2, 3, 4, 7, 15, 31, 32, 33, 40, 48, 63, 64, 255, 512):
        payloads.append(bytes(length))
        payloads.append(b'\xff' * length)
        payloads.append(bytes(index & 255 for index in range(length)))
    rng = random.Random(0xDF1A03)
    for _ in range(20):
        payloads.append(bytes(rng.randrange(256) for _ in range(rng.randrange(1, 260))))
    return payloads


def extended_payloads():
    payloads = []
    for length in (1, 7, 15, 31, 32, 33, 40, 64, 255, 700):
        payloads.append(bytes(length))
        payloads.append(b'\xff' * length)
        payloads.append(bytes(index & 255 for index in range(length)))
    rng = random.Random(0xDF1A04)
    for _ in range(12):
        payloads.append(bytes(rng.randrange(256) for _ in range(rng.randrange(1, 400))))
    return payloads


def byte_exact(packet):
    """True when the original never reads past the datagram for this input."""
    return exclusion_reason(packet) is None


def exclusion_reason(packet):
    size = len(packet)
    if size < 4:
        return 'short-header'
    first = struct.unpack('<I', packet[:4])[0]
    if first == 0 and size < 10:
        return 'short-extended-header'
    header = 4 if first != 0 else packet[9]
    if size <= header:
        return 'empty-or-overrun-payload'
    payload_length = size - header
    if header > 4 and payload_length < 32:
        expected = struct.unpack('<I', packet[4:8])[0]
        if crc32_mpeg2(packet[header:]) != expected:
            return 'retry-needs-stale-bytes'
    return None


def build_decode_groups(scanner):
    groups = []

    def case(packet, ip=0x0100007F, port=0x1234, capacity=DST_SIZE):
        return {'packet': packet, 'ip': ip, 'port': port, 'capacity': capacity}

    plain = []
    simple = {}
    for payload in simple_payloads():
        packet = scanner.encode(ENCODE_SIMPLE, payload)
        simple[packet] = payload
        plain.append(case(packet, ip=0x0100007F + (len(payload) << 8), port=0x1234 + len(payload)))
    extended = {}
    for payload in extended_payloads():
        packet = scanner.encode(ENCODE_EXTENDED, payload, flag=0)
        extended[packet] = payload
        plain.append(case(packet, ip=0x0200007F + (len(payload) << 16), port=0x2345 + len(payload)))
    rng = random.Random(0xDF1A05)
    for _ in range(150):
        size = rng.randrange(0, 320)
        packet = bytearray(rng.randrange(256) for _ in range(size))
        style = rng.randrange(4)
        if style == 0 and size >= 10:
            packet[0:4] = bytes(4)
            choices = [0, 4, 5, 8, 10, 11, 20, 255, size, size - 4]
            if size >= 32:
                choices.append(size - 32)
            packet[9] = rng.choice([value & 0xFF for value in choices])
        elif style == 1 and size >= 4:
            packet[0:4] = bytes(4)
        plain.append(case(bytes(packet)))
    for size in range(0, 12):
        for fill in (0x00, 0x7F):
            packet = bytearray([fill]) * size
            if size >= 4 and fill == 0:
                packet[size - 1] = 10
            plain.append(case(bytes(packet)))
    corrupt_source = [packet for packet in list(simple) + list(extended)]
    for index in range(12):
        packet = bytearray(corrupt_source[index * 7 % len(corrupt_source)])
        at = rng.randrange(len(packet))
        packet[at] ^= rng.choice([1, 0x80, 0xFF])
        plain.append(case(bytes(packet)))
    boundaries = list(simple)[:4] + list(extended)[:4]
    for packet in boundaries:
        plain.append(case(packet, capacity=len(packet)))
        plain.append(case(packet, capacity=max(1, len(packet) - 5)))
        plain.append(case(packet, capacity=0))
    retry_vectors = []
    for seed in (0xDF1A06, 0xDF1A07):
        wire, payload = build_retry_packet(seed)
        retry_vectors.append((wire, payload))
        plain.append(case(wire))
        plain.append(case(wire, capacity=len(wire) - 4))
    groups.append({'name': 'plain', 'ranges': [], 'expander': None, 'cases': plain,
                   'retry_vectors': retry_vectors})

    blocked_packet = list(simple)[3]
    for name, ranges in (
            ('filter-one', [(0x0A000000, 0x00FFFFFF)]),
            ('filter-two', [(0x0A000000, 0x00FFFFFF), (0x7F000000, 0x0000FFFF)]),
            ('filter-unsorted', [(0x0A000000, 0x00FFFFFF), (0x01000000, 0x00FFFFFF)])):
        cases = []
        for ip_value in (0x0302010A, 0x0100007F, 0x00000000, 0xFFFFFFFF, 0x0000000A,
                         0xFFFFFF0A, 0x05040302, 0x0A000000):
            cases.append(case(blocked_packet if ip_value & 1 else list(extended)[5],
                              ip=ip_value, port=0x3456))
        groups.append({'name': name, 'ranges': ranges, 'expander': None, 'cases': cases})

    expand_bytes = bytes.fromhex('abcd00ff1234')
    expand_cases = []
    expanded = {}
    for payload in extended_payloads()[:30]:
        packet = scanner.encode(ENCODE_EXTENDED, payload, flag=1)
        expanded[packet] = payload
        expand_cases.append(case(packet, port=0x4567))
    for payload in simple_payloads()[:6]:
        expand_cases.append(case(scanner.encode(ENCODE_SIMPLE, payload)))
    compressed = list(expanded)[0]
    damaged = bytearray(compressed)
    damaged[-1] ^= 0xFF
    expand_cases.append(case(bytes(damaged)))
    groups.append({'name': 'expand', 'ranges': [], 'expander': expand_bytes,
                   'cases': expand_cases, 'expanded': expanded})
    return groups


def build_filter_cases():
    tables = [
        [],
        [(0x0A000000, 0x00FFFFFF)],
        [(0x0A000000, 0x00000000)],
        [(0x0A000000, 0x00FFFFFF), (0x7F000000, 0x0000FFFF)],
        [(0x0A000000, 0x00FFFFFF), (0x01000000, 0x00FFFFFF)],
        [(0xFFFFFFF0, 0x00000020)],
    ]
    rng = random.Random(0xDF1A08)
    wide = sorted((rng.randrange(0x00000000, 0xFFFFFF00), rng.randrange(0, 0x40000000))
                  for _ in range(20))
    tables.append(list(wide))
    cases = []
    for table in tables:
        probes = [0x00000000, 0xFFFFFFFF, 0x7F000001, 0x0A000000, 0x0AFFFFFF,
                  0x0B000000, 0x7FFFFFFF, 0x80000000]
        for start, span in table:
            probes.extend([start, start + span, max(0, start - 1), start + span + 1])
        for _ in range(16):
            probes.append(rng.randrange(0, 0x100000000))
        for host in dict.fromkeys(value & 0xFFFFFFFF for value in probes):
            cases.append({'table': table, 'ip': ((host >> 24) | ((host >> 8) & 0xFF00) |
                                                 ((host << 8) & 0xFF0000) | (host << 24)) & 0xFFFFFFFF})
    return cases


def probe_once(probe, args, lines):
    result = subprocess.run([str(probe.resolve())] + args,
                            input=''.join(line + '\n' for line in lines),
                            capture_output=True, text=True, check=False, timeout=300)
    if result.returncode != 0:
        raise RuntimeError('probe failed: %s' % result.stderr.strip())
    return [json.loads(line) for line in result.stdout.splitlines()]


def compare_decode(probe, scanner, group, mismatches, report):
    for case in group['cases']:
        case['expected'] = scanner.receive(case['packet'], case['ip'], case['port'],
                                           case['capacity'], group['ranges'], group['expander'])
        case['exact'] = byte_exact(case['packet'])
        if not case['exact']:
            report['byte_excluded'] += 1
            reason = exclusion_reason(case['packet'])
            report['byte_excluded_reasons'][reason] = report['byte_excluded_reasons'].get(reason, 0) + 1
    lines = ['%08x %04x %x %s' % (case['ip'], case['port'], case['capacity'],
                                  case['packet'].hex()) for case in group['cases']]
    args = ['decode']
    for start, span in group['ranges']:
        args += ['--block-range', '%08x:%08x' % (start, span)]
    if group['expander'] is not None:
        args += ['--expander', group['expander'].hex()]
    actual = probe_once(probe, args, lines)
    if len(actual) != len(group['cases']):
        raise RuntimeError('probe returned an unexpected number of results')
    for index, (case, got) in enumerate(zip(group['cases'], actual)):
        expected = case['expected']
        got_ip = int(got['ip'], 16)
        got_raw_port = int(got['port'], 16)
        got_host_port = int(got['host_port'], 16)
        names = []
        if got['status'] != expected['status']:
            names.append('status')
        if got_ip != expected['ip']:
            names.append('ip')
        expected_raw_port = case['port'] & 0xFFFF if expected['status'] == 0 else 0
        if got_raw_port != expected_raw_port:
            names.append('port')
        if got_host_port != expected['port']:
            names.append('host_port')
        if got['size'] != expected['size']:
            names.append('size')
        payload = bytes.fromhex(got['payload'])
        if case['exact'] and expected['dst'] != payload + bytes(DST_SIZE - len(payload)):
            names.append('dst')
        if expected['expander'].get('calls'):
            if expected['expander'].get('input') != case['expander_input']:
                names.append('expander_input')
        if names:
            mismatches.append({'group': group['name'], 'case': index, 'fields': names,
                               'packet': case['packet'].hex(),
                               'expected': {key: expected[key] for key in ('status', 'ip', 'port', 'size')},
                               'got': {key: got[key] for key in ('status', 'ip', 'port', 'host_port', 'size')}})


def compare_filter(probe, scanner, mismatches):
    cases = build_filter_cases()
    total = 0
    by_table = {}
    for case in cases:
        key = json.dumps(case['table'])
        by_table.setdefault(key, {'table': case['table'], 'ips': []})
        by_table[key]['ips'].append(case['ip'])
    for entry in by_table.values():
        args = ['filter']
        for start, span in entry['table']:
            args += ['--block-range', '%08x:%08x' % (start, span)]
        got = probe_once(probe, args, ['%08x' % ip for ip in entry['ips']])
        for ip, line in zip(entry['ips'], got):
            total += 1
            expected = scanner.filter_hit(ip, entry['table'])
            if bool(line['blocked']) != bool(expected):
                mismatches.append({'table': entry['table'], 'ip': '%08x' % ip,
                                   'expected': expected, 'got': line['blocked']})
    return total


def wrapper_cases(scanner):
    results = []
    for wsa, name in ((0x2733, 'would-block'), (0x2736, 'hard-error')):
        status, values = scanner.receive_error(wsa)
        results.append({'case': name, 'wsa': wsa, 'status': status, 'outputs': list(values)})
    return results


def freeze_vectors(scanner, probe):
    rng = random.Random(0xDF1A09)
    simple_payload = bytes(rng.randrange(256) for _ in range(40))
    extended_payload = bytes(rng.randrange(256) for _ in range(40))
    expander = bytes.fromhex('abcd00ff1234')
    cases = []

    def add(name, packet, capacity=DST_SIZE, use_expander=None, note=''):
        cases.append({'name': name, 'packet': packet, 'capacity': capacity,
                      'expander': use_expander, 'note': note})

    add('simple-40', scanner.encode(ENCODE_SIMPLE, simple_payload))
    add('simple-40-small-capacity', scanner.encode(ENCODE_SIMPLE, simple_payload), capacity=10)
    add('extended-40', scanner.encode(ENCODE_EXTENDED, extended_payload, flag=0))
    short_payload = bytes(rng.randrange(256) for _ in range(15))
    add('extended-15', scanner.encode(ENCODE_EXTENDED, short_payload, flag=0))
    edge = bytes(rng.randrange(256) for _ in range(32))
    add('extended-32', scanner.encode(ENCODE_EXTENDED, edge, flag=0))
    add('extended-31', scanner.encode(ENCODE_EXTENDED, edge[:31], flag=0))
    retry_wire, retry_payload = build_retry_packet(0xDF1A06)
    add('retry-40', retry_wire)
    compressed = scanner.encode(ENCODE_EXTENDED, simple_payload, flag=1)
    add('compressed-40', compressed, use_expander=expander)
    add('compressed-40-no-expander', compressed, note='compression not loaded -> 0x6a')
    corrupt = bytearray(scanner.encode(ENCODE_SIMPLE, simple_payload))
    corrupt[-1] ^= 0xFF
    add('corrupt-40', bytes(corrupt))
    add('malformed-extended-9', bytes(9), note='fail-closed port extension')
    lines_by_config = {}
    for config in (None, expander):
        subset = [case for case in cases
                  if (case['expander'] == config)] if config else \
                 [case for case in cases if case['expander'] is None]
        if not subset:
            continue
        args = ['decode'] + (['--expander', config.hex()] if config else [])
        lines = ['%08x %04x %x %s' % (0x0100007F, 0x1234, case['capacity'], case['packet'].hex())
                 for case in subset]
        got = probe_once(probe, args, lines)
        for case, line in zip(subset, got):
            lines_by_config[case['name']] = line
    vectors = []
    for case in cases:
        line = lines_by_config[case['name']]
        expected = scanner.receive(case['packet'], 0x0100007F, 0x1234, case['capacity'], [],
                                   case['expander'])
        payload = bytes.fromhex(line['payload'])
        expected_raw_port = 0x1234 if expected['status'] == 0 else 0
        if (line['status'], int(line['ip'], 16), int(line['port'], 16),
                int(line['host_port'], 16), line['size']) != \
                (expected['status'], expected['ip'], expected_raw_port,
                 expected['port'], expected['size']):
            raise RuntimeError('freeze vector %s disagrees with the probe' % case['name'])
        if expected['dst'][:len(payload)] != payload:
            raise RuntimeError('freeze vector %s payload disagrees with the probe' % case['name'])
        vectors.append({
            'name': case['name'],
            'packet': case['packet'].hex(),
            'capacity': case['capacity'],
            'expander': case['expander'].hex() if case['expander'] else None,
            'status': expected['status'],
            'source_port_host': expected['port'],
            'size': expected['size'],
            'written': len(payload),
            'payload': expected['dst'][:len(payload)].hex(),
            'note': case['note'],
        })
    if len(retry_payload) != 40 or len(vectors) != len(cases):
        raise RuntimeError('unexpected vector generator state')
    return vectors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('network_dll', type=Path)
    parser.add_argument('probe', type=Path)
    args = parser.parse_args()
    try:
        data = args.network_dll.read_bytes()
        if hashlib.sha256(data).hexdigest() != SOURCE_HASH:
            raise ValueError('Expected the documented Netsocknl.dll SHA-256')
        scanner = Scanner(data)
        report = {'source_sha256': SOURCE_HASH, 'procedure_va': hex(RECEIVE),
                  'builder_vas': {'simple': hex(ENCODE_SIMPLE), 'extended': hex(ENCODE_EXTENDED),
                                  'filter': hex(FILTER)},
                  'unicorn_version': unicorn.__version__, 'byte_excluded': 0,
                  'byte_excluded_reasons': {}}
        mismatches = []
        decode_cases = 0
        groups = build_decode_groups(scanner)
        expanded_by_packet = next((group.get('expanded', {}) for group in groups
                                   if group['name'] == 'expand'), {})
        for group in groups:
            for case in group['cases']:
                case['expander_input'] = expanded_by_packet.get(case['packet'], b'')
            if group.get('retry_vectors'):
                for wire, payload in group['retry_vectors']:
                    result = scanner.receive(wire, 0x0100007F, 0x1234, DST_SIZE, [], None)
                    if result['status'] != 0 or result['dst'][:len(payload)] != payload:
                        raise RuntimeError('Retry vector did not decode as constructed')
            compare_decode(args.probe, scanner, group, mismatches, report)
            decode_cases += len(group['cases'])
        filter_cases = compare_filter(args.probe, scanner, mismatches)
        report.update({'decode_cases': decode_cases, 'filter_cases': filter_cases,
                       'mismatch_count': len(mismatches), 'mismatches': mismatches[:20],
                       'wrapper_cases': wrapper_cases(scanner),
                       'vectors': freeze_vectors(scanner, args.probe),
                       'scope': 'Datagram parsing; recvfrom/sendto/htons stubs; no network traffic'})
        print(json.dumps(report, indent=2))
        return int(bool(mismatches))
    except (OSError, ValueError, RuntimeError, pefile.PEFormatError, unicorn.UcError,
            subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        print('check_network_receive: %s' % error, file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
