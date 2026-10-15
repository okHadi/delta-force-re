#!/usr/bin/env python3
"""Replay original mesh-instance code with Unicorn and compare the native probe.

Original Df.exe functions covered (sha256
abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff):

  0x45d1e0  mesh-instance local matrix from object +0x24      (probe mode instance)
  0x45d2f0  local matrix with a uniform scale base            (probe mode scaled)
  0x45cb40  matrix product, 22-bit shifts and 32-bit wrap     (probe mode multiply)
  0x480ff0  crng matrix callback slots, no camera product     (probe mode callback)
  0x481060  crng draw callback: frame counter and camera      (probe mode counter, generic)
  0x46b830  static-object draw decision                       (probe mode static)

0x46b1d0 is redirected to a one-byte `ret` at 0x6000000 (the five call sites
inside 0x46b830/0x481060 are patched in emulator memory only). A code hook
records the four stack arguments of every stubbed call plus the frame global
0xad3cf4 at that moment. All other executed code is original image code.

Strict checks per case: the call must return to the trap, untouched matrix
buffer bytes must keep their fill pattern, 0x481060 must clear 0xad3cf4 after
its mesh loop, and every draw record is compared (mesh pointer, matrix
pointer, clip mask, level). Null-record dereferences and the zero-frame-count
divide are asserted as original faults against the native exception.

0x45d2f0 scaled cases use the same object fields plus the scale argument.
The stock fixtures (--models) are real v5 3DI files: every LOD of every file
is replayed for 0x480ff0 and 0x481060, every file for 0x46b830.

This is deterministic static replay. It does not run the game.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import random
import struct
import subprocess

import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
EXPECTED_HASH = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
STUB = 0x6000000
RETURN_TRAP = 0x6001000
SCRATCH = 0x2000000
STACK = 0x200F000

VIEW_MATRIX = 0x8D1340
TIME_GLOBAL = 0x6E471C
FRAME_GLOBAL = 0xAD3CF4
CALLBACK_BUFFER = 0xC22B80
FRAME_SENTINEL = 0x11223344

FIELDS = 0x2100000
OUT = 0x2101000
OBJECT = 0x2102000
MODEL_PRIMARY = 0x2103000
MODEL_SECONDARY = 0x2103800
CONTAINER = 0x2104000
CONTAINER_STRIDE = 0x100
MESH_ARRAY = 0x2110000
MESH_ARRAY_STRIDE = 0x1000
MATRIX_LEFT = 0x2120000
MATRIX_RIGHT = 0x2121000
MATRIX_OUT = 0x2122000

# 0x46b830 stack: entry esp is STACK - 16, then four pushes and `sub esp, 0x800`.
STATIC_BUFFER = STACK - 0x10 - 0x10 - 0x800
# 0x481060 stack: entry esp is STACK - 16, then four pushes and `sub esp, 0x40`.
GENERIC_BUFFER = STACK - 0x10 - 0x10 - 0x40
PATTERN = bytes((0xA5 ^ (index * 37)) & 0xFF for index in range(0x900))
MESH_ENTRY = 0x70
PATTERN_TAIL = PATTERN[48:64]
STUB_SITES = (0x46B906, 0x46B96D, 0x46B9D4, 0x46BA3F, 0x4810EA)
# The exact instructions where the original faults: the frame count divide of
# 0x481060 and the null-record count/array reads of 0x46b830. This Unicorn build
# stops reporting faults after two exceptions in one instance, so both an
# UcError and a silent stop are accepted only at these addresses.
GENERIC_FAULT_EIPS = {0x48109C}
STATIC_FAULT_EIPS = {0x46B8BB, 0x46B922, 0x46B989, 0x46B9F4,
                     0x46B8E5, 0x46B94C, 0x46B9B3, 0x46BA1E,
                     0x46B8F8, 0x46B95F, 0x46B9C6, 0x46BA31}
FUNCTIONS = (
    (0x45D1E0, 0x91),
    (0x45D2F0, 0x9A),
    (0x45CB40, 0x259),
    (0x480FF0, 0x68),
    (0x481060, 0xAF),
    (0x46B830, 0x218),
)


def i32(value):
    value &= 0xFFFFFFFF
    return value - 0x100000000 if value >= 0x80000000 else value


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def read_i32(data, offset):
    return i32(u32(data, offset))


def parse_generic_model(path):
    """Thresholds and per-LOD runtime-record fields of one v5 3DI file."""
    data = Path(path).read_bytes()
    if data[:4] != b'3DI\x05':
        return None
    texture_count = u32(data, 124)
    position = 128
    for _ in range(texture_count):
        if position + 36 > len(data):
            raise ValueError(f'{path}: texture header outside file')
        size = u32(data, position + 16)
        position += 36 + size + 1024
    lod_count = u32(data, 20)
    thresholds = [read_i32(data, 24 + pad * 4) for pad in range(4)]
    tag = data[40:44]
    lods = []
    for _ in range(lod_count):
        vertex_count = u32(data, position + 128)
        normal_count = u32(data, position + 136)
        face_count = u32(data, position + 144)
        mesh_count = u32(data, position + 152)
        hardpoint_count = u32(data, position + 160)
        material_count = u32(data, position + 168)
        collision_count = u32(data, position + 176)
        total = (vertex_count * 8 + normal_count * 8 + face_count * 72 + mesh_count * 112 +
                 hardpoint_count * 12 + collision_count * 72 + material_count * 76)
        if total != u32(data, position + 20):
            raise ValueError(f'{path}: LOD size mismatch while building stock cases')
        lods.append(dict(mesh_count=mesh_count,
                         frame_count_raw=read_i32(data, position + 0x40),
                         frame_duration_raw=read_i32(data, position + 0x58)))
        position += 184 + total
    return dict(name=Path(path).name, tag=tag, thresholds=thresholds, lods=lods)


def va_to_file_offset(source, address):
    pe = pefile.PE(data=source)
    delta = address - pe.OPTIONAL_HEADER.ImageBase
    for section in pe.sections:
        size = max(section.Misc_VirtualSize, section.SizeOfRawData)
        if section.VirtualAddress <= delta < section.VirtualAddress + size:
            return section.PointerToRawData + (delta - section.VirtualAddress)
    raise ValueError(f'no section maps 0x{address:x}')


class OriginalStopped(Exception):
    def __init__(self, address, eip):
        super().__init__(f'original function 0x{address:x} stopped at 0x{eip:x}')
        self.address = address
        self.eip = eip


class Original:
    def __init__(self, executable):
        source = Path(executable).read_bytes()
        digest = hashlib.sha256(source).hexdigest()
        if digest != EXPECTED_HASH:
            raise ValueError('unsupported original executable hash')
        self.digest = digest
        self.unicorn = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        u = self.unicorn
        u.mem_map(0x400000, 0x1000000)
        u.mem_write(0x400000, pefile.PE(data=source).get_memory_mapped_image())
        u.mem_map(SCRATCH, 0x400000)
        u.mem_map(0x6000000, 0x10000)
        u.mem_write(STUB, b'\xc3')
        # Redirect 0x46b1d0 to the ret stub, in this emulator only.
        for site in STUB_SITES:
            code = bytearray(u.mem_read(site, 5))
            if code[0] != 0xE8:
                raise ValueError(f'stub site 0x{site:x} is not a relative call')
            target = site + 5 + struct.unpack_from('<i', bytes(code[1:]))[0]
            if target != 0x46B1D0:
                raise ValueError(f'stub site 0x{site:x} does not call 0x46b1d0')
            struct.pack_into('<i', code, 1, STUB - (site + 5))
            u.mem_write(site, bytes(code))
        self.calls = []
        u.hook_add(unicorn.UC_HOOK_CODE, self._record_call, None, STUB, STUB + 1)
        # Build the original sine/cosine tables (0x477510) as startup does.
        self.call(0x477510, [])
        table = u.mem_read(0xBD0068, 2561 * 4)
        for index in (0, 1, 512, 1024, 2560):
            expected = int(math.sin(index * 0.00306796162109375) * 4194304.0)
            if read_i32(table, index * 4) != expected:
                raise ValueError('original sine table missing after 0x477510')

    def _record_call(self, uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        mesh, matrices, mask, level = struct.unpack('<4I', uc.mem_read(esp + 4, 16))
        self.calls.append(dict(mesh=mesh, matrices=matrices, mask=mask, level=level,
                               frame=struct.unpack('<I', uc.mem_read(FRAME_GLOBAL, 4))[0]))

    def write(self, address, data):
        self.unicorn.mem_write(address, data)

    def read(self, address, size):
        return self.unicorn.mem_read(address, size)

    def read_words(self, address, count=12):
        return list(struct.unpack(f'<{count}i', self.read(address, count * 4)))

    def call(self, address, args):
        u = self.unicorn
        esp = STACK - 16
        u.mem_write(esp, struct.pack('<I', RETURN_TRAP))
        for index, arg in enumerate(args):
            u.mem_write(esp + 4 + index * 4, struct.pack('<I', arg & 0xFFFFFFFF))
        u.reg_write(UC_X86_REG_ESP, esp)
        u.reg_write(UC_X86_REG_FPCW, 0x037F)
        u.emu_start(address, RETURN_TRAP, count=4000000)
        eip = u.reg_read(UC_X86_REG_EIP)
        if eip != RETURN_TRAP:
            raise OriginalStopped(address, eip)

    def call_fault(self, address, args):
        """None on a normal return, otherwise the fault description."""
        try:
            self.call(address, args)
            return None
        except unicorn.UcError:
            return dict(kind='uc', eip=self.unicorn.reg_read(UC_X86_REG_EIP))
        except OriginalStopped as stopped:
            return dict(kind='stop', eip=stopped.eip)

    def write_view(self, view):
        self.write(VIEW_MATRIX, struct.pack('<12i', *view))

    def write_fields(self, fields):
        translation, angles = fields
        self.write(FIELDS, struct.pack('<3i3I', *translation, *angles))

    def write_object(self, fields, primary, secondary, use_secondary):
        record = bytearray(0x100)
        struct.pack_into('<I', record, 0x10, primary)
        struct.pack_into('<I', record, 0x14, secondary)
        record[0x6C] = 4 if use_secondary else 0
        translation, angles = fields
        struct.pack_into('<3i3I', record, 0x24, *translation, *angles)
        self.write(OBJECT, bytes(record))

    def write_model(self, address, thresholds, containers):
        self.write(address, b'\0' * 0x200)
        self.write(address + 0x18, struct.pack('<4i', *thresholds))
        for index, container in enumerate(containers):
            self.write(address + 0x50 + index * 4, struct.pack('<I', container))

    def write_container(self, index, mesh_count, frame_count_raw, frame_duration_raw):
        address = CONTAINER + index * CONTAINER_STRIDE
        record = bytearray(CONTAINER_STRIDE)
        struct.pack_into('<I', record, 0x40, frame_count_raw & 0xFFFFFFFF)
        struct.pack_into('<I', record, 0x58, frame_duration_raw & 0xFFFFFFFF)
        struct.pack_into('<I', record, 0x98, mesh_count & 0xFFFFFFFF)
        struct.pack_into('<I', record, 0x9C, MESH_ARRAY + index * MESH_ARRAY_STRIDE)
        self.write(address, bytes(record))
        self.write(MESH_ARRAY + index * MESH_ARRAY_STRIDE, bytes(MESH_ENTRY * max(mesh_count, 1)))
        return MESH_ARRAY + index * MESH_ARRAY_STRIDE


def probe_run(probe, mode, payload):
    result = subprocess.run([str(probe), mode], input=payload.encode(), capture_output=True)
    if result.returncode != 0:
        raise ValueError(f'probe mode {mode} failed: {result.stderr.decode()}')
    return result.stdout.decode().splitlines()


def load_stock(directory):
    if directory is None:
        return []
    return [row for row in (parse_generic_model(path) for path in sorted(directory.glob('*.3DI')))
            if row is not None]


def check_instance(original, probe, rng, count):
    cases = []
    for number in range(count):
        if number % 6 == 0:
            translation = [rng.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x400000]) for _ in range(3)]
        else:
            translation = [rng.randrange(-0x4000000, 0x4000001) for _ in range(3)]
        angles = []
        for _ in range(3):
            pick = rng.randrange(4)
            if pick == 0:
                angles.append(0)
            elif pick == 1:
                angles.append(rng.randrange(0, 2048) << 21)
            else:
                angles.append(rng.randrange(0, 0x100000000))
        view = [rng.randrange(-0x800000, 0x800001) for _ in range(12)]
        cases.append((translation, angles, view))
    payload = '\n'.join(' '.join(map(str, translation + angles + view)) for translation, angles, view in cases)
    native = probe_run(probe, 'instance', payload)
    if len(native) != 2 * count:
        raise ValueError('native instance line count differs')
    for number, (translation, angles, view) in enumerate(cases):
        original.write_fields((translation, angles))
        original.write(OUT, PATTERN[:64])
        original.call(0x45D1E0, [OUT, FIELDS])
        local = original.read_words(OUT)
        if original.read(OUT + 48, 16) != PATTERN_TAIL:
            raise ValueError(f'0x45d1e0 wrote past 12 words at case {number}')
        if local != [i32(value) for value in map(int, native[2 * number].split())]:
            raise ValueError(f'mesh-instance local matrix mismatch at case {number}')
        original.write_view(view)
        original.write(OUT + 64, PATTERN[:64])
        original.call(0x45CB40, [VIEW_MATRIX, OUT, OUT + 64])
        camera = original.read_words(OUT + 64)
        if original.read(OUT + 112, 16) != PATTERN_TAIL:
            raise ValueError(f'0x45cb40 wrote past 12 words at case {number}')
        if camera != [i32(value) for value in map(int, native[2 * number + 1].split())]:
            raise ValueError(f'mesh-instance camera matrix mismatch at case {number}')
    return dict(cases=count, matrices=2 * count, components=24 * count)


def check_multiply(original, probe, rng, count):
    cases = []
    for number in range(count):
        if number % 5 == 0:
            left = [rng.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x400000]) for _ in range(12)]
            right = [rng.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, 0x400000]) for _ in range(12)]
        else:
            left = [rng.randrange(-0x800000, 0x800001) for _ in range(12)]
            right = [rng.randrange(-0x800000, 0x800001) for _ in range(12)]
        cases.append((left, right))
    payload = '\n'.join(' '.join(map(str, left + right)) for left, right in cases)
    native = probe_run(probe, 'multiply', payload)
    if len(native) != count:
        raise ValueError('native multiply line count differs')
    for number, (left, right) in enumerate(cases):
        original.write(MATRIX_LEFT, struct.pack('<12i', *left))
        original.write(MATRIX_RIGHT, struct.pack('<12i', *right))
        original.write(MATRIX_OUT, PATTERN[:64])
        original.call(0x45CB40, [MATRIX_LEFT, MATRIX_RIGHT, MATRIX_OUT])
        product = original.read_words(MATRIX_OUT)
        if original.read(MATRIX_OUT + 48, 16) != PATTERN_TAIL:
            raise ValueError(f'0x45cb40 wrote past 12 words at case {number}')
        if product != [i32(value) for value in map(int, native[number].split())]:
            raise ValueError(f'matrix product mismatch at case {number}')
    return dict(cases=count, components=12 * count)


def check_scaled(original, probe, rng, count):
    cases = []
    for number in range(count):
        translation = [rng.randrange(-0x4000000, 0x4000001) for _ in range(3)]
        if number % 4 == 0:
            translation = [rng.choice([0, 0x400000, -0x400000, 0x7FFFFFFF]) for _ in range(3)]
        angles = [rng.randrange(0, 0x100000000) if rng.randrange(4) else 0 for _ in range(3)]
        scale = (rng.choice([0x10000, 0x8000, 0x20000, 0xFFFF, 0x7FFFFFFF, -0x80000000])
                 if number % 4 == 0 else rng.randrange(-0x100000, 0x100001))
        cases.append((translation, angles, scale))
    payload = '\n'.join(' '.join(map(str, translation + angles + [scale]))
                        for translation, angles, scale in cases)
    native = probe_run(probe, 'scaled', payload)
    if len(native) != count:
        raise ValueError('native scaled line count differs')
    for number, (translation, angles, scale) in enumerate(cases):
        original.write_fields((translation, angles))
        original.write(OUT, PATTERN[:64])
        original.call(0x45D2F0, [OUT, FIELDS, scale])
        matrix = original.read_words(OUT)
        if original.read(OUT + 48, 16) != PATTERN_TAIL:
            raise ValueError(f'0x45d2f0 wrote past 12 words at case {number}')
        if matrix != [i32(value) for value in map(int, native[number].split())]:
            raise ValueError(f'scaled local matrix mismatch at case {number}')
    return dict(cases=count, components=12 * count)


def run_generic_case(original, mesh_count, frame_count_raw, frame_duration_raw, fields, view, mask, time):
    original.write_fields(fields)
    original.write_object(fields, MODEL_PRIMARY, 0, False)
    original.write_container(0, mesh_count, frame_count_raw, frame_duration_raw)
    original.write_view(view)
    original.write(TIME_GLOBAL, struct.pack('<I', time))
    original.write(FRAME_GLOBAL, struct.pack('<I', FRAME_SENTINEL))
    original.write(GENERIC_BUFFER, PATTERN[:0x40])
    original.calls = []
    fault = original.call_fault(0x481060, [CONTAINER, mask, OBJECT])
    calls = list(original.calls)
    cleared = struct.unpack('<I', original.read(FRAME_GLOBAL, 4))[0]
    return fault, calls, cleared


def check_generic(original, probe, rng, count):
    cases = []
    for number in range(count):
        pick = number % 10
        if pick == 0:
            frame_count_raw, frame_duration_raw = rng.randrange(1, 40) << 16, rng.randrange(1, 40) << 16
        elif pick == 1:
            frame_count_raw, frame_duration_raw = rng.randrange(1, 40) << 16, 0
        elif pick == 2:
            frame_count_raw, frame_duration_raw = 0, 0
        elif pick == 3:
            frame_count_raw, frame_duration_raw = 0, rng.randrange(1, 40) << 16  # divide fault
        elif pick == 4:
            frame_count_raw = (rng.randrange(1, 40) << 16) | rng.randrange(0x10000)
            frame_duration_raw = (rng.randrange(1, 40) << 16) | rng.randrange(0x10000)
        elif number % 2:
            frame_count_raw = rng.randrange(-0x8000, 0x8000) << 16
            frame_duration_raw = rng.randrange(-0x8000, 0x8000) << 16
        else:
            frame_count_raw = rng.randrange(-0x80000000, 0x80000000)
            frame_duration_raw = rng.randrange(-0x80000000, 0x80000000)
        mesh_count = rng.randrange(0, 19)
        mask = rng.choice([0, 2, 0x2E, 0x3E, 0x40])
        time = rng.choice([0, 1, 0x10000, rng.randrange(0, 0x100000000)])
        translation = [rng.randrange(-0x4000000, 0x4000001) for _ in range(3)]
        angles = [rng.randrange(0, 0x100000000) for _ in range(3)]
        view = [rng.randrange(-0x800000, 0x800001) for _ in range(12)]
        cases.append(dict(mesh_count=mesh_count, frame_count_raw=frame_count_raw,
                          frame_duration_raw=frame_duration_raw, time=time, mask=mask,
                          fields=(translation, angles), view=view))
    payload = '\n'.join(
        ' '.join(map(str, [case['time'], case['frame_count_raw'], case['frame_duration_raw'],
                           case['mesh_count'], case['mask']] +
                  list(case['fields'][0]) + list(case['fields'][1]) + case['view']))
        for case in cases)
    native = probe_run(probe, 'generic', payload)
    cursor = 0
    matrices = 0
    faults = 0
    for number, case in enumerate(cases):
        line = native[cursor]
        cursor += 1
        if line == 'error':
            fault, calls, _ = run_generic_case(original, case['mesh_count'], case['frame_count_raw'],
                case['frame_duration_raw'], case['fields'], case['view'], case['mask'], case['time'])
            if fault is None or calls or fault['eip'] not in GENERIC_FAULT_EIPS:
                raise ValueError(f'expected the frame divide fault at generic case {number}')
            faults += 1
            continue
        mesh_count_line = int(native[cursor]); cursor += 1
        expected_matrices = [list(map(int, native[cursor + slot].split())) for slot in range(mesh_count_line)]
        cursor += mesh_count_line
        local = list(map(int, native[cursor].split())); cursor += 1
        camera = list(map(int, native[cursor].split())); cursor += 1
        if mesh_count_line != case['mesh_count']:
            raise ValueError(f'native generic mesh count differs at case {number}')
        fault, calls, cleared = run_generic_case(original, case['mesh_count'],
            case['frame_count_raw'], case['frame_duration_raw'], case['fields'], case['view'],
            case['mask'], case['time'])
        if fault is not None:
            raise ValueError(f'generic case {number} faulted: {fault}')
        if cleared != 0:
            raise ValueError(f'0x481060 did not clear the frame global at case {number}')
        if len(calls) != case['mesh_count']:
            raise ValueError(f'generic draw count mismatch at case {number}')
        for slot, call in enumerate(calls):
            if call['mesh'] != MESH_ARRAY + slot * MESH_ENTRY or call['matrices'] != GENERIC_BUFFER:
                raise ValueError(f'generic draw {slot} pointer mismatch at case {number}')
            if call['mask'] != case['mask'] or call['level'] != 0:
                raise ValueError(f'generic draw {slot} argument mismatch at case {number}')
            if line == 'off':
                if call['frame'] != FRAME_SENTINEL:
                    raise ValueError(f'frame global changed without an active counter at case {number}')
            elif call['frame'] != int(line) & 0xFFFFFFFF:
                raise ValueError(f'frame counter mismatch at case {number}')
        if call_pointer := (calls[0]['matrices'] if calls else GENERIC_BUFFER):
            if original.read(call_pointer + 48, 16) != PATTERN_TAIL:
                raise ValueError(f'generic matrix slot wrote past 12 words at case {number}')
        stack_camera = original.read_words(GENERIC_BUFFER)
        if stack_camera != camera:
            raise ValueError(f'generic camera matrix mismatch at case {number}')
        if expected_matrices and any(expected != camera for expected in expected_matrices):
            raise ValueError(f'generic matrix slot differs from the camera matrix at case {number}')
        matrices += case['mesh_count']
    return dict(cases=count, draws=matrices, faults=faults)


def static_native_case(case):
    return (f'{int(case["use_secondary"])} {int(case["has_primary"])} {int(case["has_secondary"])} '
            f'{case["distance"]} {case["mask"]} ' +
            ' '.join(map(str, case['thresholds'])) + ' ' +
            ' '.join(str(lod['mesh_count']) if lod is not None else '-1' for lod in case['lods']) + ' ' +
            ' '.join(map(str, list(case['fields'][0]) + list(case['fields'][1]) + case['view'])))


def run_static_case(original, case):
    fields = case['fields']
    original.write_fields(fields)
    primary = MODEL_PRIMARY if case['has_primary'] else 0
    secondary = MODEL_SECONDARY if case['has_secondary'] else 0
    original.write_object(fields, primary, secondary, case['use_secondary'])
    effective = secondary if (case['use_secondary'] and case['has_secondary']) else primary
    mesh_bases = []
    for index, lod in enumerate(case['lods']):
        if lod is None:
            original.write(CONTAINER + index * CONTAINER_STRIDE, b'\0' * CONTAINER_STRIDE)
            mesh_bases.append(0)
        else:
            mesh_bases.append(original.write_container(index, lod['mesh_count'],
                lod.get('frame_count_raw', 0), lod.get('frame_duration_raw', 0)))
    pointers = [CONTAINER + index * CONTAINER_STRIDE if case['lods'][index] is not None else 0
                for index in range(4)]
    if effective:
        original.write_model(effective, case['thresholds'], pointers)
    original.write_view(case['view'])
    original.write(STATIC_BUFFER, PATTERN[:0x800])
    original.calls = []
    fault = original.call_fault(0x46B830, [OBJECT, case['distance'], case['mask']])
    calls = list(original.calls)
    return fault, calls, mesh_bases


def check_static(original, probe, rng, count):
    cases = []
    for number in range(count):
        use_secondary = rng.randrange(2) == 1
        has_primary = rng.randrange(8) != 0
        has_secondary = rng.randrange(3) != 0
        thresholds = [rng.choice([0, 0x8000, 0x100000, 0x3C0000, 0x800000, 0x2000000]) for _ in range(4)]
        if number % 7 == 0:
            thresholds = sorted((rng.randrange(0, 0x4000000) for _ in range(4)), reverse=True)
        lods = []
        for _ in range(4):
            if rng.randrange(5) == 0:
                lods.append(None)
            else:
                lods.append(dict(mesh_count=rng.choice([0, 1, 2, 6, 18]),
                                 frame_count_raw=0, frame_duration_raw=0))
        distance = rng.choice([-1, 0, 0x7FFF, 0x8000, 0x8001, 0x100000, 0x3C0000, 0x7FFFFFFF])
        if number % 3 == 0:
            distance = rng.randrange(-0x80000000, 0x80000000)
        mask = rng.choice([0, 2, 0x2E, 0x3E, 0x40])
        translation = [rng.randrange(-0x4000000, 0x4000001) for _ in range(3)]
        angles = [rng.randrange(0, 0x100000000) for _ in range(3)]
        view = [rng.randrange(-0x800000, 0x800001) for _ in range(12)]
        cases.append(dict(use_secondary=use_secondary, has_primary=has_primary,
                          has_secondary=has_secondary, thresholds=thresholds, lods=lods,
                          distance=distance, mask=mask, fields=(translation, angles), view=view))
    payload = '\n'.join(static_native_case(case) for case in cases)
    native = probe_run(probe, 'static', payload)
    cursor = 0
    draws = 0
    faults = 0
    for number, case in enumerate(cases):
        line = native[cursor]; cursor += 1
        if line == 'error':
            fault, calls, _ = run_static_case(original, case)
            if fault is None or calls or fault['eip'] not in STATIC_FAULT_EIPS:
                raise ValueError(f'expected the null-record fault at static case {number}')
            faults += 1
            continue
        lod, fill, draw_count = map(int, line.split())
        local = camera = None
        slot_matrices = []
        padding = []
        mesh_indices = []
        if lod != -2:
            local = list(map(int, native[cursor].split())); cursor += 1
            camera = list(map(int, native[cursor].split())); cursor += 1
            slot_lines = max(fill, draw_count)
            all_slots = [list(map(int, native[cursor + slot].split())) for slot in range(slot_lines)]
            cursor += slot_lines
            slot_matrices = all_slots[:fill]
            padding = all_slots[fill:]
            mesh_indices = [int(value) for value in native[cursor].split()]; cursor += 1
        fault, calls, mesh_bases = run_static_case(original, case)
        if fault is not None:
            raise ValueError(f'static case {number} faulted: {fault}')
        buffer = original.read(STATIC_BUFFER, 0x800)
        if lod == -2:
            if calls or buffer != PATTERN[:0x800]:
                raise ValueError(f'static case {number} touched state without a model')
            continue
        # 0x46b830 overwrites the local matrix with the camera product in place.
        if original.read_words(STATIC_BUFFER) != camera:
            raise ValueError(f'static camera matrix mismatch at case {number}')
        if lod < 0:
            if calls:
                raise ValueError(f'static case {number} drew without a selected level')
            if buffer[48:64] != PATTERN_TAIL or buffer[64:] != PATTERN[64:0x800]:
                raise ValueError(f'static case {number} filled slots without a draw')
            continue
        effective = case['lods'][lod]
        if len(calls) != draw_count or draw_count != effective['mesh_count']:
            raise ValueError(f'static draw count mismatch at case {number}')
        if mesh_indices != list(range(draw_count)):
            raise ValueError(f'static mesh index list mismatch at case {number}')
        if any(slot != camera for slot in padding):
            raise ValueError(f'static matrix padding differs from the camera matrix at case {number}')
        for slot, call in enumerate(calls):
            if call['mesh'] != mesh_bases[lod] + slot * MESH_ENTRY or call['matrices'] != STATIC_BUFFER:
                raise ValueError(f'static draw {slot} pointer mismatch at case {number}')
            if call['mask'] != case['mask'] or call['level'] != 0:
                raise ValueError(f'static draw {slot} argument mismatch at case {number}')
        for slot in range(fill):
            if original.read_words(STATIC_BUFFER + slot * 0x40) != slot_matrices[slot]:
                raise ValueError(f'static matrix slot {slot} mismatch at case {number}')
            if original.read(STATIC_BUFFER + slot * 0x40 + 48, 16) != PATTERN_TAIL:
                raise ValueError(f'static matrix slot {slot} wrote past 12 words at case {number}')
        if buffer[fill * 0x40:] != PATTERN[fill * 0x40:0x800]:
            raise ValueError(f'static case {number} filled more slots than the native result')
        draws += draw_count
    return dict(cases=count, draws=draws, faults=faults)


def check_counter(original, probe, rng, count):
    cases = []
    for number in range(count):
        pick = number % 6
        if pick == 0:
            frame_count_raw, frame_duration_raw = rng.randrange(1, 100) << 16, rng.randrange(1, 100) << 16
        elif pick == 1:
            frame_count_raw, frame_duration_raw = rng.randrange(1, 100) << 16, 0
        elif pick == 2:
            frame_count_raw, frame_duration_raw = 0, 0
        elif pick == 3:
            frame_count_raw, frame_duration_raw = 0, rng.randrange(1, 100) << 16
        elif pick == 4:
            frame_count_raw = rng.randrange(-0x80000000, 0x80000000)
            frame_duration_raw = rng.choice([0, rng.randrange(-0x80000000, 0x80000000)])
        else:
            frame_count_raw = rng.randrange(1, 0x8000) << 16
            frame_duration_raw = rng.randrange(1, 0x8000) << 16
        time = rng.choice([0, 1, 0x80000000, 0xFFFFFFFF, rng.randrange(0, 0x100000000)])
        cases.append((time, frame_count_raw, frame_duration_raw))
    # Signed divide edges: INT32_MIN % -1 overflows the original idiv and faults;
    # the neighbouring values stay defined.
    cases.append((0x80000000, -1, 0x10000))
    cases.append((5, -1, 0x10000))
    cases.append((0x80000001, -1, 0x10000))
    payload = '\n'.join(f'{time} {count_raw} {duration_raw}' for time, count_raw, duration_raw in cases)
    native = probe_run(probe, 'counter', payload)
    if len(native) != len(cases):
        raise ValueError('native counter line count differs')
    faults = 0
    for number, (time, frame_count_raw, frame_duration_raw) in enumerate(cases):
        fault, calls, _ = run_generic_case(original, 1, frame_count_raw, frame_duration_raw,
                                           ([0, 0, 0], [0, 0, 0]), [0] * 12, 0, time)
        line = native[number]
        if line == 'error':
            if fault is None or fault['eip'] not in GENERIC_FAULT_EIPS:
                raise ValueError(f'expected divide fault at counter case {number}')
            faults += 1
            continue
        if fault is not None:
            raise ValueError(f'counter case {number} faulted: {fault}')
        if not calls:
            raise ValueError(f'counter case {number} had no stubbed draw')
        observed = calls[0]['frame']
        if line == 'off':
            if observed != FRAME_SENTINEL:
                raise ValueError(f'counter case {number} changed the global without an active rule')
        elif observed != int(line) & 0xFFFFFFFF:
            raise ValueError(f'counter case {number} value mismatch: original {observed}, native {line}')
    return dict(cases=len(cases), faults=faults)


def check_callback(original, probe, rng, count, stock):
    cases = []
    for number in range(count):
        cases.append(dict(name=f'synthetic {number}', stock=False,
                          mesh_count=rng.choice([0, 1, 2, 3, 8, 18]),
                          fields=([rng.randrange(-0x4000000, 0x4000001) for _ in range(3)],
                                  [rng.randrange(0, 0x100000000) for _ in range(3)])))
    for model in stock:
        for index, lod in enumerate(model['lods']):
            cases.append(dict(name=f'{model["name"]} LOD {index}', stock=True,
                              mesh_count=lod['mesh_count'],
                              fields=([rng.randrange(-0x4000000, 0x4000001) for _ in range(3)],
                                      [rng.randrange(0, 0x100000000) for _ in range(3)])))
    payload = '\n'.join(' '.join(map(str, [case['mesh_count']] + list(case['fields'][0]) + list(case['fields'][1])))
                        for case in cases)
    native = probe_run(probe, 'callback', payload)
    cursor = 0
    slots = 0
    for number, case in enumerate(cases):
        expected_count = int(native[cursor]); cursor += 1
        matrices = [list(map(int, native[cursor + slot].split())) for slot in range(expected_count)]
        cursor += expected_count
        original.write_fields(case['fields'])
        original.write_object(case['fields'], MODEL_PRIMARY, 0, False)
        original.write_container(0, case['mesh_count'], 0, 0)
        original.write(CALLBACK_BUFFER, PATTERN[:0x900])
        original.call(0x480FF0, [CONTAINER, OBJECT])
        expected_slots = max(case['mesh_count'], 1)
        if expected_count != expected_slots:
            raise ValueError(f'native callback slot count differs at case {number} ({case["name"]})')
        for slot in range(expected_slots):
            if original.read_words(CALLBACK_BUFFER + slot * 0x40) != matrices[slot]:
                raise ValueError(f'callback slot {slot} mismatch at case {number} ({case["name"]})')
            # 0x480ff0 clears the word after slot 0 (0xc22bbc), and the fill copies
            # all 16 dwords, so every filled slot keeps the pattern in 48..59 and
            # is zero in 60..63.
            if original.read(CALLBACK_BUFFER + slot * 0x40 + 48, 12) != PATTERN[48:60]:
                raise ValueError(f'callback slot {slot} tail differs at case {number}')
            if original.read(CALLBACK_BUFFER + slot * 0x40 + 60, 4) != b'\0\0\0\0':
                raise ValueError(f'callback slot {slot} guard word differs at case {number}')
        tail = expected_slots * 0x40
        if original.read(CALLBACK_BUFFER + tail, 16) != PATTERN[tail:tail + 16]:
            raise ValueError(f'callback wrote beyond its slots at case {number}')
        slots += expected_slots
    return dict(cases=len(cases), slots=slots, stock=sum(1 for case in cases if case['stock']))


def check_stock_static(original, probe, rng, stock):
    cases = []
    for model in stock:
        lods = [dict(mesh_count=lod['mesh_count'],
                     frame_count_raw=lod['frame_count_raw'],
                     frame_duration_raw=lod['frame_duration_raw']) for lod in model['lods']]
        lods = (lods + [None, None, None, None])[:4]
        thresholds = model['thresholds']
        distances = {0x8001, 0x7FFFFFFF}
        for threshold in thresholds:
            distances.add(i32(threshold + 1))
        for distance in sorted(distances):
            cases.append(dict(use_secondary=False, has_primary=True, has_secondary=False,
                              thresholds=thresholds, lods=lods, distance=distance,
                              mask=rng.choice([0, 0x2E, 0x3E]),
                              fields=([rng.randrange(-0x4000000, 0x4000001) for _ in range(3)],
                                      [rng.randrange(0, 0x100000000) for _ in range(3)]),
                              view=[rng.randrange(-0x800000, 0x800001) for _ in range(12)],
                              name=model['name']))
    payload = '\n'.join(static_native_case(case) for case in cases)
    native = probe_run(probe, 'static', payload)
    cursor = 0
    stats = dict(models=len(stock), cases=0, faults=0, draws=0)
    for number, case in enumerate(cases):
        line = native[cursor]; cursor += 1
        if line == 'error':
            fault, calls, _ = run_static_case(original, case)
            if fault is None or fault['eip'] not in STATIC_FAULT_EIPS:
                raise ValueError(f'expected null-record fault at stock static case {number} ({case["name"]})')
            if calls:
                raise ValueError(f'stock static case {number} drew before its fault ({case["name"]})')
            stats['faults'] += 1
            continue
        lod, fill, draw_count = map(int, line.split())
        camera = None
        slot_matrices = []
        padding = []
        mesh_indices = []
        if lod != -2:
            cursor += 1  # native local; overwritten by the camera product in the original
            camera = list(map(int, native[cursor].split())); cursor += 1
            slot_lines = max(fill, draw_count)
            all_slots = [list(map(int, native[cursor + slot].split())) for slot in range(slot_lines)]
            cursor += slot_lines
            slot_matrices = all_slots[:fill]
            padding = all_slots[fill:]
            mesh_indices = [int(value) for value in native[cursor].split()]; cursor += 1
        fault, calls, mesh_bases = run_static_case(original, case)
        if fault is not None:
            raise ValueError(f'stock static case {number} faulted: {fault}')
        if lod == -2:
            continue
        if original.read_words(STATIC_BUFFER) != camera:
            raise ValueError(f'stock static camera mismatch at {case["name"]} distance {case["distance"]}')
        if lod >= 0:
            if len(calls) != draw_count or mesh_indices != list(range(draw_count)):
                raise ValueError(f'stock static draw count mismatch at {case["name"]}')
            if any(slot != camera for slot in padding):
                raise ValueError(f'stock static matrix padding differs at {case["name"]}')
            for slot, call in enumerate(calls):
                if call['mesh'] != mesh_bases[lod] + slot * MESH_ENTRY:
                    raise ValueError(f'stock static mesh pointer mismatch at {case["name"]}')
                if call['matrices'] != STATIC_BUFFER or call['mask'] != case['mask'] or call['level'] != 0:
                    raise ValueError(f'stock static draw argument mismatch at {case["name"]}')
            for slot in range(fill):
                if original.read_words(STATIC_BUFFER + slot * 0x40) != slot_matrices[slot]:
                    raise ValueError(f'stock static slot {slot} mismatch at {case["name"]}')
        stats['cases'] += 1
        stats['draws'] += draw_count
    return stats


def check_generic_stock(original, probe, rng, stock):
    cases = []
    for model in stock:
        for index, lod in enumerate(model['lods']):
            cases.append(dict(name=f'{model["name"]} LOD {index}', mesh_count=lod['mesh_count'],
                              frame_count_raw=lod['frame_count_raw'],
                              frame_duration_raw=lod['frame_duration_raw'],
                              time=rng.randrange(0, 0x100000000), mask=rng.choice([0, 0x2E]),
                              fields=([rng.randrange(-0x4000000, 0x4000001) for _ in range(3)],
                                      [rng.randrange(0, 0x100000000) for _ in range(3)]),
                              view=[rng.randrange(-0x800000, 0x800001) for _ in range(12)]))
    payload = '\n'.join(
        ' '.join(map(str, [case['time'], case['frame_count_raw'], case['frame_duration_raw'],
                           case['mesh_count'], case['mask']] +
                  list(case['fields'][0]) + list(case['fields'][1]) + case['view']))
        for case in cases)
    native = probe_run(probe, 'generic', payload)
    cursor = 0
    stats = dict(models=len(stock), cases=0, active=0, inactive=0, faults=0, draws=0)
    for number, case in enumerate(cases):
        line = native[cursor]; cursor += 1
        fault, calls, cleared = run_generic_case(original, case['mesh_count'], case['frame_count_raw'],
            case['frame_duration_raw'], case['fields'], case['view'], case['mask'], case['time'])
        if line == 'error':
            if fault is None or fault['eip'] not in GENERIC_FAULT_EIPS:
                raise ValueError(f'expected divide fault at stock generic case {number} ({case["name"]})')
            stats['faults'] += 1
            continue
        mesh_count_line = int(native[cursor]); cursor += 1
        camera = None
        for _ in range(mesh_count_line):
            matrix = list(map(int, native[cursor].split())); cursor += 1
            if camera is None:
                camera = matrix
            elif matrix != camera:
                raise ValueError(f'stock generic matrix padding differs at {number}')
        cursor += 2  # local and camera lines
        if fault is not None:
            raise ValueError(f'stock generic case {number} faulted: {fault}')
        if cleared != 0:
            raise ValueError(f'stock generic case {number} did not clear the frame global')
        if len(calls) != case['mesh_count']:
            raise ValueError(f'stock generic draw count mismatch at case {number} ({case["name"]})')
        expected_camera = original.read_words(GENERIC_BUFFER)
        for slot, call in enumerate(calls):
            if call['mesh'] != MESH_ARRAY + slot * MESH_ENTRY or call['matrices'] != GENERIC_BUFFER:
                raise ValueError(f'stock generic draw pointer mismatch at case {number}')
            if call['mask'] != case['mask'] or call['level'] != 0:
                raise ValueError(f'stock generic draw argument mismatch at case {number}')
            if line == 'off':
                if call['frame'] != FRAME_SENTINEL:
                    raise ValueError(f'stock generic inactivity mismatch at case {number} ({case["name"]})')
            elif call['frame'] != int(line) & 0xFFFFFFFF:
                raise ValueError(f'stock generic frame mismatch at case {number} ({case["name"]})')
        if camera is not None and camera != expected_camera:
            raise ValueError(f'stock generic camera mismatch at case {number} ({case["name"]})')
        if line == 'off':
            stats['inactive'] += 1
        else:
            stats['active'] += 1
        stats['cases'] += 1
        stats['draws'] += case['mesh_count']
    return stats


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable')
    parser.add_argument('--probe', required=True)
    parser.add_argument('--models', type=Path)
    parser.add_argument('--seed', type=int, default=45133)
    parser.add_argument('--quick', action='store_true')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    scale = 0.25 if args.quick else 1.0
    rng = random.Random(args.seed)
    source = Path(args.executable).read_bytes()
    if hashlib.sha256(source).hexdigest() != EXPECTED_HASH:
        raise ValueError('unsupported original executable hash')
    original = Original(args.executable)
    probe = Path(args.probe).resolve()
    stock = load_stock(args.models)
    crng = [model for model in stock if model['tag'] == b'crng']
    report = dict(
        source_sha256=EXPECTED_HASH,
        functions=[dict(address=hex(address), bytes=size,
                        sha256=hashlib.sha256(
                            source[va_to_file_offset(source, address):][:size]).hexdigest())
                   for address, size in FUNCTIONS],
        instance=check_instance(original, probe, rng, int(1200 * scale)),
        multiply=check_multiply(original, probe, rng, int(600 * scale)),
        scaled=check_scaled(original, probe, rng, int(500 * scale)),
        counter=check_counter(original, probe, rng, int(600 * scale)),
        callback=check_callback(original, probe, rng, int(400 * scale), crng),
        generic=check_generic(original, probe, rng, int(800 * scale)),
        static=check_static(original, probe, rng, int(900 * scale)),
        stock_static=check_stock_static(original, probe, rng, stock),
        stock_generic=check_generic_stock(original, probe, rng, crng),
        stock_models=len(stock),
        stock_crng=len(crng),
        mismatches=0,
        hooks=['0x46b1d0 redirected to a one-byte ret at 0x6000000; stack arguments and 0xad3cf4 recorded',
               '0x477510 run once to build the original sine and cosine tables'],
        scope=('0x45d1e0/0x45d2f0/0x45cb40 matrices; 0x480ff0 slots; 0x481060 frame counter, '
               'camera matrix and draw records; 0x46b830 model choice, LOD choice, matrix slots and '
               'draw records; stock v5 models from --models used as fixtures'),
    )
    if args.models:
        report['models_directory'] = str(args.models)
    output = json.dumps(report, indent=2)
    print(output)
    if args.report:
        args.report.write_text(output + '\n')


if __name__ == '__main__':
    main()
