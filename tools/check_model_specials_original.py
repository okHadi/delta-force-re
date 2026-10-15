#!/usr/bin/env python3
"""Compare the rebuilt special-model plans with the original Df.exe callbacks.

Both callbacks of every owned row are emulated:

  * the draw callback with 0x46b1d0 (mesh draw) stubbed, so every draw and the
    exact matrix are captured, plus the object-field writes and the 0xad3cf4
    counter writes;
  * the matrix callback, so every written slot of the 0xc22b80 scratch, the
    per-slot +0x3c localized flags and the returned pointer are captured.

The base matrix is built by the original routine (0x45d1e0, or 0x45d2f0 for
scaled rows) from the same placement the probe receives, so the comparison
covers the whole plan. Only data is emulated; nothing is drawn.

The rebuild must answer every line; a short or extra reply fails the run.
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
from unicorn import (Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_WRITE,
                     UC_HOOK_MEM_READ_UNMAPPED, UC_HOOK_MEM_WRITE_UNMAPPED)
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_EDI, UC_X86_REG_EIP, UC_X86_REG_EBP, UC_X86_REG_ESI,
    UC_X86_REG_ESP, UC_X86_REG_FPCW,
)

SOURCE_SHA256 = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
DRAW = 0x46B1D0
OBJECT_MATRIX = 0x45D1E0
LOCAL_MATRIX = 0x45D1E0
LOCAL_BUILD = -1  # marker for a captured 0x45d1e0 output
SCALED_MATRIX = 0x45D2F0
SCRATCH = 0xC22B80
SCRATCH_LIMIT = 0xC22D00
VIEW = 0x8D1340
COUNTER_GLOBAL = 0xAD3CF4
MESH_STRIDE = 0x70
ELEMENT_STRIDE = 0x0C
OBJECT_WRITE_TARGETS = {0x30: 'object_z', 0x33: 'object_phase', 0x34: 'object_direction'}
# Every matrix callback slot and localized flag is compared.

STACK, STOP, INSTANCE, OBJECT, MESHES, ELEMENTS, RESULT = (
    0x2000000, 0x3000000, 0x4000000, 0x4100000, 0x4200000, 0x4300000, 0x4400000)


class Original:
    def __init__(self, image_bytes):
        pe = pefile.PE(data=image_bytes)
        base = pe.OPTIONAL_HEADER.ImageBase
        self.machine = Uc(UC_ARCH_X86, UC_MODE_32)
        self.machine.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 0xFFF) & ~0xFFF)
        self.machine.mem_write(base, pe.get_memory_mapped_image())
        pe.close()
        for address, size in ((STACK, 0x10000), (STOP, 0x1000), (INSTANCE, 0x10000),
                              (OBJECT, 0x10000), (MESHES, 0x10000), (ELEMENTS, 0x10000),
                              (RESULT, 0x1000)):
            self.machine.mem_map(address, size)

    # -- memory helpers -------------------------------------------------
    def read_matrix(self, address):
        return struct.unpack('<12i', self.machine.mem_read(address, 48))

    def scratch_slots(self):
        raw = bytes(self.machine.mem_read(SCRATCH, SCRATCH_LIMIT - SCRATCH))
        slots = []
        for index in range((SCRATCH_LIMIT - SCRATCH) // 0x40):
            values = struct.unpack_from('<12i', raw, index * 0x40)
            flag = struct.unpack_from('<i', raw, index * 0x40 + 0x3C)[0]
            slots.append((index, flag, values))
        return slots

    def setup_view(self, matrix):
        self.machine.mem_write(VIEW, struct.pack('<12i', *matrix))

    def setup_blocks(self, instance, object_fields, elements):
        raw = bytearray(0x200)
        struct.pack_into('<i', raw, 0x98, instance['mesh_count'])
        struct.pack_into('<I', raw, 0x9c, MESHES)
        struct.pack_into('<I', raw, 0xa4, ELEMENTS)
        struct.pack_into('<i', raw, 0x58, instance['divisor_58'])
        struct.pack_into('<i', raw, 0x5a, instance['flag_5a'])
        struct.pack_into('<i', raw, 0x40, instance['threshold_40'])
        self.machine.mem_write(INSTANCE, bytes(raw))
        raw = bytearray(0x200)
        struct.pack_into('<6i', raw, 0x24, *object_fields['placement'])
        struct.pack_into('<i', raw, 0x7c, object_fields['scale_7c'])
        struct.pack_into('<i', raw, 0x94, object_fields['value_94'])
        struct.pack_into('<i', raw, 0x98, object_fields['value_98'])
        struct.pack_into('<i', raw, 0x9c, object_fields['angle_9c'])
        struct.pack_into('<i', raw, 0xa0, object_fields['value_a0'])
        struct.pack_into('<i', raw, 0xa4, object_fields['angle_a4'])
        struct.pack_into('<i', raw, 0xc4, object_fields['angle_c4'])
        struct.pack_into('<i', raw, 0xc8, object_fields['angle_c8'])
        struct.pack_into('<i', raw, 0xe0, object_fields['value_e0'])
        struct.pack_into('<i', raw, 0xe4, object_fields['angle_e4'])
        struct.pack_into('<i', raw, 0xe8, object_fields['angle_e8'])
        struct.pack_into('<i', raw, 0xec, object_fields['phase_ec'])
        struct.pack_into('<i', raw, 0xf0, object_fields['mode_f0'])
        struct.pack_into('<i', raw, 0xfc, object_fields['distance_fc'])
        struct.pack_into("<i",raw,0x3c,object_fields["flag_3c"])
        self.machine.mem_write(OBJECT, bytes(raw))
        mesh_bytes = bytearray(MESH_STRIDE * 16)
        for index in range(16):
            struct.pack_into('<i', mesh_bytes, index * MESH_STRIDE, index)
        self.machine.mem_write(MESHES, bytes(mesh_bytes))
        element_bytes = bytearray(ELEMENT_STRIDE * 8)
        for index, element in enumerate(elements):
            struct.pack_into('<3i', element_bytes, index * ELEMENT_STRIDE, *element)
        self.machine.mem_write(ELEMENTS, bytes(element_bytes))

    # -- original calls -------------------------------------------------
    def call(self, entry, stop, args):
        machine = self.machine
        sp = STACK + 0x8000
        payload = struct.pack('<I', stop) + b''.join(struct.pack('<I', value) for value in args)
        machine.mem_write(sp, payload)
        machine.reg_write(UC_X86_REG_ESP, sp)
        machine.reg_write(UC_X86_REG_FPCW, 0x037f)
        machine.emu_start(entry, stop, timeout=10_000_000, count=20_000_000)
        if machine.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError(f'original routine {entry:#x} did not return')
        return machine.reg_read(UC_X86_REG_EAX)

    def base_matrix(self, scaled, scale=0):
        if scaled:
            self.call(SCALED_MATRIX, STOP, [SCRATCH, OBJECT + 0x24, scale])
        else:
            self.call(OBJECT_MATRIX, STOP, [SCRATCH, OBJECT + 0x24])
        return self.read_matrix(SCRATCH)

    def matrix_callback(self, entry, args):
        machine = self.machine
        machine.mem_write(SCRATCH, bytes(SCRATCH_LIMIT - SCRATCH))
        written = set()
        flags = set()

        def on_write(uc, access, address, size, value, user):
            if SCRATCH <= address < SCRATCH_LIMIT:
                offset = address - SCRATCH
                slot = offset // 0x40
                within = offset % 0x40
                if within == 0x3C:
                    if value != 0:
                        flags.add(slot)
                elif within < 0x30:
                    written.add(slot)

        handle = machine.hook_add(UC_HOOK_MEM_WRITE, on_write)
        try:
            result = self.call(entry, STOP, args)
        finally:
            machine.hook_del(handle)
        if result == 0:
            return None
        slots = {index: matrix for index, _, matrix in self.scratch_slots()}
        return {'written': [(index, slots[index]) for index in sorted(written)],
                'flags': sorted(flags)}

    def capture_callback(self, entry, args, watch=0x45CB40):
        """Run a callback and capture every `watch` (a, b, out) and its result."""
        machine = self.machine
        captures = []
        handles = []

        def entry_hook(uc, address, size, user):
            sp = uc.reg_read(UC_X86_REG_ESP)
            first, second, out = struct.unpack('<3I', uc.mem_read(sp + 4, 12))
            ret = struct.unpack('<I', uc.mem_read(sp, 4))[0]
            # 0x45cb40 writes in place (out == second for the cameras), so the
            # input matrices must be snapshotted before the multiply runs.
            first_values = struct.unpack('<12i', uc.mem_read(first, 48))
            second_values = struct.unpack('<12i', uc.mem_read(second, 48))
            # Only direct calls from the callback count; helpers such as
            # 0x45d1e0 run their own multiplies inside.
            if not (entry <= ret < entry + 0x400):
                return

            def ret_hook(uc2, address2, size2, user2):
                values = struct.unpack('<12i', uc2.mem_read(out, 48))
                captures.append((first, second, out, values, first_values,
                                 second_values))

            handles.append(machine.hook_add(UC_HOOK_CODE, ret_hook, begin=ret, end=ret))

        def local_hook(uc, address, size, user):
            sp = uc.reg_read(UC_X86_REG_ESP)
            out = struct.unpack('<I', uc.mem_read(sp + 4, 4))[0]
            ret = struct.unpack('<I', uc.mem_read(sp, 4))[0]
            if not (entry <= ret < entry + 0x400):
                return

            def ret_hook(uc2, address2, size2, user2):
                values = struct.unpack('<12i', uc2.mem_read(out, 48))
                captures.append((LOCAL_BUILD, out, out, values, values, values))

            handles.append(machine.hook_add(UC_HOOK_CODE, ret_hook, begin=ret, end=ret))

        entry_handle = machine.hook_add(UC_HOOK_CODE, entry_hook, begin=watch, end=watch)
        local_handle = machine.hook_add(UC_HOOK_CODE, local_hook,
                                        begin=LOCAL_MATRIX, end=LOCAL_MATRIX)
        try:
            result = self.call(entry, STOP, args)
        finally:
            machine.hook_del(entry_handle)
            machine.hook_del(local_handle)
            for handle in handles:
                machine.hook_del(handle)
        return result, captures

    def draw_callback(self, entry):
        machine = self.machine
        draws = []
        counter_writes = []
        object_writes = []

        def draw_stub(uc, address, size, user):
            sp = uc.reg_read(UC_X86_REG_ESP)
            mesh, matrix, second, mode = struct.unpack('<4I', uc.mem_read(sp + 4, 16))
            values = struct.unpack('<12i', uc.mem_read(matrix, 48))
            draws.append((mesh, values))
            callback = struct.unpack('<I', uc.mem_read(sp, 4))[0]
            uc.reg_write(UC_X86_REG_ESP, sp + 4)
            uc.reg_write(UC_X86_REG_EAX, 0)
            uc.reg_write(UC_X86_REG_EIP, callback)

        def on_write(uc, access, address, size, value, user):
            if address == COUNTER_GLOBAL:
                counter_writes.append(struct.unpack('<i', struct.pack('<I', value))[0])
            if OBJECT <= address < OBJECT + 0x200 and size == 4:
                object_writes.append((address - OBJECT, value & 0xFFFFFFFF))
            if OBJECT <= address < OBJECT + 0x200 and size == 1:
                object_writes.append((address - OBJECT, value & 0xFF))

        draw_handle = machine.hook_add(UC_HOOK_CODE, draw_stub, begin=DRAW, end=DRAW)
        write_handle = machine.hook_add(UC_HOOK_MEM_WRITE, on_write)
        try:
            self.call(entry, STOP, [INSTANCE, 0x1234, OBJECT])
        finally:
            machine.hook_del(draw_handle)
            machine.hook_del(write_handle)
        return draws, counter_writes, object_writes


def probe_line(tag, instance, object_fields, elements, object_matrix, view, z_high,
               slot_matrix=None):
    return (f"draw {tag} {instance['mesh_count']} {object_fields['value_98']} "
            f"{object_fields['value_a0']} {object_fields['value_e0']} "
            f"{object_fields['value_94']} {object_fields['angle_9c']} "
            f"{object_fields['angle_a4']} {object_fields['angle_c4']} "
            f"{object_fields['angle_c8']} {z_high} {object_fields['placement'][3]} "
            f"{object_fields['phase_ec']} {object_fields['angle_e4']} "
            f"{object_fields['angle_e8']} {object_fields['mode_f0']} "
            f"{(object_fields['placement'][3] >> 24) & 255} {object_fields['flag_3c']} {object_fields['z_low']} 1048576 "
            f"{instance['flag_5a']} {instance['divisor_58']} {instance['threshold_40']} "
            f"{object_fields['distance_fc']} "
            + ''.join(f'{word & 0xFFFFFFFF:08x}' for word in object_matrix)
            + ' ' + ''.join(f'{word & 0xFFFFFFFF:08x}'
                            for word in (slot_matrix or object_matrix))
            + ' ' + ''.join(f'{word & 0xFFFFFFFF:08x}' for word in view)
            + ' ' + ''.join(f'{value & 0xFFFFFFFF:08x}' for element in elements for value in element))


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
        pe=pefile.PE(data=image_bytes)
        raw_table=pe.get_data(0x500c1c-pe.OPTIONAL_HEADER.ImageBase,16*0x14)
        rows=[]
        for offset in range(0,len(raw_table),0x14):
            key,draw,matrix=struct.unpack_from('<3I',raw_table,offset)
            rows.append(dict(tag=key.to_bytes(4,'little').decode('ascii'),draw=hex(draw),matrix=hex(matrix)))
        pe.close()
        original = Original(image_bytes)
        table = subprocess.run([str(args.probe.resolve())], input='table\n',
                               capture_output=True, text=True, check=True)
        original.call(0x477510, STOP, [])
        table_values=tuple(int(value) for value in table.stdout.split())
        original_values=struct.unpack('<2561i',original.machine.mem_read(0xbd0068,2561*4))
        if table_values != original_values:
            raise ValueError('Original and native sine tables differ')
        # A real rotation in the view matrix so pivots are exercised.
        view = (0x400000, 0, 0, 0x2000, 0, 0x400000, 0, 0x4000, 0, 0, 0x400000, 0x6000)
        original.setup_view(view)

        cases = []
        for row in rows:
            if row['tag'] in ('0gro', 'crng'):
                continue
            cases.extend(build_cases(row['tag']))

        lines = []
        expected = []
        for tag, instance, object_fields, elements in cases:
            draw_address = int(next(r['draw'] for r in rows if r['tag'] == tag), 16)
            matrix_address = int(next(r['matrix'] for r in rows if r['tag'] == tag), 16)
            # The object matrix is the local input of the callback's first
            # 0x45cb40(view, local) multiply. The callback applies its own
            # temporary object tweaks (phase byte, raised angle, peng lean).
            original.setup_blocks(instance, object_fields, elements)
            _, captures = original.capture_callback(draw_address, [INSTANCE, 0x1234, OBJECT])
            view_calls = [item for item in captures if item[0] == VIEW]
            if view_calls:
                object_matrix = view_calls[0][5]
            else:
                # llun draws nothing: the plan carries no steps.
                object_matrix = original.base_matrix(False)
            original.setup_blocks(instance, object_fields, elements)
            draws, counters, object_writes = original.draw_callback(draw_address)
            original.setup_blocks(instance, object_fields, elements)
            _, slot_captures = original.capture_callback(
                matrix_address, [INSTANCE, OBJECT, 0x00100000])
            local_builds = [item for item in slot_captures if item[0] == LOCAL_BUILD]
            if local_builds:
                slot_base = local_builds[0][3]
            else:
                slot_base = slot_captures[0][5] if slot_captures else object_matrix
            # Each matrix callback starts from the unscaled object placement.
            # ecaf uses the third callback argument as its temporary yaw.
            original.setup_blocks(instance,object_fields,elements)
            if tag == "ecaf":
                original.machine.mem_write(OBJECT+0x30,struct.pack('<I',0x800fffff))
            slot_base=original.base_matrix(False)
            original.setup_blocks(instance,object_fields,elements)
            slots = original.matrix_callback(matrix_address,
                                             [INSTANCE, OBJECT, 0x00100000])
            lines.append(probe_line(tag, instance, object_fields, elements,
                                    object_matrix, view, 0, slot_base))
            expected.append({
                'tag': tag,
                'draws': draws,
                'slots': slots,
                'counters': counters,
                'object_writes': object_writes,
            })

        result = subprocess.run([str(args.probe.resolve())],
                                input=''.join(line + '\n' for line in lines),
                                capture_output=True, text=True, check=True)
        replies = result.stdout.splitlines()
        mismatches = compare(replies, expected)
        missing = required_lines(expected)
        if len(replies) != missing:
            mismatches.append({'kind': 'reply count', 'want': missing, 'got': len(replies)})
        print(json.dumps({
            'source_sha256': digest,
            'unicorn_version': unicorn.__version__,
            'tags': sorted({case['tag'] for case in expected}),
            'cases': len(expected),
            'sine_table_entries': 2561,
            'draw_calls': sum(len(case['draws']) for case in expected),
            'slot_writes': sum(len(case['slots']['written']) if case['slots'] else 0
                               for case in expected),
            'write_cases': sum(len(case['object_writes']) + len(case['counters'])
                               for case in expected),
            'mismatch_count': len(mismatches),
            'mismatches': mismatches,
            'scope': 'Data emulation only; the mesh draw is stubbed',
        }, indent=2))
        return int(bool(mismatches))
    except (OSError, ValueError, RuntimeError, pefile.PEFormatError,
            unicorn.UcError, subprocess.CalledProcessError, KeyError,
            struct.error) as error:
        print(f'check_model_specials_original: {error}', file=sys.stderr)
        return 1


def build_cases(tag):
    """Input variants per tag: mesh counts, selectors and phases."""
    instance_base = {'divisor_58': 0x10000, 'flag_5a': 1, 'threshold_40': 0x10000000}
    object_base = {
        'placement': (0x1000, 0x2000, 0x3000, 0x100000, 0x200000, 0x300000),
        'scale_7c': 0x180000, 'value_94': 0x400000, 'value_98': 0x08000000,
        'angle_9c': 0x00200000, 'value_a0': 0x10000000, 'angle_a4': 0x00400000,
        'angle_c4': 0x00100000, 'angle_c8': 0x00300000, 'value_e0': 2,
        'angle_e4': 0x00500000, 'angle_e8': 0x00700000, 'phase_ec': 0x00600000,
        'mode_f0': 0, 'distance_fc': 0x2000, 'phase_33': 0x40, 'flag_3c': 0,
        'z_low': 0,
    }
    elements = [(0x100, 0x200, 0x300), (0x400, 0x500, 0x600), (0x700, 0x800, 0x900),
                (0xA00, 0xB00, 0xC00), (0xD00, 0xE00, 0xF00)]
    cases = []
    counts = (1, 2, 3, 4, 6)
    for count in counts:
        instance = dict(instance_base)
        instance['mesh_count'] = count
        cases.append((tag, instance, dict(object_base), elements))
    if tag == 'mlap':
        for selector in (0, 1, 2, 3, 0x0F, 0x10, 0x11, 0x12, 0x1F, 0x20, 0x21):
            instance = dict(instance_base)
            instance['mesh_count'] = 6
            fields = dict(object_base)
            fields['value_e0'] = selector
            cases.append((tag, instance, fields, elements))
    if tag == 'oleh':
        for value, a0 in ((0x08000000, 0x08000000), (0x10000000, 0x10000000),
                          (0x0BFFFFFF, 0x0C000000)):
            instance = dict(instance_base)
            instance['mesh_count'] = 6
            fields = dict(object_base)
            fields['value_98'] = value
            fields['value_a0'] = a0
            cases.append((tag, instance, fields, elements))
    if tag == 'hsif':
        for phase in (0x00000000, 0x00800000, 0x00880000, 0x00900000, 0x00A00000,
                      0x00C00000, 0x01000000, 0x01800000, 0x01FF0000, 0x00600000):
            instance = dict(instance_base)
            instance['mesh_count'] = 6
            fields = dict(object_base)
            fields['phase_ec'] = phase
            cases.append((tag, instance, fields, elements))
    if tag == 'gnep':
        for value_e0, e4, e8, mode in ((0x00000000, 0x00000000, 0x00000000, 0),
                                       (0x01000000, 0x01800000, 0x01000000, 0),
                                       (0x00800000, 0x00500000, 0x01800000, 2),
                                       (0x01FF0000, 0x01FF0000, 0x01FF0000, 0),
                                       (0x00300000, 0x00200000, 0x00100000, 2),
                                       (0x00C00000, 0x00C00000, 0x00C00000, 1)):
            instance = dict(instance_base)
            instance['mesh_count'] = 6
            fields = dict(object_base)
            fields['value_e0'] = value_e0
            fields['angle_e4'] = e4
            fields['angle_e8'] = e8
            fields['mode_f0'] = mode
            cases.append((tag, instance, fields, elements))
    if tag == 'eert':
        for z_high, z_30 in ((0x00000000, 0x3000), (0x00A00000, 0x00100000)):
            instance = dict(instance_base)
            instance['mesh_count'] = 6
            fields = dict(object_base)
            fields['placement'] = (0x1000, 0x2000, z_30, 0x100000, 0x200000, 0x300000)
            cases.append((tag, instance, fields, elements))
    return cases


def signed32(value):
    return value - 0x100000000 if value >= 0x80000000 else value


def read32(data, at):
    return struct.unpack_from('<I', data, at)[0]


def required_lines(expected):
    total = 0
    for case in expected:
        total += 1 + len(case['draws'])
        total += 1 + (len(case['slots']['written']) if case['slots'] else 0)
        total += 1  # one localized-flag line
        total += 1 + len(case['object_writes']) + len(case['counters'])
    return total


def compare(replies, expected):
    mismatches = []
    line = 0

    def take():
        nonlocal line
        value = replies[line]
        line += 1
        return value

    for case in expected:
        header = take().split()
        steps = int(header[0])
        if steps != len(case['draws']):
            mismatches.append({'tag': case['tag'], 'kind': 'step count',
                               'want': len(case['draws']), 'got': steps})
            line += steps
            continue
        for index, (mesh, matrix) in enumerate(case['draws']):
            step = take().split()
            got_mesh = int(step[0]) - 0x40000000
            want_mesh = mesh - MESHES
            if got_mesh != want_mesh:
                mismatches.append({'tag': case['tag'], 'step': index, 'kind': 'mesh',
                                   'want': hex(want_mesh), 'got': hex(got_mesh)})
                continue
            got_matrix = tuple(signed32(int(word, 16)) for word in step[1:13])
            if got_matrix != matrix:
                mismatches.append({'tag': case['tag'], 'step': index, 'kind': 'matrix',
                                   'want': matrix, 'got': got_matrix})
        slot_count = int(take())
        original_slots = case['slots']
        written = original_slots['written'] if original_slots else []
        if slot_count != len(written):
            mismatches.append({'tag': case['tag'], 'kind': 'slot count',
                               'want': len(written), 'got': slot_count})
        for index in range(slot_count):
            slot_line = take().split()
            got_index = int(slot_line[0])
            got_matrix = tuple(signed32(int(word, 16)) for word in slot_line[1:13])
            if index >= len(written):
                continue
            want_index, want_matrix = written[index]
            if got_index != want_index:
                mismatches.append({'tag': case['tag'], 'slot': want_index,
                                   'kind': 'slot index',
                                   'want': want_index, 'got': got_index})
            elif got_matrix != want_matrix:
                mismatches.append({'tag': case['tag'], 'slot': want_index,
                                   'kind': 'slot matrix',
                                   'want': want_matrix, 'got': got_matrix})
        flag_line = take().split()
        got_flags = sorted(int(value) for value in flag_line[1:])
        want_flags = original_slots['flags'] if original_slots else []
        if got_flags != want_flags:
            mismatches.append({'tag': case['tag'], 'kind': 'localized flags',
                               'want': want_flags, 'got': got_flags})
        write_count = int(take())
        wanted = []
        for offset, value in case['object_writes']:
            wanted.append(({0x30:0,0x34:1,0x33:2}[offset], signed32(value)))
        for value in case['counters']:
            wanted.append((3, signed32(value)))
        got_writes = []
        for _ in range(write_count):
            value = take().split()
            got_writes.append((int(value[0]), int(value[1])))
        if got_writes != wanted:
            mismatches.append({'tag': case['tag'], 'kind': 'ordered writes',
                               'want': wanted, 'got': got_writes})
    return mismatches


if __name__ == '__main__':
    sys.exit(main())
