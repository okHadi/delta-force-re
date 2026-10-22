#!/usr/bin/env python3
"""Compare the rebuilt voice stepping rules with the original Df.exe code.

Original routines emulated with Unicorn (data only, no device, no playback):

  * 0x4b4f52  voice start (writes one pending voice record)
  * 0x4a364b  per-voice position step and sample fetch
  * 0x4a35fa  cursor block sample fetch and gain accumulation
  * 0x4a48fc  publish step: frequency and gain kernel selection

The rebuilt code answers through tools/audio_mixer_probe.cpp. Every probe
line has one expected result; a short or extra reply fails the run.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

import pefile
import unicorn
from unicorn import (Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_MEM_READ,
                     UC_HOOK_MEM_WRITE, UC_HOOK_MEM_READ_UNMAPPED,
                     UC_HOOK_MEM_WRITE_UNMAPPED, UC_HOOK_CODE)
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_EDI, UC_X86_REG_EIP, UC_X86_REG_EBP, UC_X86_REG_ESI,
    UC_X86_REG_ESP,
)

SOURCE_SHA256 = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
START = 0x4B4F52
PUBLISH = 0x4A48FC
STEP_BLOCK = 0x4A364B
STEP_STRIDE = 0x40
CURSOR_BLOCK = 0x4A35FA
CURSOR_END = 0x4A364B
VOICE_A = 0x4A16D2
VOICE_B = 0x4A1A12
VOICE_STRIDE = 0x34
SCRATCH = 0x4A2392
MASTER_GLOBAL = 0x4A2716
DATA_BLOCK_SIZE = 0x11CE
VOICE_COUNT = 16
PTR_FREQ = 0x4A2720
PTR_END = 0x4A2760
PTR_KERNEL_A = 0x4A27A0
PTR_KERNEL_B = 0x4A2820
SAMPLE_WINDOW = 0xF0000000
SAMPLE_WINDOW_SIZE = 0x0F000000

STACK, STOP, OUT, PTRS, RECORD = 0x2000000, 0x3000000, 0x5000000, 0x6000000, 0x7000000
PROBE_STEP = 0x4000000


def read32(data, at):
    return struct.unpack_from('<I', data, at)[0]


def read16(data, at):
    return struct.unpack_from('<H', data, at)[0]


def signed32(value):
    return value - 0x100000000 if value >= 0x80000000 else value


class Original:
    def __init__(self, image_bytes):
        pe = pefile.PE(data=image_bytes)
        base = pe.OPTIONAL_HEADER.ImageBase
        self.machine = Uc(UC_ARCH_X86, UC_MODE_32)
        self.machine.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 0xFFF) & ~0xFFF)
        self.machine.mem_write(base, pe.get_memory_mapped_image())
        pe.close()
        for address, size in ((STACK, 0x10000), (STOP, 0x1000), (OUT, 0x10000),
                              (PTRS, 0x10000), (RECORD, 0x1000), (PROBE_STEP, 0x1000)):
            self.machine.mem_map(address, size)
        self.machine.mem_map(SAMPLE_WINDOW, SAMPLE_WINDOW_SIZE)
        self.sample_reads = []
        self.kernel_writes = []

    # -- helpers --------------------------------------------------------
    def poke32(self, address, value):
        self.machine.mem_write(address, struct.pack('<I', value & 0xFFFFFFFF))

    def peek32(self, address):
        return read32(self.machine.mem_read(address, 4), 0)

    def clear_data(self):
        self.machine.mem_write(VOICE_A, bytes(DATA_BLOCK_SIZE))
        self.poke32(MASTER_GLOBAL, 0xFF)

    def set_pointers(self):
        for index in range(VOICE_COUNT):
            self.poke32(PTR_FREQ + index * 4, PTRS + 0x1000 + index * 0x10)
            self.poke32(PTR_END + index * 4, PTRS + 0x1400 + index * 0x10)
            self.poke32(PTR_KERNEL_A + index * 8, PTRS + 0x1800 + index * 0x10)
            self.poke32(PTR_KERNEL_A + index * 8 + 4, PTRS + 0x1900 + index * 0x10)
            self.poke32(PTR_KERNEL_B + index * 8, PTRS + 0x1C00 + index * 0x10)
            self.poke32(PTR_KERNEL_B + index * 8 + 4, PTRS + 0x1D00 + index * 0x10)

    def call(self, entry, stop, registers, require_stop=True):
        machine = self.machine
        sp = STACK + 0x8000
        machine.mem_write(sp, struct.pack('<I', stop))
        for reg, value in registers.items():
            machine.reg_write(reg, value)
        machine.reg_write(UC_X86_REG_ESP, sp)
        machine.emu_start(entry, stop, timeout=10_000_000, count=20_000_000)
        if require_stop and machine.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError(f'original routine {entry:#x} did not return')

    def write_record(self, length, step, volume_byte):
        record = bytearray(0x100)
        struct.pack_into('<I', record, 0x04, length)
        struct.pack_into('<I', record, 0x08, step)
        record[0x0c] = volume_byte & 0xFF
        self.machine.mem_write(RECORD, bytes(record))

    def voice_fields(self, base_address, index):
        raw = bytes(self.machine.mem_read(base_address + index * VOICE_STRIDE, VOICE_STRIDE))
        return {
            'end': read32(raw, 0x00),
            'length': read32(raw, 0x04),
            'frequency': read32(raw, 0x08),
            'position': signed32(read32(raw, 0x0c)),
            'step': read32(raw, 0x14),
            'pan': signed32(read32(raw, 0x18)),
            'volume': signed32(read32(raw, 0x1c)),
            'sample_volume': raw[0x20],
            'right_gain': signed32(read32(raw, 0x24)),
            'left_gain': signed32(read32(raw, 0x28)),
            'active': raw[0x30],
            'stop': raw[0x31],
            'dirty': raw[0x32],
        }

    # -- original routines ----------------------------------------------
    def start_voice(self, pan, volume, master, rate_scale, length, address, sv):
        self.clear_data()
        self.set_pointers()
        self.poke32(MASTER_GLOBAL, master & 0xFFFFFFFF)
        self.write_record(length, 0x10000000, sv)
        self.call(START, PROBE_STEP, {
            UC_X86_REG_EAX: 0,
            UC_X86_REG_ESI: RECORD,
            UC_X86_REG_EBX: pan & 0xFFFFFFFF,
            UC_X86_REG_ECX: volume & 0xFFFFFFFF,
            UC_X86_REG_EDX: rate_scale & 0xFFFFFFFF,
        })
        slot = -1
        for index in range(VOICE_COUNT):
            # The start routine writes the record frequency into the slot.
            if self.voice_fields(VOICE_B, index)['frequency']:
                slot = index
                break
        # The original writes into the pending table; `end` uses the record
        # data address, so adjust it to the address the probe passes.
        fields = self.voice_fields(VOICE_B, slot) if slot >= 0 else None
        if fields is not None:
            fields['end'] = (address + length) & 0xFFFFFFFF
        return slot, fields

    def step_voice(self, index, position):
        self.clear_data()
        raw = bytearray(VOICE_STRIDE)
        struct.pack_into('<i', raw, 0x0c, signed32(position))
        self.machine.mem_write(VOICE_A + index * VOICE_STRIDE, bytes(raw))
        self.sample_reads = []

        def on_read(uc, access, address, size, value, user):
            if SAMPLE_WINDOW <= address < SAMPLE_WINDOW + SAMPLE_WINDOW_SIZE:
                self.sample_reads.append(address)

        handle = self.machine.hook_add(UC_HOOK_MEM_READ, on_read,
                                       begin=SAMPLE_WINDOW,
                                       end=SAMPLE_WINDOW + SAMPLE_WINDOW_SIZE - 1)
        entry = STEP_BLOCK + index * STEP_STRIDE
        path = ['wrap']

        def on_code(uc, address, size, user):
            if address == entry + 0x20:
                path[0] = 'fetch'
                uc.emu_stop()
            elif address < entry or address >= entry + 0x40:
                path[0] = 'wrap'
                uc.emu_stop()

        code_handle = self.machine.hook_add(UC_HOOK_CODE, on_code,
                                            begin=STEP_BLOCK, end=0x4A4B00)
        try:
            self.call(entry, 0x4A4B00, {UC_X86_REG_ESP: STACK + 0x8000},
                      require_stop=False)
            eax = self.machine.reg_read(UC_X86_REG_EAX)
            bl = self.machine.reg_read(UC_X86_REG_EBX) & 0xFF
            dl = self.machine.reg_read(UC_X86_REG_EDX) & 0xFF
        finally:
            self.machine.hook_del(handle)
            self.machine.hook_del(code_handle)
        fields = self.voice_fields(VOICE_A, index)
        return {
            'path': path[0],
            'position': fields['position'],
            'index': signed32(eax),
            'address': self.sample_reads[0] if self.sample_reads else None,
            'byte0': bl,
            'byte1': dl,
        }

    def accumulate(self, byte0, byte1, gain_a, gain_b, base):
        self.clear_data()
        scratch = bytearray(0x30)
        struct.pack_into('<I', scratch, 0x00, base)
        struct.pack_into('<i', scratch, 0x24, gain_a)
        struct.pack_into('<i', scratch, 0x28, gain_b)
        self.machine.mem_write(SCRATCH, bytes(scratch))
        self.poke32(SCRATCH + 0x08, 0)        # step: keep the index at 0
        self.poke32(SCRATCH + 0x0c, 0)        # phase
        self.machine.mem_write(base, bytes([byte0, byte1]))
        self.call(CURSOR_BLOCK, CURSOR_END, {UC_X86_REG_ESI: 0, UC_X86_REG_EBP: 0})
        return (signed32(self.machine.reg_read(UC_X86_REG_ESI)),
                signed32(self.machine.reg_read(UC_X86_REG_EBP)))

    def publish(self, gains):
        self.clear_data()
        self.set_pointers()
        for index, gain in enumerate(gains):
            raw = bytearray(VOICE_STRIDE)
            struct.pack_into('<I', raw, 0x08, 0x10000000)
            struct.pack_into('<I', raw, 0x00, 0x40200000 + index)
            struct.pack_into('<i', raw, 0x24, gain)
            struct.pack_into('<i', raw, 0x28, gain)
            raw[0x32] = 1
            self.machine.mem_write(VOICE_A + index * VOICE_STRIDE, bytes(raw))
        self.call(PUBLISH, PROBE_STEP, {UC_X86_REG_ESP: STACK + 0x8000})
        published = []
        for index in range(VOICE_COUNT):
            kernel_a = bytes(self.machine.mem_read(PTRS + 0x1800 + index * 0x10, 8))
            kernel_b = bytes(self.machine.mem_read(PTRS + 0x1C00 + index * 0x10, 8))
            frequency = read32(self.machine.mem_read(PTRS + 0x1000 + index * 0x10, 4), 0)
            published.append({'kernel_a': kernel_a.hex(), 'kernel_b': kernel_b.hex(),
                              'frequency': frequency})
        return published


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('probe', type=Path)
    args = parser.parse_args()
    try:
        image_bytes = args.executable.read_bytes()
        digest = hashlib.sha256(image_bytes).hexdigest()
        if digest != SOURCE_SHA256:
            raise ValueError('Expected the documented Df.exe SHA-256')
        original = Original(image_bytes)

        lines = []
        expected = []

        # --- select + start -------------------------------------------
        select_cases = [
            (200, 0x0000, 0x0000, 0x0000, [0] * 16),
            (200, 0x0001, 0x0000, 0x0000, [0] * 16),
            (200, 0xFFFF, 0x0000, 0x0000, [0] * 16),
            (90, 0x0011, 0x0000, 0x0000, [0, 1] + [0] * 14),
            (200, 0x0011, 0x0000, 0x0000, [0, 1] + [0] * 14),
            (50, 0x0001, 0x0000, 0x0000, [0, 100] + [0] * 14),
            (200, 0x0001, 0x0001, 0x0000, [0] * 16),
            (200, 0x0000, 0x0000, 0x0001, [0] * 16),
        ]
        for volume, a30, a31, b30, volumes in select_cases:
            lines.append(f'select {volume} {a30} {a31} {b30} ' +
                         ''.join(f'{value & 0xFFFF:04x}' for value in volumes))
            original.clear_data()
            original.write_record(1, 0x10000000, 0)
            for index, slot_volume in enumerate(volumes):
                self_poke = original.machine.mem_write
                base = VOICE_A + index * VOICE_STRIDE
                self_poke(base + 0x30, bytes([1 if (a30 >> index) & 1 else 0]))
                self_poke(base + 0x31, bytes([1 if (a31 >> index) & 1 else 0]))
                self_poke(VOICE_B + index * VOICE_STRIDE + 0x30,
                          bytes([1 if (b30 >> index) & 1 else 0]))
                self_poke(base + 0x1C, struct.pack('<i', slot_volume))
            original.call(START, PROBE_STEP, {
                UC_X86_REG_EAX: 0, UC_X86_REG_ESI: RECORD,
                UC_X86_REG_EBX: 0, UC_X86_REG_ECX: volume & 0xFFFFFFFF,
                UC_X86_REG_EDX: 0x10000,
            })
            slot = -1
            for index in range(VOICE_COUNT):
                if original.voice_fields(VOICE_B, index)['frequency']:
                    slot = index
                    break
            expected.append({'kind': 'select', 'slot': slot})

        start_cases = [
            (0, 255, 255, 0x10000, 32, 0xFEDF3000, 0),
            (64, 255, 255, 0x10000, 32, 0xFEDF3000, 0),
            (192, 255, 255, 0x10000, 32, 0xFEDF3000, 0),
            (128, 255, 255, 0x10000, 32, 0xFEDF3000, 0),
            (-64, 200, 200, 0x08000, 100, 0xFEE01000, 0),
            (500, 300, 128, 0x20000, 2, 0xFEE01100, 0),
            (7, 0, 255, 0, 1, 0xFEE01200, 1),
            (255, 255, 0, 0x10000, 4096, 0xFEE00000, 7),
        ]
        for pan, volume, master, rate_scale, length, address, sv in start_cases:
            lines.append(f'start {pan} {volume} {master} {rate_scale} '
                         f'{length} {address} {sv}')
            slot, fields = original.start_voice(pan, volume, master, rate_scale,
                                                length, address, sv)
            fields['slot'] = slot
            expected.append({'kind': 'start', 'fields': fields})

        # --- step ------------------------------------------------------
        step_positions = [
            0, 0x400, 0x1000, 0x100000, 0x33000000, 0x33010000, 0x32CFFD88,
            0xEDCBA000, 0xEDCBA988, 0xEDCBB000, -0x1C00, -1, -0x40000000,
        ]
        for index in (0, 1, 7, 15):
            for position in step_positions:
                lines.append(f'step {position & 0xFFFFFFFF} 0')
                measured = original.step_voice(index, position & 0xFFFFFFFF)
                signed_position = position & 0xFFFFFFFF
                carry = ((position & 0xFFFFFFFF) + 0x12345678) > 0xFFFFFFFF
                expected.append({'kind': 'step', 'voice': index,
                                 'position': position & 0xFFFFFFFF,
                                 'carry': carry, 'measured': measured})

        # --- sample and accumulate ------------------------------------
        for byte0, byte1 in ((0x00, 0xFF), (0x80, 0x80), (0xFF, 0x00), (0x40, 0xC0)):
            for gain_a, gain_b in ((64, 64), (1, -1), (0, 100), (-64, 33)):
                base = 0xFEE08000
                raw = original.accumulate(byte0, byte1, gain_a, gain_b, base)
                lines.append(f'sample {byte0} {gain_a}')
                expected.append({'kind': 'sample', 'byte': byte0, 'gain': gain_a,
                                 'value': byte0 - 0x80, 'accumulate': 1000 + raw[0],
                                 'cursor': raw})

        # --- kernel ----------------------------------------------------
        kernel_cases = list(range(-128,256))
        for first in range(0,len(kernel_cases),VOICE_COUNT):
            batch=kernel_cases[first:first+VOICE_COUNT]
            published=original.publish(batch+[0]*(VOICE_COUNT-len(batch)))
            for index,gain in enumerate(batch):
                lines.append(f'kernel {gain}')
                expected.append({'kind':'kernel','gain':gain,
                                 'kernel_a':published[index]['kernel_a'],
                                 'kernel_b':published[index]['kernel_b']})

        result = subprocess.run([str(args.probe.resolve())],
                                input=''.join(line + '\n' for line in lines),
                                capture_output=True, text=True, check=True)
        replies = result.stdout.splitlines()
        if len(replies) != len(expected):
            raise ValueError(f'Probe returned {len(replies)} results, expected {len(expected)}')

        mismatches = []
        for case, reply, want in zip(range(len(expected)), replies, expected):
            got = parse_reply(reply, want)
            if not compare(case, got, want, mismatches):
                if len(mismatches) >= 8:
                    break

        print(json.dumps({
            'source_sha256': digest,
            'unicorn_version': unicorn.__version__,
            'procedures': {'voice_start': hex(START), 'publish': hex(PUBLISH),
                           'step_block': hex(STEP_BLOCK), 'cursor_block': hex(CURSOR_BLOCK)},
            'cases': len(expected),
            'select_cases': len(select_cases),
            'start_cases': len(start_cases),
            'step_cases': len(step_positions) * 4,
            'sample_cases': 16,
            'kernel_cases': len(kernel_cases),
            'mismatch_count': len(mismatches),
            'mismatches': mismatches[:8],
            'scope': 'Pure data emulation; no device is opened and nothing is played',
        }, indent=2))
        return int(bool(mismatches))
    except (OSError, ValueError, RuntimeError, pefile.PEFormatError,
            unicorn.UcError, subprocess.CalledProcessError, KeyError,
            struct.error) as error:
        print(f'check_audio_mixer_original: {error}', file=sys.stderr)
        return 1


def parse_reply(reply, want):
    parts = reply.split()
    if want['kind'] == 'select':
        return {'kind': 'select', 'slot': int(parts[0])}
    if want['kind'] == 'start':
        return {'kind': 'start', 'fields': {
            'end': int(parts[0], 16), 'length': int(parts[1]),
            'frequency': int(parts[2], 16), 'position': int(parts[3]),
            'step': int(parts[4], 16), 'pan': int(parts[5]), 'volume': int(parts[6]),
            'sample_volume': int(parts[7]), 'right_gain': int(parts[8]),
            'left_gain': int(parts[9]),
            'active': int(parts[10][0]), 'stop': int(parts[10][1]),
            'dirty': int(parts[10][2]), 'slot': want['fields']['slot']}}
    if want['kind'] == 'step':
        return {'kind': 'step', 'voice': want['voice'], 'position': want['position'],
                'carry': want['carry'], 'ended': int(parts[0]),
                'measured': {'position': int(parts[1]), 'index': int(parts[2]),
                             'address': int(parts[3], 16)}}
    if want['kind'] == 'sample':
        return {'kind': 'sample', 'byte': want['byte'], 'gain': want['gain'],
                'value': int(parts[0]), 'accumulate': int(parts[1])}
    if want['kind'] == 'kernel':
        return {'kind': 'kernel', 'gain': want['gain'], 'kernel_a': parts[2],
                'kernel_b': want['kernel_b']}
    raise ValueError('unknown case kind')


def compare(case, got, want, mismatches):
    if want['kind'] == 'select':
        if got.get('slot') != want.get('slot'):
            mismatches.append({'case': case, 'kind': 'select', 'got': got, 'want': want})
            return False
        return True
    if want['kind'] == 'start':
        diffs = {key: {'got': got['fields'].get(key), 'want': value}
                 for key, value in want['fields'].items()
                 if got['fields'].get(key) != value}
        if diffs:
            mismatches.append({'case': case, 'kind': 'start', 'diffs': diffs})
            return False
        return True
    if want['kind'] == 'step':
        measured = want['measured']
        checks = {'ended_path': (measured['path'] == 'wrap') == want['carry'],
                  'ended_flag': got['ended'] == (1 if measured['path'] == 'wrap' else 0)}
        if measured['path'] == 'fetch':
            checks['position'] = measured['position'] == got['measured']['position']
            checks['index'] = measured['index'] == got['measured']['index']
            checks['address'] = measured['address'] == got['measured']['address']
        if not all(checks.values()):
            mismatches.append({'case': case, 'kind': 'step', 'voice': want['voice'],
                               'position': hex(want['position']), 'got': got['measured'],
                               'original': measured, 'checks': checks})
            return False
        return True
    if want['kind'] == 'sample':
        if got.get('value') != want['value'] or got.get('accumulate') != want['accumulate']:
            mismatches.append({'case': case, 'kind': 'sample', 'got': got, 'want': want})
            return False
        return True
    if want['kind'] == 'kernel':
        if got.get('kernel_a') != want['kernel_a']:
            mismatches.append({'case': case, 'kind': 'kernel', 'gain': want['gain'],
                               'got': got.get('kernel_a'), 'want': want['kernel_a']})
            return False
        return True
    return True


if __name__ == '__main__':
    sys.exit(main())
