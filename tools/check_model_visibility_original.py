#!/usr/bin/env python3
"""Replay original model visibility code with Unicorn and compare the native probe.

Original Df.exe functions covered (sha256
abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff):

  0x44acf0  sphere projection and clip mask                (probe mode project)
  0x4818f0  callback row lookup, 16 proven entries          (probe mode callback)
  0x481a00  matrix row lookup, 16 proven entries            (probe mode callback)
  0x485040  one queue record for an actor/effect record     (probe mode collect)
  0x485290  scene collection, second effect loop            (probe mode prep)
  0x4854d0  cull pass with 0x484fb0 row tables              (probe mode cull)
  0x485580  draw pass with 0x46b780 level dispatch          (probe mode lod, pass)
  0x485420  large list pass and 0x46bb20 view setup         (probe mode pass on 0xc23e70)
  0x484fb0  row table visibility test                       (probe mode visible)

The four level callbacks of 0x46b780 are stubbed with a one-byte `ret` at 0x6000000.
A code hook records (level_data, mask, object, level) from their stack arguments.
All other executed code is original image code.

This is deterministic static replay. It does not run the game.
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess

import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP

EXPECTED_HASH = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
STUB = 0x6000000
RETURN_TRAP = 0x6001000
STACK = 0x200F000
POINT = 0x2003000
SCRATCH = 0x2000000
ACTOR_BASE = 0x2010000
MODEL_BASE = 0x2020000
CLASS_BASE = 0x2030000
DATA_BASE = 0x2040000
ROW_WORDS_BASE = 0x2050000
TABLE_ROWS = 24

CAMERA = 0x8CFA20
CAMERA_FOCAL = CAMERA + 0x54
CAMERA_CENTRE_X = CAMERA + 0x60
CAMERA_CENTRE_Y = CAMERA + 0x64
CAMERA_PLANES = CAMERA + 0x1880
CAMERA_POS = CAMERA + 0x4
CAMERA_STATE_MASK = CAMERA + 0x1960
CAMERA_STATE_RADIUS = CAMERA + 0x1964
VIEW_MATRIX = 0x8D1340
BOUND_LEFT = 0x8CFA54
BOUND_TOP = 0x8CFA58
BOUND_RIGHT = 0x8CFA5C
BOUND_BOTTOM = 0x8CFA60
ROW_KEYS = 0xC93EA0
ROW_POINTERS = 0x8D0688
QUEUE_LARGE, QUEUE_LARGE_COUNT = 0xC23E70, 0xC54478
QUEUE_NEAR, QUEUE_NEAR_COUNT = 0xC3CD70, 0xC54474
QUEUE_CHARACTER, QUEUE_CHARACTER_COUNT = 0xC3B570, 0xC54470
SUPPRESS_OBJECT = 0x6BA3F0
SUPPRESS_ALLOW = 0x6BA3EC
CALLBACK_TABLE = 0x500C1C
CALLBACK_ROWS = 16
CALLBACK_STRIDE = 0x14
ACTOR_COUNTS = (0x8CFA18, 0x8CFA1C, None, 0x8CF904)
ARRAYS = (0x7CCA80, 0x7DDE80, 0x822E80, 0x867E80)
EFFECT_ARRAY = 0x9F3098
EFFECT_GATE = 0xFC
RECORD_SIZE = 0x114


def object_address(index):
    return ACTOR_BASE + index * 0x200


def model_address(index):
    return MODEL_BASE + index * 0x200


def view_from_case(rng):
    """Random real-shaped camera: focal, centres, five planes, matrix, position."""
    focal = rng.randrange(1, 1025)
    centre_x = rng.randrange(0, 2048) << 16
    centre_y = rng.randrange(0, 2048) << 16
    sine = int(4194304 * rng.uniform(0.05, 0.99))
    cosine = int(4194304 * rng.uniform(0.05, 0.99))
    near = rng.randrange(1, 0x10000)
    planes = [(4194304, 0, 0, near), (sine, -cosine, 0, 0), (sine, cosine, 0, 0),
              (sine, 0, cosine, 0), (sine, 0, -cosine, 0)]
    matrix = [rng.randrange(-4194304, 4194305) for _ in range(12)]
    position = [rng.randrange(-0x2000000, 0x2000001) for _ in range(3)]
    return {'focal': focal, 'centre': (centre_x, centre_y), 'planes': planes, 'matrix': matrix,
            'position': position}


def payload_view(view):
    values = [view['focal'], view['centre'][0], view['centre'][1]]
    for plane in view['planes']:
        values.extend(plane)
    values.extend(view['matrix'])
    values.extend(view['position'])
    return ' '.join(map(str, values))


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
        u.mem_map(SCRATCH, 0x100000)
        u.mem_map(0x2100000, 0x100000)
        u.mem_map(0x6000000, 0x10000)
        u.mem_write(STUB, b'\xc3')
        self.calls = []
        u.hook_add(unicorn.UC_HOOK_CODE, self._record_call, None, STUB, STUB + 1)

    def _record_call(self, uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        values = struct.unpack('<4I', uc.mem_read(esp + 4, 16))
        self.calls.append((values[3], values[0], values[1], values[2]))

    def write(self, address, data):
        self.unicorn.mem_write(address, data)

    def read(self, address, size):
        return self.unicorn.mem_read(address, size)

    def read_u32(self, address):
        return struct.unpack('<I', self.read(address, 4))[0]

    def read_i32(self, address):
        return struct.unpack('<i', self.read(address, 4))[0]

    def call(self, address, args):
        u = self.unicorn
        esp = STACK - 16
        u.mem_write(esp, struct.pack('<I', RETURN_TRAP))
        for index, arg in enumerate(args):
            u.mem_write(esp + 4 + index * 4, struct.pack('<I', arg & 0xFFFFFFFF))
        u.reg_write(UC_X86_REG_ESP, esp)
        u.emu_start(address, RETURN_TRAP, count=4000000)
        if u.reg_read(UC_X86_REG_EIP) != RETURN_TRAP:
            raise ValueError(f'original function 0x{address:x} did not return')
        return u.reg_read(UC_X86_REG_EAX)

    def write_camera(self, view):
        u = self.unicorn
        u.mem_write(CAMERA_FOCAL, struct.pack('<i', view['focal']))
        u.mem_write(CAMERA_CENTRE_X, struct.pack('<i', view['centre'][0]))
        u.mem_write(CAMERA_CENTRE_Y, struct.pack('<i', view['centre'][1]))
        plane_bytes = b''.join(struct.pack('<4i', *plane) for plane in view['planes'])
        u.mem_write(CAMERA_PLANES, plane_bytes)
        u.mem_write(VIEW_MATRIX, struct.pack('<12i', *view['matrix']))
        u.mem_write(CAMERA_POS, struct.pack('<3i', *view['position']))
        u.mem_write(CAMERA_STATE_MASK, struct.pack('<I', 0))
        u.mem_write(CAMERA_STATE_RADIUS, struct.pack('<I', 0))

    def write_record(self, address, view_position, radius, hidden, class_kind, model=True,
                     model_index=0):
        u = self.unicorn
        record = bytearray(RECORD_SIZE)
        struct.pack_into('<I', record, 0, 1)
        struct.pack_into('<I', record, 4, CLASS_BASE)
        struct.pack_into('<I', record, 0x10, model_address(model_index) if model else 0)
        struct.pack_into('<I', record, 0x14, 0)
        struct.pack_into('<3i', record, 0x24, *view_position)
        record[0x6C] = 2 if hidden else 0
        struct.pack_into('<I', record, 0x94, 0)
        u.mem_write(address, bytes(record))
        u.mem_write(CLASS_BASE, b'\0' * 0x28 + struct.pack('<i', 3 if class_kind else 0) + b'\0' * 0xD4)
        u.mem_write(model_address(model_index) + 0x60, struct.pack('<i', radius))

    def clear_queues(self):
        for base, count in ((QUEUE_LARGE, QUEUE_LARGE_COUNT), (QUEUE_NEAR, QUEUE_NEAR_COUNT),
                            (QUEUE_CHARACTER, QUEUE_CHARACTER_COUNT)):
            self.write(base, b'\0' * (RECORD_SIZE * 4))
            self.write(count, struct.pack('<i', 0))


def probe_run(probe, mode, payload):
    result = subprocess.run([str(probe), mode], input=payload.encode(), capture_output=True)
    if result.returncode != 0:
        raise ValueError(f'probe mode {mode} failed: {result.stderr.decode()}')
    return result.stdout.decode().splitlines()


def check_project(original, probe, count, rng):
    payload = []
    expected = []
    for _ in range(count):
        view = view_from_case(rng)
        radius = rng.choice([0, rng.randrange(1, 0x8000), rng.randrange(0x8000, 0x400000)])
        depth = rng.choice([rng.randrange(-0x200000, 0), rng.randrange(1, 0x1000),
                            rng.randrange(0x1000, 0x500000), rng.randrange(0x500000, 0x5000000)])
        point_y = rng.randrange(-0x500000, 0x500001)
        point_z = rng.randrange(-0x500000, 0x500001)
        original.write_camera(view)
        original.write(POINT, struct.pack('<3i', depth, point_y, point_z))
        original.calls.clear()
        returned = original.call(0x44ACF0, [CAMERA, POINT, radius])
        out = struct.unpack('<3i', original.read(POINT, 12))
        state_radius = original.read_i32(CAMERA_STATE_RADIUS)
        if returned == 1:
            state = 0
        elif depth < radius:
            state = 2
        else:
            state = 1
        expected.append(f'{state} {returned} {out[0]} {out[1]} {out[2]} {state_radius}')
        plane_values = ' '.join(str(v) for plane in view['planes'] for v in plane)
        payload.append(f"{view['focal']} {view['centre'][0]} {view['centre'][1]} {plane_values} "
                       f"{radius} {depth} {point_y} {point_z}")
    lines = probe_run(probe, 'project', '\n'.join(payload) + '\n')
    compare('project', expected, lines)
    return count


def check_lod(original, probe, count, rng):
    payload = []
    expected = []
    for _ in range(count):
        thresholds = sorted((rng.choice([0, 0x8000, rng.randrange(0, 0x1000000)]) for _ in range(4)),
                            reverse=True)
        radius = rng.choice([0x8000, 0x8001, rng.randrange(0, 0x1000000)])
        original.clear_queues()
        original.write(model_address(0) + 0x18, struct.pack('<4i', *thresholds))
        for level in range(4):
            original.write(model_address(0) + 0x3C + level * 4, struct.pack('<I', STUB))
            original.write(model_address(0) + 0x50 + level * 4,
                           struct.pack('<I', DATA_BASE + level * 0x100))
        original.write_record(object_address(1), [0, 0, 0], 0x10000, False, False)
        original.write(QUEUE_NEAR, struct.pack('<I', object_address(1)) + b'\0' * 4 +
                       struct.pack('<iI', radius, 0) + b'\0' * 8)
        original.write(QUEUE_NEAR_COUNT, struct.pack('<i', 1))
        original.calls.clear()
        original.call(0x485580, [4])
        level = original.calls[0][0] if original.calls else -1
        expected.append(str(level))
        payload.append(' '.join(map(str, thresholds)) + f' {radius}')
    lines = probe_run(probe, 'lod', '\n'.join(payload) + '\n')
    compare('lod', expected, lines)
    return count


def build_pass_batch(original, rng, large_list):
    """One 0x485580/0x485420 batch. Returns (payload, entries, model config)."""
    thresholds = sorted((rng.choice([0, 0x8000, rng.randrange(0, 0x1000000)]) for _ in range(4)),
                        reverse=True)
    object_primary = rng.choice([0, model_address(1)])
    object_secondary = rng.choice([0, model_address(2)])
    prefer = rng.randrange(2)
    presence = [rng.random() > 0.15 for _ in range(4)]
    for model in (object_primary, object_secondary):
        if model == 0:
            continue
        original.write(model + 0x18, struct.pack('<4i', *thresholds))
        for level in range(4):
            original.write(model + 0x3C + level * 4, struct.pack('<I', STUB))
            original.write(model + 0x50 + level * 4,
                           struct.pack('<I', DATA_BASE + level * 0x100 if presence[level] else 0))
    entries = []
    for _ in range(rng.randrange(1, 7)):
        object_index = rng.randrange(0, 5) if (rng.random() < 0.25 and not large_list) else rng.randrange(1, 5)
        radius = rng.choice([0x8000, 0x8001, rng.randrange(0, 0x1000000)])
        mask = rng.choice([0, 1, 2, 0x2E, 0x3E, 0x40, rng.randrange(0, 0x100)])
        entries.append((object_index, radius, mask))
    payload = ' '.join(map(str, thresholds))
    payload += ' ' + ' '.join(str(DATA_BASE + level * 0x100) if presence[level] else '-1'
                              for level in range(4))
    payload += f' {STUB} {STUB} {STUB} {STUB}'
    payload += f' {object_primary} {object_secondary} {prefer} {len(entries)}'
    for object_index, radius, mask in entries:
        payload += f' {object_index} {radius} {mask}'
    original.calls.clear()
    return payload, entries, object_primary, object_secondary, prefer


def run_pass_batch(original, entries, large_list, object_primary, object_secondary, prefer):
    for index in range(1, 5):
        record = bytearray(RECORD_SIZE)
        record[0] = 1
        struct.pack_into('<I', record, 4, CLASS_BASE)
        struct.pack_into('<I', record, 0x10, object_primary)
        struct.pack_into('<I', record, 0x14, object_secondary)
        record[0x6C] = 8 if prefer else 0
        original.write(object_address(index), bytes(record))
    queue = QUEUE_LARGE if large_list else QUEUE_NEAR
    original.write(queue, b'\0' * (RECORD_SIZE * 8))
    for position, (object_index, radius, mask) in enumerate(entries):
        record = bytearray(0x18)
        struct.pack_into('<I', record, 0, object_address(object_index) if object_index else 0)
        struct.pack_into('<iI', record, 8, radius, mask)
        original.write(queue + position * 0x18, bytes(record))
    original.write(QUEUE_LARGE_COUNT, struct.pack('<i', len(entries) if large_list else 0))
    original.write(QUEUE_NEAR_COUNT, struct.pack('<i', 0 if large_list else len(entries)))
    original.write(QUEUE_CHARACTER_COUNT, struct.pack('<i', 0))
    if large_list:
        original.call(0x485420, [4])
    else:
        original.call(0x485580, [4])


def check_pass(original, probe, batches, rng, large_list):
    payload = []
    expected = []
    for _ in range(batches):
        batch, entries, primary, secondary, prefer = build_pass_batch(original, rng, large_list)
        payload.append(batch)
        run_pass_batch(original, entries, large_list, primary, secondary, prefer)
        lines = []
        for level, data, mask, obj in original.calls:
            object_index = (obj - ACTOR_BASE) // 0x200
            lines.append(f'call {level} {data} {mask} {object_index}')
        expected.extend(lines if lines else ['none'])
    lines = probe_run(probe, 'pass', '\n'.join(payload) + '\n')
    compare('large pass' if large_list else 'draw pass', expected, lines)
    return batches


def check_collect(original, probe, count, rng):
    payload = []
    expected = []
    for _ in range(count):
        view = view_from_case(rng)
        kind = rng.choice([0, 0, 0, 1])
        hidden = rng.random() < 0.15
        position = [rng.randrange(-0x2000000, 0x2000001) for _ in range(3)]
        radius = rng.choice([0x10000, rng.randrange(0x1000, 0x800000)])
        original.write_camera(view)
        original.clear_queues()
        original.write_record(ARRAYS[0], position, radius, hidden, kind)
        original.call(0x485040, [ARRAYS[0], 1])
        lines = queue_lines(original)
        expected.append(lines[0] if lines else 'reject')
        payload.append(f"{payload_view(view)} {kind} {position[0]} {position[1]} {position[2]} "
                       f"{radius} {int(hidden)} {kind}")
    lines = probe_run(probe, 'collect', '\n'.join(payload) + '\n')
    compare('collect', expected, lines)
    return count


def queue_lines(original):
    """Read the three queues after a collection call."""
    lines = []
    for base, count, keep in ((QUEUE_NEAR, QUEUE_NEAR_COUNT, 5),
                              (QUEUE_LARGE, QUEUE_LARGE_COUNT, 2),
                              (QUEUE_CHARACTER, QUEUE_CHARACTER_COUNT, 5)):
        total = original.read_i32(count)
        for index in range(total):
            record = original.read(base + index * 0x18, 0x18)
            object_field = struct.unpack_from('<I', record, 0)[0]
            radius = struct.unpack_from('<i', record, 8)[0]
            mask = struct.unpack_from('<I', record, 0xC)[0]
            x, y = struct.unpack_from('<hh', record, 0x10)
            depth = struct.unpack_from('<i', record, 0x14)[0]
            if object_field == 0:
                continue
            kind = 'near' if base == QUEUE_NEAR else ('large' if base == QUEUE_LARGE else 'character')
            if keep == 2:
                lines.append(f'{kind} {mask} {radius} 0 0 0')
            else:
                lines.append(f'{kind} {mask} {radius} {x} {y} {depth}')
    return lines


def check_prep(original, probe, count, rng):
    payload = []
    expected = []
    for _ in range(count):
        view = view_from_case(rng)
        kind = rng.choice([0, 0, 1])
        hidden = rng.random() < 0.15
        position = [rng.randrange(-0x2000000, 0x2000001) for _ in range(3)]
        radius = rng.choice([0x10000, rng.randrange(0x1000, 0x800000)])
        effect = rng.random() < 0.7
        effect_position = [rng.randrange(-0x2000000, 0x2000001) for _ in range(3)]
        effect_radius = rng.randrange(0x1000, 0x800000)
        original.write_camera(view)
        original.clear_queues()
        original.write_record(ARRAYS[0], position, radius, hidden, kind)
        original.write(EFFECT_ARRAY, b'\0' * (RECORD_SIZE * 2))
        effect_record = bytearray(RECORD_SIZE)
        effect_record[0] = 1
        struct.pack_into('<I', effect_record, 0x10, model_address(1))
        struct.pack_into('<3i', effect_record, 0x24, *effect_position)
        struct.pack_into('<i', effect_record, EFFECT_GATE, 1 if effect else 0)
        original.write(EFFECT_ARRAY, bytes(effect_record))
        original.write(model_address(1) + 0x60, struct.pack('<i', effect_radius))
        for index, count_address in enumerate(ACTOR_COUNTS):
            if count_address is None:
                continue
            original.write(count_address, struct.pack('<i', 1 if index == 0 else 0))
        original.call(0x485290, [])
        grouped = {name: [] for name in ('near', 'large', 'character')}
        for line in queue_lines(original):
            grouped[line.split()[0]].append(line)
        for name in ('near', 'large', 'character'):
            entries = grouped[name]
            expected.append(f"{name} {len(entries)}" + ''.join(' ' + line.split(' ', 1)[1] for line in entries))
        payload.append(f"{payload_view(view)} {kind} {position[0]} {position[1]} {position[2]} "
                       f"{radius} {int(hidden)} {int(effect)} {effect_position[0]} {effect_position[1]} "
                       f"{effect_position[2]} {effect_radius}")
    lines = probe_run(probe, 'prep', '\n'.join(payload) + '\n')
    compare('prep', expected, lines)
    return count


def check_visible(original, probe, count, rng):
    done = 0
    for _ in range(count // 200):
        bounds = (rng.randrange(0, 3), rng.randrange(0, 3), TABLE_ROWS - 1 - rng.randrange(0, 3),
                  TABLE_ROWS - 1 - rng.randrange(0, 3))
        keys, rows = [], []
        for row in range(TABLE_ROWS):
            keys.append(rng.choice([0, 0x10000, 0x400000, rng.randrange(0, 0x800000)]))
            rows.append([rng.randrange(0, 0x10000) for _ in range(TABLE_ROWS)])
        original.write(ROW_KEYS, struct.pack('<%di' % TABLE_ROWS, *keys))
        row_bytes = b''.join(struct.pack('<%dH' % TABLE_ROWS, *row) for row in rows)
        original.write(ROW_WORDS_BASE, row_bytes)
        original.write(ROW_POINTERS, struct.pack('<%dI' % TABLE_ROWS,
                                                 *[ROW_WORDS_BASE + index * TABLE_ROWS * 2
                                                   for index in range(TABLE_ROWS)]))
        original.write(BOUND_LEFT, struct.pack('<i', bounds[0]))
        original.write(BOUND_TOP, struct.pack('<i', bounds[1]))
        original.write(BOUND_RIGHT, struct.pack('<i', bounds[2]))
        original.write(BOUND_BOTTOM, struct.pack('<i', bounds[3]))
        payload = [f'{bounds[0]} {bounds[1]} {bounds[2]} {bounds[3]}']
        for row in range(TABLE_ROWS):
            payload.append(' '.join(map(str, [keys[row]] + rows[row])))
        expected = []
        cases = []
        for case in range(200):
            radius = rng.choice([0, 1, rng.randrange(0, 8) << 16, rng.randrange(0, 12) << 16])
            x = rng.randrange(-24, TABLE_ROWS + 24)
            y = rng.randrange(-24, TABLE_ROWS + 24)
            depth = rng.choice([rng.randrange(0, 0x100000), rng.randrange(0x100000, 0x4000000)])
            record = bytearray(0x18)
            struct.pack_into('<iI', record, 8, radius, 0)
            struct.pack_into('<hh', record, 0x10, x, y)
            struct.pack_into('<i', record, 0x14, depth)
            original.write(POINT, bytes(record))
            expected.append(str(original.call(0x484FB0, [POINT])))
            payload.append(f'{radius} {x} {y} {depth}')
            cases.append(case)
            done += 1
        lines = probe_run(probe, 'visible', '\n'.join(payload) + '\n')
        compare('visible', expected, lines)
    return done


def check_cull(original, probe, count, rng):
    done = 0
    for _ in range(count // 200):
        bounds = (rng.randrange(0, 3), rng.randrange(0, 3), TABLE_ROWS - 1 - rng.randrange(0, 3),
                  TABLE_ROWS - 1 - rng.randrange(0, 3))
        keys, rows = [], []
        for row in range(TABLE_ROWS):
            keys.append(rng.choice([0, 0x10000, 0x400000, rng.randrange(0, 0x800000)]))
            rows.append([rng.randrange(0, 0x10000) for _ in range(TABLE_ROWS)])
        original.write(ROW_KEYS, struct.pack('<%di' % TABLE_ROWS, *keys))
        row_bytes = b''.join(struct.pack('<%dH' % TABLE_ROWS, *row) for row in rows)
        original.write(ROW_WORDS_BASE, row_bytes)
        original.write(ROW_POINTERS, struct.pack('<%dI' % TABLE_ROWS,
                                                 *[ROW_WORDS_BASE + index * TABLE_ROWS * 2
                                                   for index in range(TABLE_ROWS)]))
        original.write(BOUND_LEFT, struct.pack('<i', bounds[0]))
        original.write(BOUND_TOP, struct.pack('<i', bounds[1]))
        original.write(BOUND_RIGHT, struct.pack('<i', bounds[2]))
        original.write(BOUND_BOTTOM, struct.pack('<i', bounds[3]))
        payload = [f'{bounds[0]} {bounds[1]} {bounds[2]} {bounds[3]}']
        for row in range(TABLE_ROWS):
            payload.append(' '.join(map(str, [keys[row]] + rows[row])))
        expected = []
        for case in range(200):
            character = rng.random() < 0.5
            object_index = rng.randrange(1, 5)
            suppressed_index = rng.choice([0, object_index, rng.randrange(1, 5)])
            allow = rng.randrange(2)
            radius = rng.choice([0, 1, rng.randrange(0, 12) << 16])
            x = rng.randrange(-24, TABLE_ROWS + 24)
            y = rng.randrange(-24, TABLE_ROWS + 24)
            depth = rng.randrange(0, 0x4000000)
            record = bytearray(0x18)
            struct.pack_into('<I', record, 0, object_address(object_index))
            struct.pack_into('<iI', record, 8, radius, 0)
            struct.pack_into('<hh', record, 0x10, x, y)
            struct.pack_into('<i', record, 0x14, depth)
            base, count_address = (QUEUE_CHARACTER, QUEUE_CHARACTER_COUNT) if character else \
                (QUEUE_NEAR, QUEUE_NEAR_COUNT)
            original.write(QUEUE_NEAR, b'\0' * 0x60)
            original.write(QUEUE_CHARACTER, b'\0' * 0x60)
            original.write(base, bytes(record))
            original.write(QUEUE_NEAR_COUNT, struct.pack('<i', 0 if character else 1))
            original.write(QUEUE_CHARACTER_COUNT, struct.pack('<i', 1 if character else 0))
            original.write(SUPPRESS_OBJECT,
                           struct.pack('<I', object_address(suppressed_index) if suppressed_index else 0x1000))
            original.write(SUPPRESS_ALLOW, struct.pack('<i', allow))
            original.call(0x4854D0, [4])
            surviving = original.read_u32(base)
            expected.append('0' if surviving == 0 else str(object_index))
            payload.append(f'{object_index} {suppressed_index} {allow} {int(character)} {radius} {x} {y} {depth}')
            done += 1
        lines = probe_run(probe, 'cull', '\n'.join(payload) + '\n')
        compare('cull', expected, lines)
    return done


def compare(name, expected, actual):
    if expected == actual:
        return
    for index, (want, got) in enumerate(zip(expected, actual)):
        if want != got:
            raise ValueError(f'{name} case {index}: original={want} native={got}')
    raise ValueError(f'{name}: line count differs, original={len(expected)} native={len(actual)}')


def check_callback(original, probe, count, rng):
    """Compare the proven callback table and the 0x4818f0 / 0x481a00 lookups."""
    table = original.read(CALLBACK_TABLE, CALLBACK_ROWS * CALLBACK_STRIDE)
    expected_table = []
    keys = []
    near_misses = []
    for index in range(CALLBACK_ROWS):
        key, draw, matrix = struct.unpack_from('<III', table, index * CALLBACK_STRIDE)
        keys.append(key)
        expected_table.append(f'{key} {draw} {matrix}')
        near_misses.append(struct.unpack_from('<I', table, index * CALLBACK_STRIDE + 0xC)[0])
    cases = keys + near_misses + [0, 0xFFFFFFFF]
    cases += [rng.randrange(0, 0x100000000) for _ in range(count)]
    lines = probe_run(probe, 'callback', '\n'.join(map(str, cases)) + '\n')
    if not lines or lines[0] != f'table {CALLBACK_ROWS}':
        raise ValueError('callback probe did not print the table header')
    compare('callback table', expected_table, lines[1:1 + CALLBACK_ROWS])
    expected = []
    for key in cases:
        draw = original.call(0x4818F0, [key])
        matrix = original.call(0x481A00, [key])
        expected.append(f'{draw} {matrix}')
    compare('callback resolver', expected, lines[1 + CALLBACK_ROWS:])
    return len(cases)


def run(executable, probe, counts, output):
    rng = random.Random(0x485040)
    original = Original(executable)
    result = {'source_sha256': original.digest, 'probe': str(probe),
              'addresses': ['44ACF0', '46B780', '4818F0', '481A00', '484FB0', '485040', '485290',
                            '485420', '4854D0', '485580']}
    result['project_cases'] = check_project(original, probe, counts['project'], rng)
    result['callback_cases'] = check_callback(original, probe, counts['callback'], rng)
    result['lod_cases'] = check_lod(original, probe, counts['lod'], rng)
    result['collect_cases'] = check_collect(original, probe, counts['collect'], rng)
    result['prep_cases'] = check_prep(original, probe, counts['prep'], rng)
    result['visible_cases'] = check_visible(original, probe, counts['visible'], rng)
    result['cull_cases'] = check_cull(original, probe, counts['cull'], rng)
    result['draw_pass_batches'] = check_pass(original, probe, counts['pass'], rng, False)
    result['large_pass_batches'] = check_pass(original, probe, counts['pass'], rng, True)
    result['mismatches'] = 0
    if output:
        output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('executable')
    parser.add_argument('--probe', required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--project', type=int, default=1500)
    parser.add_argument('--callback', type=int, default=64)
    parser.add_argument('--lod', type=int, default=750)
    parser.add_argument('--collect', type=int, default=750)
    parser.add_argument('--prep', type=int, default=250)
    parser.add_argument('--visible', type=int, default=1600)
    parser.add_argument('--cull', type=int, default=1600)
    parser.add_argument('--pass', dest='bpass', type=int, default=120)
    args = parser.parse_args()
    run(args.executable, Path(args.probe).resolve(),
        {'project': args.project, 'callback': args.callback, 'lod': args.lod,
         'collect': args.collect, 'prep': args.prep,
         'visible': args.visible, 'cull': args.cull, 'pass': args.bpass},
        args.output)
