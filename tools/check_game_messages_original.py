#!/usr/bin/env python3
"""Strict original-x86 replay for the game message bus port.

Emulates original Df.exe functions under Unicorn with stubbed callees and
compares them with tools/game_messages_probe output:

  0x460DE0 descriptor sort        0x46F3E0 enqueue
  0x46F4B0 frame worker           0x4610E0 serializer
  0x4611C0 flush                  0x461050 receive getter
  0x46D9F0 dispatch               0x470F40/50/60/B0 producers

Checks require equal output counts, completed emulation (return to the stop
sentinel) and zero mismatches; any mismatch prints a report and exits nonzero.

Usage: check_game_messages_original.py GAME_FOLDER GAME_MESSAGES_PROBE [--report FILE]
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
from unicorn import UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_WRITE, Uc
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP

SOURCE_HASH = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
IMAGE, SIZE = 0x400000, 0x1000000
STACK, STOP = 0x2000000, 0x6000000
FRAME_SRC = 0x10000000
VTABLE = 0x11000000
SEND_STUB = 0x410000
RECV_STUB = 0x410100

F = {'sort': 0x460DE0, 'enqueue': 0x46F3E0, 'worker': 0x46F4B0, 'serialize': 0x4610E0,
     'flush': 0x4611C0, 'receive': 0x461050, 'dispatch': 0x46D9F0, 'update': 0x46F5C0,
     'shot': 0x470F40, 'detonate': 0x470F50, 'placement': 0x470F60, 'trace': 0x470FB0}

QUEUE, RECORD, SLOTS = 0xBC5D24, 0x264, 0x40
RING = 0xBCFC3C
ARRAYS = {'players_7cca80': (0x7CCA80, 0x11400), 'actors_7dde80': (0x7DDE80, 0x11400),
          'squad_867e80': (0x867E80, 0x11400), 'effects_8ace80': (0x8ACE80, 0x11400)}
ARRAY_BASE = {name: base for name, (base, _) in ARRAYS.items()}
ACTOR_TABLE_POINTER = 0xC23810
ACTOR_TABLE = 0x1500000
NAME_POINTER = 0x7F000000
WEAPON_TABLE = 0x1600000
CLASS_TABLE = 0x1610000
FORMAT_POINTER = 0x7F200000
INDIRECT_TARGET = 0x490000

HOST_STUBS = {0x4828C0, 0x44A0C0, 0x42CCE0, 0x47DF40, 0x477820, 0x46C180}
UI_STUBS = {0x404CF0, 0x479F00, 0x493A80, 0x4564B0}
STUBS = {
    0x4828C0: 1, 0x44A0C0: 1, 0x42CCE0: 1, 0x47DF40: 2, 0x477820: 3, 0x46C180: 0,
    0x45DA30: 1, 0x45D6F0: 1, 0x462F20: 3, 0x486AF0: 1, 0x484ED0: 0, 0x470D50: 1,
    0x471010: 1, 0x470400: 1, 0x43F5A0: 2, 0x4606B0: 3, 0x4607E0: 2, 0x460950: 3,
    0x45DC70: 4, 0x43E7D0: 4, 0x43E720: 1, 0x479F00: 2, 0x479FA0: 4, 0x471570: 3,
    0x4715B0: 1, 0x471600: 0, 0x471760: 1, 0x471810: 1, 0x471870: 1, 0x4560C0: 2,
    0x4848C0: 0, 0x48DD60: 0, 0x42CEF0: 3, 0x455AF0: 1, 0x455B30: 1, 0x455B10: 1,
    0x404CF0: 3, 0x493A80: 4, 0x4564B0: 1, 0x482B00: 0, 0x490880: 0, 0x490000: 2,
}
NUMERIC_ARGS = {0x43E720: 1, 0x43E7D0: 4, 0x479FA0: 4, 0x455AF0: 1, 0x455B30: 1,
                0x455B10: 1, 0x45D6F0: 1, 0x471570: 3, 0x45DC70: 4, 0x490000: 2}


def load_image(raw):
    pe = pefile.PE(data=raw)
    image = bytearray(pe.OPTIONAL_HEADER.SizeOfImage)
    for section in pe.sections:
        if section.PointerToRawData and not section.Characteristics & 0x80:
            chunk = raw[section.PointerToRawData:section.PointerToRawData + section.SizeOfRawData]
            image[section.VirtualAddress:section.VirtualAddress + len(chunk)] = chunk
    pe.close()
    # 0x4014E9 runs 0x460DE0 at startup: sort the packed 0x4F8D48 image into id
    # order (idempotent on an already sorted table). Every original emulation
    # then sees the runtime table the game uses.
    base = 0x4F8D48 - IMAGE  # image is RVA indexed
    window = [bytes(image[base + i * 4:base + i * 4 + 4]) for i in range(0x100)]
    for ident in range(0xFF):
        found = next((slot for slot in range(0xFF) if window[slot][0] == ident), None)
        if found is not None:
            window[found], window[ident] = window[ident], window[found]
    for index, entry in enumerate(window):
        image[base + index * 4:base + index * 4 + 4] = entry
    return image


class Harness:
    def __init__(self, raw):
        self.image = load_image(raw)
        self.calls = []
        self.sent = []
        self.received = []
        self.names = 0
        self.formats = 0

    def machine(self):
        u = Uc(UC_ARCH_X86, UC_MODE_32)
        u.mem_map(IMAGE, SIZE)
        u.mem_write(IMAGE, bytes(self.image))
        u.mem_map(STACK, 0x100000)
        u.mem_map(STOP, 0x1000)
        u.mem_map(FRAME_SRC, 0x10000)
        u.mem_map(VTABLE, 0x1000)
        u.mem_map(ACTOR_TABLE, 0x50000)
        u.mem_map(NAME_POINTER, 0x10000)
        u.mem_map(FORMAT_POINTER, 0x1000)
        u.mem_map(WEAPON_TABLE, 0x4000)
        u.mem_map(CLASS_TABLE, 0x1000)
        return u

    def install(self, u):
        def read_dword(at):
            return struct.unpack('<I', u.mem_read(at, 4))[0]

        def ret(value=0):
            sp = u.reg_read(UC_X86_REG_ESP)
            u.reg_write(UC_X86_REG_EAX, value)
            u.reg_write(UC_X86_REG_EIP, read_dword(sp))
            u.reg_write(UC_X86_REG_ESP, sp + 4)

        def hook(machine, at, size, user):
            sp = u.reg_read(UC_X86_REG_ESP)
            if at == SEND_STUB:
                buf, length = read_dword(sp + 4), read_dword(sp + 8)
                if length:
                    self.sent.append(bytes(u.mem_read(buf, length)))
                ret(0)
                return
            if at == RECV_STUB:
                value = self.received.pop(0) if self.received else b''
                if value:
                    u.mem_write(FRAME_SRC + 0x8000, value)
                    ret(FRAME_SRC + 0x8000)
                else:
                    ret(0)
                return
            if at == 0x477820:
                angle = struct.unpack('<i', u.mem_read(sp + 4, 4))[0]
                sin_at, cos_at = read_dword(sp + 8), read_dword(sp + 12)
                u.mem_write(sin_at, struct.pack('<i', (angle >> 8) * 2))
                u.mem_write(cos_at, struct.pack('<i', (angle >> 8) * 3))
                ret(0)
                return
            if at == 0x4828C0:
                ret((read_dword(sp + 4) * 7 + 1) & 0x3FF)
                return
            if at == 0x42CCE0:
                ret((read_dword(sp + 4) ^ 0x1234) & 0xFFFF)
                return
            if at == 0x47DF40:
                x, y = read_dword(sp + 4), read_dword(sp + 8)
                ret(((x >> 8) * 7 + (y >> 8) * 13) & 0xFFFFFF)
                return
            if at == 0x46C180:
                ret(0x00112233)
                return
            if at == 0x44A0C0:
                self.calls.append((at, [read_dword(sp + 4)]))
                ret(NAME_POINTER + self.names * 0x100)
                self.names += 1
                return
            if at == 0x404CF0:
                self.calls.append((at, [read_dword(sp + 4 + 4 * i) for i in range(3)]))
                ret(FORMAT_POINTER + self.formats * 0x40)
                self.formats += 1
                return
            if at in STUBS:
                nargs = STUBS[at]
                args = [read_dword(sp + 4 + 4 * i) for i in range(nargs)]
                if at == 0x462F20:
                    pos = args[0]
                    args = [struct.unpack('<i', u.mem_read(pos + 4 * i, 4))[0] for i in range(5)]
                    args.append(read_dword(sp + 4 + 4 * 2))
                self.calls.append((at, args))
                ret(0)
                return

        u.hook_add(UC_HOOK_CODE, hook)
        return u

    def call(self, u, entry, args=()):
        sp = STACK + 0x80000
        payload = struct.pack('<I', STOP) + b''.join(struct.pack('<I', a & 0xFFFFFFFF) for a in args)
        u.mem_write(sp, payload)
        u.reg_write(UC_X86_REG_ESP, sp)
        u.emu_start(entry, STOP, count=3000000)
        if u.reg_read(UC_X86_REG_EIP) != STOP:
            raise ValueError(f'original function 0x{entry:x} did not return')

    def seed_records(self, u, occupied=(), snapshot=None):
        for name, (base, size) in ARRAYS.items():
            occupied_here = name in occupied
            blob = bytearray()
            for index in range(size // 0x114):
                record = bytearray(0x114)
                if occupied_here:
                    struct.pack_into('<I', record, 0, 0x04030201 + index)
                    struct.pack_into('<I', record, 0x110, INDIRECT_TARGET)
                blob += record
            u.mem_write(base, bytes(blob))
        table = bytearray()
        for handle in range(0x400):
            record = bytearray(0x13C)
            for offset in (0x20, 0x24, 0x28, 0x6C, 0x74, 0x84, 0x90):
                struct.pack_into('<I', record, offset,
                                 0x5A000000 | ((handle & 0xFFF) << 12) | ((offset >> 2) & 0xFFF))
            struct.pack_into('<H', record, 0x94, (handle * 3 + 0x94) & 0xFFFF)
            table += record
        u.mem_write(ACTOR_TABLE, bytes(table))
        u.mem_write(ACTOR_TABLE_POINTER, struct.pack('<I', ACTOR_TABLE))
        weapons = bytearray(39 * 0xA8)
        for index in range(39):
            for offset in range(0xA8):
                weapons[index * 0xA8 + offset] = (index * 0x21 + offset) & 0xFF
        u.mem_write(WEAPON_TABLE, bytes(weapons))
        classes = bytearray(28 * 0x34)
        for index in range(28):
            for offset in range(0x34):
                classes[index * 0x34 + offset] = (index * 0x31 + offset * 3) & 0xFF
        u.mem_write(CLASS_TABLE, bytes(classes))
        u.mem_write(0xAD3ED0, struct.pack('<I', WEAPON_TABLE))
        u.mem_write(0xAD3EE4, struct.pack('<I', CLASS_TABLE))
        slots = bytearray(0x28)
        for slot in range(10):
            struct.pack_into('<H', slots, slot * 4, (slot * 0x111) & 0xFFFF)
        u.mem_write(0xC239C0, bytes(slots))
        u.mem_write(0xC2382C, struct.pack('<I', 0x55667788))
        if snapshot is not None:
            snapshot.update({name: bytes(u.mem_read(base, size)) for name, (base, size) in ARRAYS.items()})


def run_probe(probe, lines):
    out = subprocess.run([probe], input=('\n'.join(lines) + '\n').encode(),
                         stdout=subprocess.PIPE, check=True)
    return [json.loads(line) for line in out.stdout.decode().splitlines()]


def apply_record_writes(before, actions):
    image = {name: bytearray(data) for name, data in before.items()}
    for action in actions:
        if action['array'] == 'none':
            continue
        for offset, width, value in action['writes']:
            at = action['index'] * 0x114 + offset
            for i in range(width):
                image[action['array']][at + i] = (value >> (8 * i)) & 0xFF
    return {name: bytes(data) for name, data in image.items()}


def queue_bytes(records):
    out = bytearray(RECORD * SLOTS)
    for slot, record_hex in records:
        out[slot * RECORD:(slot + 1) * RECORD] = bytes.fromhex(record_hex)
    return bytes(out)


def first_diff(a, b):
    return next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), -1)


# -------------------------------------------------------------- fixtures ----
def pattern(size, seed=0):
    return bytes(((i * 17 + seed + 3) & 0xFF) for i in range(size))


def dispatch_fixture(msg_id):
    if msg_id == 0x01:
        return struct.pack('<I', 1) + bytes(range(0x80))
    if msg_id == 0x03:
        return pattern(0x10C)
    if msg_id in (0x07, 0x16):
        payload = bytearray(pattern(0x2D))
        payload[0:2] = struct.pack('<H', 2)
        payload[2:4] = struct.pack('<H', 0x0102)
        payload[8:8 + 6] = b'NAME\x00\x00'
        return bytes(payload)
    if msg_id in (0x08, 0x2B):
        payload = bytearray(pattern(0x2B))
        payload[0:2] = struct.pack('<H', 4)
        payload[2:4] = struct.pack('<H', 0x0203)
        payload[4:4 + 5] = b'ACT\x00\x00'
        payload[0x28:0x2A] = struct.pack('<H', 3)
        return bytes(payload)
    if msg_id == 0x09:
        payload = bytearray(pattern(0x0F))
        payload[2:4] = struct.pack('<H', 0x0405)
        payload[8:10] = struct.pack('<H', 3)
        return bytes(payload)
    if msg_id == 0x0C:
        payload = bytearray(pattern(0x13 * 2 + 4))
        payload[0:4] = struct.pack('<HH', 6, 2)
        return bytes(payload)
    if msg_id == 0x0E:
        payload = bytearray(pattern(0x12))
        struct.pack_into('<H', payload, 2, 3)
        return bytes(payload)
    if msg_id == 0x0F:
        payload = bytearray(pattern(0x0E))
        payload[1] = 3
        return bytes(payload)
    if msg_id == 0x10:
        return struct.pack('<h', 3) + b'\x00\x00'
    if msg_id == 0x11:
        payload = bytearray(pattern(4))
        struct.pack_into('<h', payload, 0, 3)
        return bytes(payload)
    if msg_id == 0x12:
        body = bytes([1]) + b'TEXT\x00'
        return struct.pack('<H', 2 + len(body)) + body
    if msg_id == 0x13:
        payload = bytearray(pattern(0x10))
        struct.pack_into('<h', payload, 0, 3)
        return bytes(payload)
    if msg_id == 0x14:
        return struct.pack('<H', 7)
    if msg_id == 0x15:
        payload = bytearray(pattern(0x8F))
        payload[0x8D] = 2
        payload[0x8E] = 2
        payload[0x0D] = 1
        payload[0x0E] = 2
        return bytes(payload)
    if msg_id == 0x18:
        payload = bytearray(pattern(0x2D))
        struct.pack_into('<h', payload, 0, 4)
        payload[4] = 1
        payload[0x0C:0x0C + 6] = b'NM2\x00\x00\x00'
        return bytes(payload)
    if msg_id in (0x1A, 0x1B):
        return struct.pack('<I', 9)
    if msg_id == 0x1C:
        payload = bytearray(pattern(7))
        payload[4] = 0x11
        payload[5] = 2
        return bytes(payload)
    if msg_id == 0x26:
        payload = bytearray(pattern(0x17 * 2 + 4))
        payload[0:4] = struct.pack('<HH', 8, 2)
        return bytes(payload)
    if msg_id == 0x27:
        payload = bytearray(pattern(0x12))
        struct.pack_into('<h', payload, 0, 3)
        payload[0x10] = 1
        return bytes(payload)
    if msg_id == 0x29:
        payload = bytearray(pattern(0x10))
        struct.pack_into('<h', payload, 0, 3)
        return bytes(payload)
    if msg_id in (0x34, 0x35, 0x36, 0x37, 0x38):
        size = {0x34: 0x18, 0x35: 0x1C, 0x36: 0x1C, 0x37: 0x18, 0x38: 0x14}[msg_id]
        payload = bytearray(pattern(size))
        struct.pack_into('<h', payload, 0, 3)
        if msg_id == 0x36:
            payload[0x0E] = 0x40
        return bytes(payload)
    if msg_id == 0x39:
        payload = bytearray(pattern(4))
        struct.pack_into('<I', payload, 0, 0x00050004)
        return bytes(payload)
    if msg_id == 0x3A:
        return pattern(0x10)
    if msg_id == 0x3B:
        payload = bytearray(pattern(0x12))
        struct.pack_into('<I', payload, 0x0A, 3 << 16)
        return bytes(payload)
    if msg_id == 0x3C:
        return pattern(0x0A)
    if msg_id == 0x3D:
        return pattern(0x100)
    if msg_id == 0x42:
        first = b'SESSION\x00' + pattern(0x20 - 8)
        second = b'C01M01.BMS\x00'
        return first + second + bytes(0x40 - len(first) - len(second))
    if msg_id == 0x4B:
        return bytes([7])
    if msg_id == 0x4D:
        return bytes([5])
    if msg_id == 0x40:
        return struct.pack('<H', 0x0A) + bytes(range(8))
    sizes = {0x2D: 0, 0x2E: 0, 0x30: 1, 0x33: 0, 0x24: 2, 0x25: 2, 0x3F: 0x0A,
             0x43: 3, 0x46: 8, 0x48: 0x0F, 0x23: 0x14, 0x0D: 0, 0x19: 0, 0x0A: 0x10,
             0x1D: 0, 0x1E: 0xCE, 0x20: 3, 0x41: 2, 0x44: 0, 0x47: 0, 0x4A: 0,
             0x4F: 0x10, 0x50: 8, 0x0B: 0xE2, 0x3E: 3, 0x45: 1, 0x4C: 9, 0x4E: 3,
             0x22: 0x28, 0x2F: 4, 0x28: 0, 0x2C: 0, 0x32: 0, 0x2A: 0x0F, 0x02: 0, 0x04: 0}
    return pattern(sizes[msg_id])


def build_frame(descriptors, msg_id, sequence, payload):
    flags = descriptors[msg_id][0]
    frame = bytes([msg_id])
    if flags & 0x06:
        frame += struct.pack('<H', sequence)
    return frame + payload


DISPATCHABLE = [0x01, 0x03, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
                0x1E, 0x20, 0x23, 0x24, 0x25, 0x26, 0x27, 0x29, 0x2B, 0x2D, 0x2E, 0x30,
                0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,
                0x3F, 0x41, 0x42, 0x43, 0x44, 0x46, 0x47, 0x48, 0x4A, 0x4B, 0x4D,
                0x22, 0x2A, 0x2F, 0x40, 0x45, 0x4C, 0x4E]
# Zero length messages with no case: the original stream would not advance.
# The port fails closed; checked separately.
UNDISPATCHABLE = [0x02, 0x04, 0x28, 0x2C, 0x32]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    raw = (args.game / 'Df.exe').read_bytes()
    if hashlib.sha256(raw).hexdigest() != SOURCE_HASH:
        raise SystemExit('Unsupported original executable hash')
    probe = str(args.probe.resolve())
    mismatches = []
    counts = {'table': 0, 'enqueue': 0, 'worker': 0, 'serialize': 0, 'producer': 0,
              'dispatch': 0}

    def note(case, **fields):
        fields['case'] = case
        mismatches.append(fields)

    table_native = run_probe(probe, ['table'])[0]['descriptors']

    # ---- table (0x460DE0) ----
    h = Harness(raw)
    u = h.machine()
    h.call(u, F['sort'])
    sorted_memory = bytes(u.mem_read(0x4F8D48, 0x200))
    probe_bytes = bytearray(0x200)
    for index, (flags, length) in enumerate(table_native):
        struct.pack_into('<BBH', probe_bytes, index * 4, index, flags, length)
    for index in range(0x51):
        counts['table'] += 1
        # byte 0 is the id echo; holes keep zero, sorted real entries echo the id
        original_entry = sorted_memory[index * 4 + 1:index * 4 + 4]
        native_entry = probe_bytes[index * 4 + 1:index * 4 + 4]
        if original_entry != native_entry:
            note('table', id=index, original=original_entry.hex(), native=native_entry.hex())

    # ---- enqueue (0x46F3E0) ----
    fixtures = []
    for msg_id in list(range(0x00, 0x52)) + [0x1F, 0x31, 0x60]:
        length = 4 if msg_id == 0 else 0x30
        fixtures.append((msg_id, bytes(((i * 31 + msg_id) & 0xFF) for i in range(length))))
    fixtures.append((0x09, b''))
    fixtures.append((0x2A, bytes(range(0x0F))))
    counters = [0, 1, 31999, 32000, 32001, 65535, 70000]
    plans = []
    for index, (msg_id, payload) in enumerate(fixtures):
        counter = counters[index % len(counters)]
        plans.append((msg_id, payload, counter))
    native_enqueues = []
    for msg_id, payload, counter in plans:
        lines = ['reset', f'setcounter {counter}', f'enqueue {msg_id} {payload.hex()}']
        native_enqueues.append(run_probe(probe, lines)[-1])
    for (msg_id, payload, counter), native in zip(plans, native_enqueues):
        h = Harness(raw)
        u = h.machine()
        h.install(u)
        u.mem_write(0xBC5D1C, struct.pack('<I', counter))
        u.mem_write(FRAME_SRC, payload)
        h.call(u, F['enqueue'], (msg_id, FRAME_SRC, len(payload)))
        counts['enqueue'] += 1
        original_queue = bytes(u.mem_read(QUEUE, RECORD * SLOTS))
        original_counter = struct.unpack('<I', u.mem_read(0xBC5D1C, 4))[0]
        expected_ok = msg_id not in (0,) and len(payload) <= 0x25A
        if native['ok'] != (1 if expected_ok else 0):
            note('enqueue', id=msg_id, counter=counter, field='ok',
                 original=int(expected_ok), native=native['ok'])
        if expected_ok:
            expected_counter = counter + 1
            if counter + 1 > 32000:
                expected_counter = 1
            if original_counter != expected_counter:
                note('enqueue', id=msg_id, counter=counter, field='counter',
                     original=original_counter, native=expected_counter)
            if native['seq'] != original_counter:
                note('enqueue', id=msg_id, counter=counter, field='native counter',
                     original=original_counter, native=native['seq'])
            original_record = original_queue[:RECORD]
            native_record = bytes.fromhex(native['records'][0][1])
            if original_record != native_record:
                offset = first_diff(original_record, native_record)
                note('enqueue', id=msg_id, counter=counter, field='record', offset=hex(offset),
                     original=original_record[max(offset, 0):max(offset, 0) + 8].hex(),
                     native=native_record[max(offset, 0):max(offset, 0) + 8].hex())
        elif original_queue != bytes(RECORD * SLOTS):
            note('enqueue', id=msg_id, counter=counter, field='empty queue')

    # full queue drop (65 enqueues)
    fills = [(0x0A, bytes([index]) * 0x10) for index in range(65)]
    native_fills = run_probe(probe, ['reset'] + [f'enqueue {i} {p.hex()}' for i, p in fills])[1:]
    h = Harness(raw)
    u = h.machine()
    h.install(u)
    for msg_id, payload in fills:
        u.mem_write(FRAME_SRC, payload)
        h.call(u, F['enqueue'], (msg_id, FRAME_SRC, len(payload)))
    counts['enqueue'] += 1
    original_queue = bytes(u.mem_read(QUEUE, RECORD * SLOTS))
    native_queue = queue_bytes(native_fills[-1]['records'])
    if original_queue != native_queue:
        slot = first_diff(original_queue, native_queue) // RECORD
        note('enqueue', field='full queue', slot=slot)
    if native_fills[-1]['ok'] != 0:
        note('enqueue', field='full drop accepted', original=0, native=native_fills[-1]['ok'])

    # ---- worker (0x46F4B0) + flush (0x4611C0) ----
    def worker_case(networked):
        plan = [(0x28, b''), (0x2C, b''), (0x2A, bytes(range(0x0F))), (0x09, bytes(range(0x0F))),
                (0x0A, bytes(range(0x10))), (0x22, bytes(range(0x28))), (0x14, struct.pack('<H', 3))]
        ticks = 6
        lines = ['reset', f'setnet {int(networked)}', 'setsingle 1']
        lines += [f'enqueue {i} {p.hex()}' for i, p in plan]
        lines.append(f'worker {ticks}')
        native = run_probe(probe, lines)
        native_ticks = native[2 + len(plan):]
        h = Harness(raw)
        u = h.machine()
        h.install(u)
        for msg_id, payload in plan:
            u.mem_write(FRAME_SRC, payload)
            h.call(u, F['enqueue'], (msg_id, FRAME_SRC, len(payload)))
        u.mem_write(0x67F528, struct.pack('<I', int(networked)))
        u.mem_write(0x501398, struct.pack('<I', VTABLE + 0x40))
        u.mem_write(VTABLE + 0x40 + 0x58, struct.pack('<I', SEND_STUB))
        u.mem_write(VTABLE + 0x40 + 0x54, struct.pack('<I', RECV_STUB))
        u.mem_write(0x8EA1C4, struct.pack('<I', 0))
        u.mem_write(0x8EE1CC, struct.pack('<I', 0))
        for tick in range(ticks):
            h.sent = []
            h.call(u, F['worker'])
            counts['worker'] += 1
            native_tick = native_ticks[1 + tick]
            # 0x4610E0 appends into one frame per tick; the flush sends it whole.
            native_frame = bytes.fromhex(native_tick['frame'])
            if not networked:
                original_frame = h.sent[-1] if h.sent else b''
                if original_frame != native_frame:
                    note('worker', tick=tick, field='frame',
                         original=original_frame.hex(), native=native_frame.hex())
            original_queue = bytes(u.mem_read(QUEUE, RECORD * SLOTS))
            native_queue = queue_bytes(native_tick['records'])
            if original_queue != native_queue:
                slot = first_diff(original_queue, native_queue) // RECORD
                note('worker', tick=tick, net=int(networked), field='queue', slot=slot)
            if networked:
                cursor = struct.unpack('<I', u.mem_read(0x8EE1CC, 4))[0]
                original_packet = bytes(u.mem_read(0x8EC1CC, cursor))
                native_packet = bytes.fromhex(native_tick['net'])
                if original_packet != native_packet:
                    note('worker', tick=tick, field='packet',
                         original=original_packet.hex()[:64], native=native_packet.hex()[:64])
            else:
                if native_tick['sent'] != h.sent[-1].hex() if h.sent else native_tick['sent'] != '':
                    note('worker', tick=tick, field='sent',
                         original=h.sent[-1].hex() if h.sent else '', native=native_tick['sent'])

    worker_case(False)
    worker_case(True)

    # ---- serialize (0x4610E0), including length mismatch ----
    serialize_plan = [(0x28, 1, b''), (0x2A, 2, bytes(range(0x0F))), (0x09, 4, bytes(range(0x0F))),
                      (0x12, 5, struct.pack('<H', 6) + bytes([1]) + b'T\x00\x00'),
                      (0x2D, 6, b''), (0x4F, 7, bytes(range(0x10))),
                      (0x2A, 8, bytes(range(5)))]
    lines = ['reset'] + [f'serialize {i} {s} {p.hex()}' for i, s, p in serialize_plan]
    native_serialize = run_probe(probe, lines)[1:]
    for (msg_id, sequence, payload), native in zip(serialize_plan, native_serialize):
        h = Harness(raw)
        u = h.machine()
        h.install(u)
        u.mem_write(FRAME_SRC, payload)
        u.mem_write(0x8EA1C4, struct.pack('<I', 0))
        u.mem_write(0x4F8714, struct.pack('<I', 1))
        h.call(u, F['serialize'], (msg_id, sequence, FRAME_SRC, len(payload)))
        counts['serialize'] += 1
        original_frame = bytes(u.mem_read(0x8E81C4, len(native['frame']) // 2))
        if original_frame != bytes.fromhex(native['frame']):
            note('serialize', id=msg_id, field='frame', original=original_frame.hex(),
                 native=native['frame'])
        flags, length = table_native[msg_id]
        expected_error = int(not flags & 0x01 and length != len(payload))
        original_errors = sum(1 for at, _ in h.calls if at == 0x4564B0)
        if original_errors != expected_error or native['errors'] != expected_error:
            note('serialize', id=msg_id, field='error count', original=original_errors,
                 native=native['errors'])

    # ---- producers ----
    def producer_case(commands, entry, call_args, msg_id, label):
        native = run_probe(probe, ['reset'] + commands)[-1]
        h = Harness(raw)
        u = h.machine()
        h.install(u)
        u.mem_write(0xC23B70, struct.pack('<I', 0x10203040))
        u.mem_write(0xC23B74, struct.pack('<I', 0x50607080))
        h.call(u, entry, call_args)
        counts['producer'] += 1
        original_queue = bytes(u.mem_read(QUEUE, RECORD * SLOTS))
        native_queue = queue_bytes(native['records'])
        if original_queue != native_queue:
            slot = first_diff(original_queue, native_queue) // RECORD
            note('producer', id=msg_id, label=label, slot=slot,
                 original=original_queue[max(slot, 0) * RECORD:max(slot, 0) * RECORD + 0x10].hex(),
                 native=native_queue[max(slot, 0) * RECORD:max(slot, 0) * RECORD + 0x10].hex())

    producer_case(['produce shot'], F['shot'], (), 0x28, 'shot')
    producer_case(['produce detonate'], F['detonate'], (), 0x2C, 'detonate')
    producer_case(['placement 4660 287454020 1432778632 2578103244 3699376128'],
                  F['placement'], (4660, 287454020, 1432778632, 2578103244, 3699376128), 0x2A,
                  'placement')
    producer_case(['produce trace'], F['trace'], (), 0x2F, 'trace')

    # ---- dispatch (0x46D9F0) ----
    for msg_id in DISPATCHABLE:
        for occupied in (False, True):
            payload = dispatch_fixture(msg_id)
            frame = build_frame(table_native, msg_id, 7, payload)
            lines = ['reset', 'setsingle 1', 'setnet 0', 'setlocal 3', 'setflag53c 5',
                     'settext 17 34 51 68 85 102']
            if occupied:
                lines += ['occupy players 2', 'occupy players 3', 'occupy players 4',
                          'occupy actors 3', 'occupy actors 4']
            lines.append(f'dispatch {frame.hex()}')
            native = run_probe(probe, lines)[-1]
            h = Harness(raw)
            u = h.machine()
            h.install(u)

            snapshot = {}
            u.mem_write(0x67F528, struct.pack('<I', 0))
            u.mem_write(0x67F520, struct.pack('<I', 0))
            u.mem_write(0x67F53C, struct.pack('<I', 5))
            u.mem_write(0xC23820, struct.pack('<I', ARRAY_BASE['players_7cca80'] + 3 * 0x114))
            u.mem_write(0x67DB40, struct.pack('<I', 0x11))
            u.mem_write(0x67DB48, struct.pack('<I', 0x33))
            u.mem_write(0x67DB4C, struct.pack('<I', 0x22))
            u.mem_write(0x67DB50, struct.pack('<I', 0x44))
            u.mem_write(0x67DB54, struct.pack('<I', 0x55))
            u.mem_write(0x67DB5C, struct.pack('<I', 0x66))
            u.mem_write(FRAME_SRC, frame)
            u.mem_write(0x8E61B4, struct.pack('<I', FRAME_SRC))
            u.mem_write(0x8E61B8, struct.pack('<I', len(frame)))
            u.mem_write(0xBCFC24, struct.pack('<I', 0))
            h.seed_records(u, occupied=('players_7cca80', 'actors_7dde80') if occupied else (),
                           snapshot=snapshot)
            h.call(u, F['dispatch'])
            counts['dispatch'] += 1
            # arrays
            after = {name: bytes(u.mem_read(base, size)) for name, (base, size) in ARRAYS.items()}
            expected = apply_record_writes(snapshot, native['actions'])
            for name in ARRAYS:
                if after[name] != expected[name]:
                    offset = first_diff(after[name], expected[name])
                    index, inner = divmod(offset, 0x114)
                    note('dispatch', id=msg_id, occupied=int(occupied), array=name, record=index,
                         offset=hex(inner), original=after[name][offset], native=expected[name][offset])
            # queue follow-ups + cancel behaviour
            original_queue = bytes(u.mem_read(QUEUE, RECORD * SLOTS))
            native_queue = queue_bytes(native['followups'])
            if original_queue != native_queue:
                slot = first_diff(original_queue, native_queue) // RECORD
                note('dispatch', id=msg_id, occupied=int(occupied), field='queue', slot=slot)
            # dedup ring
            original_ring = list(struct.unpack('<256H', bytes(u.mem_read(RING, 0x200))))
            if original_ring != native['state']['ring']:
                note('dispatch', id=msg_id, occupied=int(occupied), field='ring')
            # globals from state_json
            def dword(at):
                return struct.unpack('<I', u.mem_read(at, 4))[0]
            state_now = {'flag14': int(bool(dword(0xBC5D14))), 'flag18': int(bool(dword(0xBC5D18))),
                         'delay': dword(0x50321C) & 0xFFFF, 'value548': dword(0x67F548),
                         'c2c': struct.unpack('<i', u.mem_read(0xBCFC2C, 4))[0],
                         'c30': struct.unpack('<i', u.mem_read(0xBCFC30, 4))[0],
                         'c34': dword(0xBCFC34), 'ctl': dword(0xBCFC14), 'dbe8': dword(0x67DBE8),
                         'b78': dword(0xC23B78), 'dbbc': dword(0x67DBBC),
                         'a158c0': dword(0xA158C0), 'fa18': dword(0x8CFA18),
                         'fa1c': dword(0x8CFA1C), 'f904': dword(0x8CF904), 'f900': dword(0x8CF900)}
            for key, value in state_now.items():
                if key in native['state'] and native['state'][key] != value:
                    note('dispatch', id=msg_id, occupied=int(occupied), field=key,
                         original=value, native=native['state'][key])
            # 0x8CF908 client state copy
            if msg_id == 0x03:
                original_block = bytes(u.mem_read(0x8CF908, 0x10C))
                native_block = bytes.fromhex(next((a['bytes'] for a in native['actions']
                                                   if a['kind'] == 'client_state'), ''))
                if original_block != native_block:
                    note('dispatch', id=msg_id, field='client state')
            # 0x50352C blob
            if msg_id == 0x3D:
                original_blob = bytes(u.mem_read(0x50352C, 0x100))
                if original_blob.hex() != native['state']['blob']:
                    note('dispatch', id=msg_id, field='blob')
            # session strings
            if msg_id == 0x42:
                original_first = bytes(u.mem_read(0xBCF624, 0x20)).split(b'\0')[0]
                original_second = bytes(u.mem_read(0x67F438, 0x40)).split(b'\0')[0]
                if original_first.hex() + '00' != native['state']['s624']:
                    note('dispatch', id=msg_id, field='session string')
                if original_second.hex() + '00' != native['state']['f438']:
                    note('dispatch', id=msg_id, field='mission file')
            # scoreboard writes (absolute addresses)
            if msg_id == 0x15:
                for action in native['actions']:
                    if action['array'] == 'none':
                        for offset, width, value in action['writes']:
                            original_value = struct.unpack('<I', u.mem_read(offset, 4))[0]
                            if original_value != value:
                                note('dispatch', id=msg_id, field='scoreboard', offset=hex(offset),
                                     original=original_value, native=value)
                original_pairs = [struct.unpack('<i', u.mem_read(at, 4))[0]
                                  for at in (0xBCFBF0, 0xBCFBFC, 0xBCFC08)]
                native_pairs = [native['state']['sb_a'][0], native['state']['sb_b'][0],
                                native['state']['sb_c'][0]]
                if original_pairs != native_pairs:
                    note('dispatch', id=msg_id, field='scoreboard pairs',
                         original=original_pairs, native=native_pairs)
            # event call sequence
            original_events = [(at, args) for at, args in h.calls
                               if at not in HOST_STUBS and at not in UI_STUBS]
            native_events = []
            for action in native['actions']:
                kind = action['kind']
                action_args = action['args']
                if kind == 'hud_text':
                    continue  # UI_STUBS filtered on the original side
                if kind == 'local_respawn':
                    native_events.append((0x4848C0, []))
                    if action_args[1]:
                        native_events.append((0x48DD60, []))
                    if action_args[3]:
                        native_events.append((0x42CEF0, [action_args[0], action_args[2]]))
                elif kind == 'mission_state':
                    native_events.append((0x4848C0, []))
                    native_events.append((0x43E720, [0x164]))
                elif kind == 'mission_ok':
                    native_events.append((0x43E720, [0x1AF]))
                elif kind == 'mission_denied':
                    native_events.append((0x43E720, [0x1B0]))
                elif kind == 'local_death':
                    if action_args[3]:
                        native_events.append((0x484ED0, []))
                elif action['address']:
                    native_events.append((action['address'], action_args))
            if msg_id == 0x07 and any(a['kind'] == 'mbp_error' for a in native['actions']):
                original_events = [(at, args) for at, args in h.calls if at in (0x479F00, 0x4564B0)]
                native_events = [(0x479F00, []), (0x4564B0, [])]
            if [at for at, _ in original_events] != [at for at, _ in native_events]:
                note('dispatch', id=msg_id, occupied=int(occupied), field='call sequence',
                     original=[hex(at) for at, _ in original_events],
                     native=[hex(at) for at, _ in native_events])
            else:
                for (at, original_args), (_nat, native_args) in zip(original_events, native_events):
                    if at == 0x490000:
                        # the original calls [record + 0x110]; the port action carries
                        # the record index instead of the pointer
                        pointer = original_args[0]
                        index = next((action['index'] for action in native['actions']
                                      if action['address'] == 0x490000), -1)
                        expected_pointer = ARRAY_BASE['players_7cca80'] + index * 0x114
                        if pointer != expected_pointer:
                            note('dispatch', id=msg_id, field='callback pointer',
                                 original=pointer, native=expected_pointer)
                        continue
                    if at in NUMERIC_ARGS:
                        count = NUMERIC_ARGS[at]
                        original_norm = [value & 0xFFFFFFFF for value in original_args[:count]]
                        native_norm = [value & 0xFFFFFFFF for value in native_args[:count]]
                        if original_norm != native_norm:
                            note('dispatch', id=msg_id, field='call args', address=hex(at),
                                 original=original_norm, native=native_norm)
                    elif at == 0x462F20:
                        original_norm = [v & 0xFFFFFFFF for v in original_args[:6]]
                        native_norm = [v & 0xFFFFFFFF for v in native_args[:6]]
                        if original_norm != native_norm:
                            note('dispatch', id=msg_id, field='projectile args',
                                 original=original_norm, native=native_norm)
            # UI config values checked per case
            if msg_id == 0x12:
                color = next((args[1] for at, args in h.calls if at == 0x479F00), None)
                native_color = next((a['args'][1] for a in native['actions']
                                     if a['kind'] == 'hud_text'), None)
                if color != native_color:
                    note('dispatch', id=msg_id, field='hud color', original=color,
                         native=native_color)
            if msg_id in (0x1D, 0x2D, 0x2E):
                config = next((args[2] for at, args in h.calls if at == 0x404CF0), None)
                key = 'mission_state' if msg_id == 0x1D else (
                    'mission_ok' if msg_id == 0x2D else 'mission_denied')
                native_config = next((a['args'][0] if key == 'mission_state' else a['args'][1]
                                      for a in native['actions'] if a['kind'] == key), None)
                if config != native_config:
                    note('dispatch', id=msg_id, field='config', original=config, native=native_config)

    # ---- zero length messages with no case fail closed ----
    # The original stream would not advance (infinite loop); these ids belong to
    # the peer/transport layer. The port reports undispatchable and stops.
    for msg_id in UNDISPATCHABLE:
        payload = b''
        frame = build_frame(table_native, msg_id, 7, payload)
        native = run_probe(probe, ['reset', 'setsingle 1', 'setnet 0',
                                   f'dispatch {frame.hex()}'])[-1]
        counts['dispatch'] += 1
        if not native['actions'] or native['actions'][0]['kind'] != 'undispatchable':
            note('dispatch', id=msg_id, field='undispatchable',
                 original='no advance', native=[a['kind'] for a in native['actions']])

    # Live routing uses current globals and synchronous required handlers.
    live_ids = [0x09,0x0A,0x0B,0x0D,0x14,0x19,0x1A,0x1B,0x1E,0x20,0x23,0x25,
                0x0E,0x29,0x34,0x35,0x36,0x37,0x38,0x3B,0x30,0x33,0x39,0x3A,0x3C,0x3E,0x3F,0x41,0x43,0x44,0x46,0x47,0x48]
    live_cases=[]
    rng=random.Random(0x46D9F0461050)
    for msg_id in live_ids:
        for gate in [0,1]:
            for mutation in [0,1]:
                for duplicate in [0,1]:
                    payload=bytearray(rng.randrange(256) for _ in range(table_native[msg_id][1]))
                    if msg_id==0x09:
                        struct.pack_into('<h',payload,8,3)
                    if msg_id in [0x29,0x34,0x35,0x36,0x37,0x38]:struct.pack_into('<h',payload,0,3)
                    if msg_id==0x0e:struct.pack_into('<H',payload,2,0x8003)
                    if msg_id==0x3b:struct.pack_into('<h',payload,12,3)
                    if msg_id==0x39:struct.pack_into('<h',payload,0,3)
                    frame=build_frame(table_native,msg_id,7,payload)
                    if duplicate:frame+=frame
                    live_cases.append((msg_id,gate,mutation,frame))
    native_live=run_probe(probe,[f'live {frame.hex()} {gate} {mutation}' for _,gate,mutation,frame in live_cases])
    globals_at=[0xbc5d14,0xbc5d18,0x50321c,0x67f548,0xbcfc14,0xc23b78,0x67dbbc,0xa158c0]
    payload_calls={0x470d50,0x471010,0x470400,0x4715b0,0x471760,0x471810,0x471870}
    for (msg_id,gate,mutation,frame),native in zip(live_cases,native_live):
        h=Harness(raw);u=h.machine()
        payload_at=FRAME_SRC+1+(2 if table_native[msg_id][0]&6 else 0)
        mutate_counts={0x45dc70:0}
        def mutate(machine,address,size,user):
            if not mutation:return
            if address==0x477820:machine.mem_write(0x7dde80+3*0x114+0x7c,struct.pack('<I',0xf1234567))
            if address==0x455af0:machine.mem_write(payload_at+8,struct.pack('<I',77))
            if address==0x45dc70:
                offset=mutate_counts[address]*(1+table_native[msg_id][1])
                machine.mem_write(payload_at+offset+14,struct.pack('<H',55));mutate_counts[address]+=1
        u.hook_add(UC_HOOK_CODE,mutate)
        h.install(u)
        h.seed_records(u,occupied=('players_7cca80','actors_7dde80'))
        replication=msg_id in [0x0e,0x29,0x34,0x35,0x36,0x37,0x38,0x3b]
        for base in [0x7cca80,0x7dde80]:
            u.mem_write(base+3*0x114,struct.pack('<I',0x04030204 if gate or not replication else 0))
            u.mem_write(base+3*0x114+0x6c,struct.pack('<I',0xa1b2c3d4))
        for at in globals_at+[0xbcfc24]:u.mem_write(at,bytes(4))
        u.mem_write(RING,bytes(512));u.mem_write(QUEUE,bytes(RECORD*SLOTS))
        u.mem_write(0x67f528,struct.pack('<I',gate))
        u.mem_write(FRAME_SRC,frame);u.mem_write(0x8e61b4,struct.pack('<I',FRAME_SRC));u.mem_write(0x8e61b8,struct.pack('<I',len(frame)))
        record_writes=[]
        def record_write(machine,access,address,size,value,user):
            for base in [0x7cca80,0x7dde80]:
                if base+3*0x114<=address<base+4*0x114:
                    record_writes.append([base,3,address-base-3*0x114,size,value&((1<<(size*8))-1)])
        u.hook_add(UC_HOOK_MEM_WRITE,record_write)
        h.call(u,F['dispatch']);counts['live_dispatch']=counts.get('live_dispatch',0)+1
        calls=[]
        for address,arguments in h.calls:
            if address in HOST_STUBS or address in UI_STUBS:continue
            if address in payload_calls:arguments=[]
            if address==0x4560c0:arguments=arguments[1:]
            calls.append([address,*[value&0xffffffff for value in arguments]])
        expected={'calls':calls,'globals':[struct.unpack('<I',u.mem_read(at,4))[0] for at in globals_at],
                  'received':struct.unpack('<I',u.mem_read(0xbcfc24,4))[0],
                  'ring':list(struct.unpack('<256H',u.mem_read(RING,512)))}
        for field,value in expected.items():
            if native[field]!=value:note('live_dispatch',id=msg_id,gate=gate,mutation=mutation,field=field,original=value,native=native[field])
        if replication:
            if native['writes']!=record_writes:note('live_dispatch',id=msg_id,field='writes',original=record_writes,native=native['writes'])
            expected_records=[bytes(u.mem_read(base+3*0x114,0x114)).hex() for base in [0x7cca80,0x7dde80]]
            if native['records']!=expected_records:note('live_dispatch',id=msg_id,field='records',original=expected_records,native=native['records'])
        if queue_bytes(native['queue'])!=bytes(u.mem_read(QUEUE,RECORD*SLOTS)):
            note('live_dispatch',id=msg_id,field='queue')

    native_receive=run_probe(probe,[f'receive_flags {flags}' for flags in range(4)])
    for flags,native in enumerate(native_receive):
        h=Harness(raw);u=h.machine();h.install(u);h.received=[bytes([0x0a])+bytes(16)]
        for address,value in [(0x67f520,flags&1),(0x67f528,(flags>>1)&1),(0x8ec1c8,17),
                              (0x501398,VTABLE+0x40),(VTABLE+0x40+0x54,RECV_STUB),
                              (0x50135c,FRAME_SRC+0x8000),(0x501360,17)]:
            u.mem_write(address,struct.pack('<I',value))
        h.call(u,F['receive']);counts['receive']=counts.get('receive',0)+1
        expected=[int(u.reg_read(UC_X86_REG_EAX)!=0),len(h.received),struct.unpack('<I',u.mem_read(0x8ec1c8,4))[0]]
        if native['state']!=expected:note('receive',flags=flags,original=expected,native=native['state'])

    total = sum(counts.values())
    if mismatches:
        print(f'{len(mismatches)} mismatches over {total} cases')
        for row in mismatches[:20]:
            print(' ', json.dumps(row)[:240])
    else:
        print(f'all {total} cases match; counts={counts}')
    if args.report:
        args.report.write_text(json.dumps({'counts': counts, 'mismatches': mismatches}, indent=1))
    return 1 if mismatches else 0


if __name__ == '__main__':
    sys.exit(main())
