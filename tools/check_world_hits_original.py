#!/usr/bin/env python3
"""Compare the native world hit stage with original DF1 x86 code.

Replays:
  0x477620  integer square root
  0x477670  approximate 3D length
  0x45D170  matrix build used to produce the per-mesh matrices
  0x42DB80  model list hit selection (front-side gate, plane crossing, point test)
  0x42E2B0  world segment query (length, detail march, two lists, water)
Uses unicorn + pefile from the local tool environment.
"""
import argparse
import hashlib
import json
import random
import struct
import subprocess
from pathlib import Path

import pefile
import unicorn
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI,
    UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EFLAGS, UC_X86_REG_EIP)

parser = argparse.ArgumentParser(description="Compare native world hits with original DF1 x86.")
parser.add_argument("game", type=Path)
parser.add_argument("probe", type=Path)
parser.add_argument("--report", type=Path)
parser.add_argument("--math-cases", type=int, default=2000)
parser.add_argument("--hit-cases", type=int, default=100)
parser.add_argument("--query-cases", type=int, default=200)
args = parser.parse_args()

PROBE = args.probe.resolve()
source = (args.game / 'Df.exe').read_bytes()
SOURCE_HASH = hashlib.sha256(source).hexdigest()
if SOURCE_HASH != 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff':
    raise SystemExit('Unsupported original executable hash')
pe = pefile.PE(data=source)

STACK = 0x2008000
SCRATCH = 0x2200000
SCRATCH_END = 0x2240000
TERRAIN_HEIGHT = 0x3000000
TERRAIN_CHARACTER = 0x3100000
TERRAIN_DETAIL = 0x3200000
TERRAIN_BOUNDS = 0x3300000
GLOBAL_LIST = (0x867E80, 0x7DDE80)
GLOBAL_COUNT = (0x8CF904, 0x8CFA1C)
WATER = 0xBD5248
SENTINEL = 0x4000000
MATRIX_ONE = 0x400000  # 1.0 in the original 22-bit rotation units

u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
for base, size in ((0x400000, 0x1000000), (0x2000000, 0x20000), (0x2200000, 0x40000),
                   (0x3000000, 0x100000), (0x3100000, 0x100000), (0x3200000, 0x40000),
                   (0x3300000, 0x100000), (0x4000000, 0x1000)):
    u.mem_map(base, size)
u.mem_write(0x400000, pe.get_memory_mapped_image())
# The divide-error message path is not part of the comparison.
u.mem_write(0x493A80, b'\xc3')
u.mem_write(0x4564B0, b'\xc3')


def signed(value):
    return value if value < 0x80000000 else value - 0x100000000


def build_maps():
    heights = bytes((x * 13 + y * 7 + (x ^ y)) & 255 for y in range(1024) for x in range(1024))
    characters = bytes((x + y * 3) & 255 for y in range(1024) for x in range(1024))
    detail = struct.pack('<65536I', *[((tile * 17 + at * 3) & 255) << 16 for tile in range(256) for at in range(256)])
    bounds = bytes((x * 31 + y * 19 + ((x * y) & 255)) & 255 for y in range(1024) for x in range(1024))
    return heights, characters, detail, bounds


HEIGHTS, CHARACTERS, DETAIL, BOUNDS = build_maps()
u.mem_write(TERRAIN_HEIGHT, HEIGHTS)
u.mem_write(TERRAIN_CHARACTER, CHARACTERS)
u.mem_write(TERRAIN_DETAIL, DETAIL)
u.mem_write(TERRAIN_BOUNDS, BOUNDS)
for address, value in ((0xbd5284, TERRAIN_HEIGHT), (0xbd5330, TERRAIN_CHARACTER),
                       (0xbd531c, TERRAIN_DETAIL), (0xbd52d4, TERRAIN_BOUNDS), (WATER, 0)):
    u.mem_write(address, struct.pack('<I', value))


def call_original(function, *data, count=200000):
    u.mem_write(STACK, struct.pack('<%dI' % (len(data) + 1), SENTINEL, *[value & 0xffffffff for value in data]))
    for register in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                     UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
        u.reg_write(register, 0)
    u.reg_write(UC_X86_REG_ESP, STACK)
    u.reg_write(UC_X86_REG_EFLAGS, 2)
    u.emu_start(function, SENTINEL, timeout=10000000, count=count)
    assert u.reg_read(UC_X86_REG_EIP) == SENTINEL, hex(function)
    return signed(u.reg_read(UC_X86_REG_EAX))


def probe_lines(command, lines):
    process = subprocess.run([str(PROBE), command], input=('\n'.join(lines) + '\n').encode(),
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if process.returncode != 0:
        raise SystemExit(f'probe {command} failed: {process.stderr.decode()}')
    return process.stdout.decode().splitlines()


class Bump:
    def __init__(self):
        self.at = SCRATCH

    def reset(self):
        self.at = SCRATCH

    def take(self, size, align=4):
        self.at = (self.at + align - 1) & ~(align - 1)
        result = self.at
        self.at += size
        if self.at > SCRATCH_END:
            raise SystemExit('world scratch exhausted')
        return result


def write_i32(address, values):
    u.mem_write(address, struct.pack('<%di' % len(values), *values))


class World:
    """Lays one scenario into emulated memory and fills in each mesh matrix."""
    def __init__(self):
        self.bump = Bump()

    def reset(self):
        self.bump.reset()

    def place_record(self, record, record_address):
        u.mem_write(record_address, b'\x00' * 0x114)
        write_i32(record_address, [record['active']])
        write_i32(record_address + 0x24, record['position'])
        model_address = self.bump.take(0x100, 0x10)
        slot_address = self.bump.take(0x100, 0x10)
        # The callback returns a per-mesh state array; allocate it away from
        # the record so the 0x40-byte records never overlap other data.
        state_base = self.bump.take(0x40 * (len(record['meshes']) or 1), 0x10)
        stub_address = self.bump.take(16, 4)
        u.mem_write(stub_address, bytes([0xB8]) + struct.pack('<I', state_base) + b'\xc3')
        u.mem_write(model_address, b'\x00' * 0x7c)
        write_i32(model_address + 0x4c, [stub_address])
        write_i32(model_address + 0x50, [slot_address])
        write_i32(model_address + 0x60, [record['radius']])
        write_i32(record_address + 0x10, [model_address])
        mesh_count = len(record['meshes'])
        mesh_addresses = [self.bump.take(0x70, 0x10) for _ in range(mesh_count)]
        write_i32(slot_address + 0x98, [mesh_count])
        write_i32(slot_address + 0x9c, [mesh_addresses[0] if mesh_addresses else 0])
        for mesh_index, mesh in enumerate(record['meshes']):
            mesh_address = mesh_addresses[mesh_index]
            u.mem_write(mesh_address, b'\x00' * 0x70)
            face_count = max(len(mesh['faces']), 1)
            vertex_address = self.bump.take(8 * max(len(mesh['vertices']), 1), 8)
            normal_address = self.bump.take(8 * face_count, 8)
            face_address = self.bump.take(0x48 * face_count, 8)
            material_addresses = [self.bump.take(0x4c, 4) for _ in range(face_count)]
            write_i32(mesh_address + 8, [vertex_address])
            write_i32(mesh_address + 0xc, [len(mesh['faces'])])
            write_i32(mesh_address + 0x10, [face_address])
            write_i32(mesh_address + 0x18, [normal_address])
            for index, vertex in enumerate(mesh['vertices']):
                u.mem_write(vertex_address + index * 8, struct.pack('<hhhH', vertex[0], vertex[1], vertex[2], 0))
            for face_index, face in enumerate(mesh['faces']):
                face_at = face_address + face_index * 0x48
                u.mem_write(face_at, b'\x00' * 0x48)
                u.mem_write(face_at + 0x1c, struct.pack('<3h', *face['indices']))
                write_i32(face_at + 0x28, [face['plane']])
                write_i32(face_at + 0x2c, face['box'])
                write_i32(face_at + 0x44, [material_addresses[face_index]])
                u.mem_write(normal_address + face_index * 8,
                            struct.pack('<hhhH', *face['normal'], face['normal_flags']))
                u.mem_write(material_addresses[face_index] + 0x10, struct.pack('<I', face['material_flags']))
                face['material_address'] = material_addresses[face_index]
        matrices = []
        for mesh_index, mesh in enumerate(record['meshes']):
            state = state_base + mesh_index * 0x40
            write_i32(state, mesh['state'])
            u.mem_write(state + 0x3c, bytes([mesh['state_flags']]))
            matrix_address = self.bump.take(0x30, 0x10)
            call_original(0x45D170, state, matrix_address, count=4000)
            matrices.append(list(struct.unpack('<12i', u.mem_read(matrix_address, 0x30))))
        for mesh_index, matrix in enumerate(matrices):
            record['meshes'][mesh_index]['matrix'] = matrix

    def place_list(self, records, base):
        for index, record in enumerate(records):
            self.place_record(record, base + index * 0x114)

    def place_points(self, values):
        address = self.bump.take(12, 4)
        write_i32(address, values)
        return address


def crafted_mesh(rng, local_origin, delta, material_flags, second_face=False):
    """A mesh whose identity inverse matrix sends the ray through a triangle.

    The stored state uses an identity rotation, so 0x45D170 produces an
    identity matrix with the translation that maps the caller's ray onto
    `local_origin`. The face plane crosses the local axis delta at its middle,
    the box spans the segment, and the triangle surrounds the crossing point
    in the projection plane the normal flags select.
    """
    axis = 0
    for candidate in range(3):
        if delta[candidate]:
            axis = candidate
            break
    sign = -1 if delta[axis] > 0 else 1
    level = local_origin[axis] + delta[axis] // 2
    plane = -sign * level
    normal = [0, 0, 0]
    normal[axis] = 16384 * sign
    flags = {0: 4, 1: 2, 2: 1}[axis]
    crossing = list(local_origin)
    crossing[axis] = level
    others = [candidate for candidate in range(3) if candidate != axis]
    first, second = others
    base_first = crossing[first] // 256
    base_second = crossing[second] // 256
    faces = []
    vertex_base = 0
    for face_flags in ([material_flags, 0] if second_face else [material_flags]):
        vertices = []
        for offset_first, offset_second in ((-600, -600), (600, -600), (0, 600)):
            vertex = [0, 0, 0]
            vertex[first] = base_first + offset_first
            vertex[second] = base_second + offset_second
            vertices.append(tuple(vertex))
        box = []
        for candidate in range(3):
            low = min(local_origin[candidate], local_origin[candidate] + delta[candidate]) - 0x10000
            high = max(local_origin[candidate], local_origin[candidate] + delta[candidate]) + 0x10000
            box += [low, high]
        faces.append(dict(indices=(vertex_base, vertex_base + 1, vertex_base + 2), vertices=vertices,
                          normal=list(normal), normal_flags=flags, plane=plane, box=box,
                          material_flags=face_flags))
        vertex_base += 3
    state = [MATRIX_ONE, 0, 0, 0, 0, MATRIX_ONE, 0, 0, 0, 0, MATRIX_ONE, 0, 0, 0, 0, 0]
    return dict(state=state, state_flags=0,
                vertices=[v for face in faces for v in face['vertices']], faces=faces,
                translation=(local_origin, delta))


def random_mesh(rng):
    state = [rng.randrange(-0x2000000, 0x2000000) for _ in range(12)]
    state += [rng.randrange(-0x10000, 0x10000) for _ in range(4)]
    faces = []
    vertex_base = 0
    for _ in range(rng.randrange(1, 3)):
        vertices = []
        for _ in range(3):
            vertices.append((rng.randrange(-600, 601), rng.randrange(-600, 601), rng.randrange(-600, 601)))
        flags = rng.choice([1, 2, 4])
        normal = [0, 0, 0]
        normal[{1: 2, 2: 1, 4: 0}[flags]] = rng.choice([-16384, 16384])
        box = []
        for _axis in range(3):
            low = rng.randrange(-0x4000, 0x4000)
            box += [low, low + rng.randrange(0, 0x8000)]
        faces.append(dict(indices=(vertex_base, vertex_base + 1, vertex_base + 2), vertices=vertices,
                          normal=normal, normal_flags=flags, plane=rng.randrange(-0x8000, 0x8000),
                          box=box, material_flags=rng.choice([0, 2, 0x800, 0x868C, 0x1000000])))
        vertex_base += 3
    return dict(state=state, state_flags=1 if rng.random() < 0.12 else 0,
                vertices=[v for face in faces for v in face['vertices']], faces=faces)


def apply_translation(mesh, world_matrix_translation):
    """Sets the state so 0x45D170 outputs the given translation."""
    mesh['state'][3] = -world_matrix_translation[0]
    mesh['state'][7] = -world_matrix_translation[1]
    mesh['state'][11] = -world_matrix_translation[2]


def craft_records(rng, origin, delta, radius, local_origin, slot_material, second_lists=False):
    mesh = crafted_mesh(rng, local_origin, delta, slot_material, second_face=second_lists)
    apply_translation(mesh, [local_origin[axis] - origin[axis] for axis in range(3)])
    record = dict(active=1, identity=rng.randrange(1, 0x100000), position=list(origin), radius=radius,
                  meshes=[mesh])
    return [record]


def scenario_lines_modelhit(records, header):
    lines = [header]
    for record_index, record in enumerate(records):
        lines.append(' '.join(str(v) for v in ['record', 0, record['active'], record['identity'],
                                               *record['position'], record['radius'], len(record['meshes'])]))
        for mesh_index, mesh in enumerate(record['meshes']):
            lines.append(' '.join(str(v) for v in ['mesh', 0, record_index, mesh['state_flags'],
                                                   *mesh['matrix'], 0, len(mesh['vertices'])]))
            for vertex in mesh['vertices']:
                lines.append(' '.join(str(v) for v in ['vertex', 0, record_index, mesh_index, *vertex]))
            for face in mesh['faces']:
                lines.append(' '.join(str(v) for v in ['face', 0, record_index, mesh_index,
                                                       face['material_flags'], face['plane'], *face['box'],
                                                       *face['indices'], *face['normal'], face['normal_flags']]))
    return lines


def compare_math():
    rng = random.Random(0x477620)
    values = [0, 1, 2, 3, 4, 5, -1, -5, 0x7fffffff, 0x40000000, 0x3fffffff, 2 ** 31 - 2]
    values += [rng.randrange(-2 ** 31, 2 ** 31) for _ in range(args.math_cases)]
    native = probe_lines('sqrt', [str(value) for value in values])
    mismatches = []
    for index, value in enumerate(values):
        expected = str(call_original(0x477620, value, count=2000))
        if native[index] != expected:
            mismatches.append(('sqrt', value, native[index], expected))
            if len(mismatches) > 4:
                return dict(square_root_cases=len(values), length_cases=0, mismatches=mismatches)
    triples = [(rng.randrange(-2 ** 31, 2 ** 31), rng.randrange(-2 ** 31, 2 ** 31),
                rng.randrange(-2 ** 31, 2 ** 31)) for _ in range(args.math_cases)]
    triples += [(0, 0, 0), (-1, 0, 0), (3, -4, 5), (2 ** 31 - 1, -2 ** 31, 0)]
    native = probe_lines('length', [' '.join(str(v) for v in triple) for triple in triples])
    for index, triple in enumerate(triples):
        expected = str(call_original(0x477670, *triple, count=2000))
        if native[index] != expected:
            mismatches.append(('length', triple, native[index], expected))
            if len(mismatches) > 8:
                break
    return dict(square_root_cases=len(values), length_cases=len(triples), mismatches=mismatches)


def compare_model_hits():
    rng = random.Random(0x42DB80)
    cases = []
    world = World()
    for case_index in range(args.hit_cases):
        origin = [rng.randrange(-0x100000, 0x100001) for _ in range(3)]
        delta = [rng.choice([-0x8000, 0x8000]) if axis == 0 else 0 for axis in range(3)]
        end = [origin[axis] + delta[axis] for axis in range(3)]
        bounds = []
        for axis in range(3):
            low, high = sorted((origin[axis], end[axis]))
            bounds += [low, high]
        # The crossing fraction scales the ray length, so the crafted ray keeps
        # length and direction equal to the delta for a consistent hit point.
        length = abs(delta[0])
        direction = [0x10000 if delta[0] > 0 else -0x10000, 0, 0]
        if case_index % 4 == 3:
            length = rng.randrange(0x8000, 0x80000)
            direction = [rng.randrange(-0x10000, 0x10001) for _ in range(3)]
            # Random mesh for breadth; the other cases target the deep path.
            records = [dict(active=1, identity=rng.randrange(1, 0x100000), position=list(origin),
                            radius=0x40000, meshes=[random_mesh(rng)])]
        else:
            local_origin = [rng.randrange(-0x8000, 0x8001) for _ in range(3)]
            if case_index % 8 == 6:
                # A face that crosses but is material-excluded; the list call
                # then reports the masked count through its return value.
                material = 0x868C
            else:
                material = rng.choice([0, 2])
            records = craft_records(rng, origin, delta, 0x40000, local_origin, material,
                                    second_lists=case_index % 8 == 5)
        cases.append(dict(origin=origin, end=end, direction=direction, length=length, bounds=bounds,
                          skip=[0, 0], records=records))
    lines = []
    for case in cases:
        # Every case starts from the same scratch base so the probe identities
        # and the emulated record pointers agree in both layout passes.
        world.reset()
        base = world.bump.take(0x114 * len(case['records']) + 0x10, 0x10)
        world.place_list(case['records'], base)
        case['base'] = base
        if rng.random() < 0.2:
            # The original compares record pointers; the port compares identities.
            case['records'][0]['identity'] = base
            case['skip'] = [base, 0]
        header = ' '.join(str(v) for v in ['modelhit', *case['origin'], *case['end'], *case['direction'],
                                           case['length'], *case['bounds'], *case['skip']])
        lines.extend(scenario_lines_modelhit(case['records'], header))
    native = probe_lines('scenario', lines)
    mismatches = []
    hits = masked_cases = divided_cases = 0
    for index, case in enumerate(cases):
        # Layout again from the same start so the record addresses match the
        # identities already placed in the probe input.
        world.reset()
        base = world.bump.take(0x114 * len(case['records']) + 0x10, 0x10)
        world.place_list(case['records'], base)
        query = world.bump.take(0x100, 0x10)
        write_i32(query, case['origin'])
        write_i32(query + 0x10, case['end'])
        write_i32(query + 0x20, case['direction'])
        write_i32(query + 0x30, [case['length']])
        write_i32(query + 0x34, case['bounds'])
        write_i32(query + 0x4c, case['skip'])
        return_value = call_original(0x42db80, case['base'], len(case['records']), query, count=200000)
        flag = struct.unpack('<i', u.mem_read(query + 0x58, 4))[0]
        distance = struct.unpack('<i', u.mem_read(query + 0x5c, 4))[0]
        record_index = struct.unpack('<i', u.mem_read(query + 0x60, 4))[0]
        mesh_index = struct.unpack('<i', u.mem_read(query + 0x64, 4))[0]
        material = struct.unpack('<I', u.mem_read(query + 0x54, 4))[0]
        face_index = -1
        for record_number, record in enumerate(case['records']):
            for mesh_number, mesh in enumerate(record['meshes']):
                for candidate, face in enumerate(mesh['faces']):
                    if face['material_address'] == material:
                        face_index = candidate
        parts = native[index].split()
        native_count, native_flag, native_divided = int(parts[0]), parts[1] == '1', parts[2] == '1'
        native_record, native_mesh, native_face = int(parts[3]), int(parts[4]), int(parts[5])
        native_distance = int(parts[7])
        if flag:
            hits += 1
        if native_divided:
            divided_cases += 1
        if not flag and native_count > 1:
            masked_cases += 1
        ok = native_count == return_value and native_flag == bool(flag)
        if flag:
            ok = (ok and native_face == face_index and native_record == record_index
                  and native_mesh == mesh_index and native_distance == distance)
        if not ok:
            mismatches.append(('modelhit', index, native[index], return_value, flag, distance, record_index,
                               mesh_index, face_index))
            if len(mismatches) > 4:
                break
    return dict(cases=len(cases), hits=hits, masked_only=masked_cases, divided=divided_cases, mismatches=mismatches)


def compare_queries():
    rng = random.Random(0x42E2B0)
    cases = []
    world = World()
    for case_index in range(args.query_cases):
        x = rng.randrange(0, 1024 << 16)
        y = rng.randrange(0, 1024 << 16)
        delta = [rng.choice([-0x8000, 0x8000]), 0, 0]
        high = case_index % 3 != 0
        if high:
            # Above the terrain so the list stage runs.
            z = 0x3000000
            end = [x + delta[0], y, z]
        else:
            byte = HEIGHTS[(y >> 16 & 1023) * 1024 + (x >> 16 & 1023)]
            z = byte * 16384 + rng.randrange(-0x20000, 0x20000)
            end = [x + delta[0], y + rng.randrange(-0x4000, 0x4001), z + rng.randrange(-0x4000, 0x4001)]
        start = [x, y, z]
        water = rng.choice([0, 0, 0, rng.randrange(-0x100000, 0x4000000)])
        skip = [rng.randrange(1, 0x100000), rng.randrange(1, 0x100000)]
        first = []
        second = []
        if high:
            local_origin = [rng.randrange(-0x8000, 0x8001) for _ in range(3)]
            material = rng.choice([0, 2])
            target = second if case_index % 4 == 0 else first
            target.extend(craft_records(rng, start, delta, 0x40000, local_origin, material,
                                        second_lists=case_index % 8 == 6))
            leftover = second if target is first else first
            if case_index % 5 == 0:
                leftover.extend(craft_records(rng, start, delta, 0x40000, local_origin, 0x868C))
        cases.append(dict(start=start, end=end, water=water, first=first, second=second, skip=skip))
    # Branch coverage: zero length and the 0x3200000 rejection stay explicit.
    cases.append(dict(start=[0x10000, 0x20000, 0x300000], end=[0x10000, 0x20000, 0x300000], water=0,
                      first=[], second=[], skip=[0, 0]))
    cases.append(dict(start=[0x10000, 0x20000, 0x300000], end=[0x3000000, 0x20000, 0x300000], water=0,
                      first=[], second=[], skip=[0, 0]))
    for case in cases:
        world.place_list(case['first'], GLOBAL_LIST[0])
        world.place_list(case['second'], GLOBAL_LIST[1])
        case['start_address'] = world.place_points(case['start'])
        case['end_address'] = world.place_points(case['end'])
    lines = []
    for case in cases:
        header = ' '.join(str(v) for v in ['segment', case['water'], *case['start'], *case['end'], *case['skip']])
        lines.append(header)
        for slot, records in ((0, case['first']), (1, case['second'])):
            for record_index, record in enumerate(records):
                lines.append(' '.join(str(v) for v in ['record', slot, record['active'], record['identity'],
                                                       *record['position'], record['radius'], len(record['meshes'])]))
                for mesh_index, mesh in enumerate(record['meshes']):
                    lines.append(' '.join(str(v) for v in ['mesh', slot, record_index, mesh['state_flags'],
                                                           *mesh['matrix'], 0, len(mesh['vertices'])]))
                    for vertex in mesh['vertices']:
                        lines.append(' '.join(str(v) for v in ['vertex', slot, record_index, mesh_index, *vertex]))
                    for face in mesh['faces']:
                        lines.append(' '.join(str(v) for v in ['face', slot, record_index, mesh_index,
                                                               face['material_flags'], face['plane'], *face['box'],
                                                               *face['indices'], *face['normal'],
                                                               face['normal_flags']]))
    native = probe_lines('scenario', lines)
    mismatches = []
    kinds = {}
    for index, case in enumerate(cases):
        # Rewrite the global lists with this case's records before it runs.
        world.reset()
        world.place_list(case['first'], GLOBAL_LIST[0])
        world.place_list(case['second'], GLOBAL_LIST[1])
        case['start_address'] = world.place_points(case['start'])
        case['end_address'] = world.place_points(case['end'])
        u.mem_write(GLOBAL_COUNT[0], struct.pack('<i', len(case['first'])))
        u.mem_write(GLOBAL_COUNT[1], struct.pack('<i', len(case['second'])))
        u.mem_write(WATER, struct.pack('<i', case['water']))
        code = call_original(0x42e2b0, case['skip'][0], case['start_address'], case['skip'][1],
                             case['end_address'], count=200000)
        parts = native[index].split()
        native_code = int(parts[0])
        kinds[parts[1]] = kinds.get(parts[1], 0) + 1
        if len(parts) > 9 and parts[9] == '1':
            kinds['water_layer'] = kinds.get('water_layer', 0) + 1
        if native_code != code:
            mismatches.append(('segment', index, native[index], code))
            if len(mismatches) > 4:
                break
    return dict(cases=len(cases), kinds=kinds, mismatches=mismatches)


math_report = compare_math()
print('math', {k: v for k, v in math_report.items() if k != 'mismatches'})
if math_report['mismatches']:
    print('MISMATCH', json.dumps(math_report['mismatches'][:3]))
    raise SystemExit(1)
hit_report = compare_model_hits()
print('model hits', {k: v for k, v in hit_report.items() if k != 'mismatches'})
if hit_report['mismatches']:
    print('MISMATCH', json.dumps(hit_report['mismatches'][:3], default=str))
    raise SystemExit(1)
query_report = compare_queries()
print('queries', {k: v for k, v in query_report.items() if k != 'mismatches'})
if query_report['mismatches']:
    print('MISMATCH', json.dumps(query_report['mismatches'][:3], default=str))
    raise SystemExit(1)
report = dict(source_sha256=SOURCE_HASH, unicorn_version=unicorn.__version__, math=math_report,
              model_hits=hit_report, queries=query_report)
destination = args.report or Path('.local/reports/world-hits-original-check.json')
destination.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({k: {key: value for key, value in v.items() if key != 'mismatches'}
                  for k, v in report.items() if isinstance(v, dict)}))
