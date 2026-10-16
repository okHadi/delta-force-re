#!/usr/bin/env python3
"""Compare the native collision stage with original DF1 x86 code.

Replays:
  0x47E390 terrain ray march (base height)
  0x47E4B0 terrain ray march (detail height, max-height gate)
  0x42D2A0 point-in-face projected test
  0x42D592 mesh box gate
  0x42D6AB face box gate and material gate
  0x42D75D face plane crossing and point assembly
Uses unicorn + pefile from the local tool environment.
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
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI,
    UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EFLAGS, UC_X86_REG_EIP)

parser = argparse.ArgumentParser(description="Compare native collision code with original DF1 x86.")
parser.add_argument("game", type=Path)
parser.add_argument("probe", type=Path)
parser.add_argument("--report", type=Path)
parser.add_argument("--march-cases", type=int, default=2000)
parser.add_argument("--point-cases", type=int, default=3000)
parser.add_argument("--face-cases", type=int, default=3000)
args = parser.parse_args()

PROBE = args.probe.resolve()
source = (args.game / 'Df.exe').read_bytes()
SOURCE_HASH = hashlib.sha256(source).hexdigest()
if SOURCE_HASH != 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff':
    raise SystemExit('Unsupported original executable hash')
pe = pefile.PE(data=source)

STACK = 0x2008000
SCRATCH = 0x2100000
POINT = SCRATCH
FACE = SCRATCH + 0x100
VERTS = SCRATCH + 0x200
NORMAL = SCRATCH + 0x300
MATERIAL = SCRATCH + 0x340
MESH = SCRATCH + 0x400
CONTEXT = SCRATCH + 0x500
FRAME = 0x2000000 + 0x10000

u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
for base, size in ((0x400000, 0x1000000), (0x2000000, 0x20000), (0x2100000, 0x10000),
                   (0x3000000, 0x100000), (0x3100000, 0x100000), (0x3200000, 0x40000),
                   (0x3300000, 0x100000), (0x4000000, 0x1000)):
    u.mem_map(base, size)
u.mem_write(0x400000, pe.get_memory_mapped_image())


def signed(value):
    return value if value < 0x80000000 else value - 0x100000000


def build_maps():
    heights = bytes((x * 13 + y * 7 + (x ^ y)) & 255 for y in range(1024) for x in range(1024))
    characters = bytes((x + y * 3) & 255 for y in range(1024) for x in range(1024))
    detail = struct.pack('<65536I', *[((tile * 17 + at * 3) & 255) << 16 for tile in range(256) for at in range(256)])
    max_height = bytes((x * 31 + y * 19 + ((x * y) & 255)) & 255 for y in range(1024) for x in range(1024))
    return heights, characters, detail, max_height


HEIGHTS, CHARACTERS, DETAIL, MAX_HEIGHT = build_maps()
u.mem_write(0x3000000, HEIGHTS)
u.mem_write(0x3100000, CHARACTERS)
u.mem_write(0x3200000, DETAIL)
u.mem_write(0x3300000, MAX_HEIGHT)
for address, value in ((0xbd5284, 0x3000000), (0xbd5330, 0x3100000), (0xbd531c, 0x3200000), (0xbd52d4, 0x3300000)):
    u.mem_write(address, struct.pack('<I', value))


def zero_registers():
    for register in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                     UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
        u.reg_write(register, 0)


def call_original(function, *data, count=20000):
    """Calls a function with a return sentinel at 0x4000000 and returns EAX."""
    u.mem_write(STACK, struct.pack('<%dI' % (len(data) + 1), 0x4000000, *[value & 0xffffffff for value in data]))
    zero_registers()
    u.reg_write(UC_X86_REG_ESP, STACK)
    u.reg_write(UC_X86_REG_EFLAGS, 2)
    u.emu_start(function, 0x4000000, timeout=10000000, count=count)
    assert u.reg_read(UC_X86_REG_EIP) == 0x4000000, hex(function)
    return signed(u.reg_read(UC_X86_REG_EAX))


def run_until(begin, stops, count=200000):
    """Runs original code until one of the stop addresses executes."""
    reached = []

    def hook(machine, address, size, user):
        if address in stops:
            reached.append(address)
            machine.emu_stop()

    handle = u.hook_add(unicorn.UC_HOOK_CODE, hook)
    try:
        u.emu_start(begin, 0, timeout=10000000, count=count)
    finally:
        u.hook_del(handle)
    return reached[0] if reached else None


def probe_lines(command, lines, *extra):
    process = subprocess.run([str(PROBE), command, *extra], input=('\n'.join(lines) + '\n').encode(),
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if process.returncode != 0:
        raise SystemExit(f'probe {command} failed: {process.stderr.decode()}')
    return process.stdout.decode().splitlines()


def terrain_byte(x, y):
    return HEIGHTS[(y >> 16 & 1023) * 1024 + (x >> 16 & 1023)]


def march_cases(count):
    rng = random.Random(0x47E390)
    cases = []
    edges = [0, 1, -1, 0x10000, -0x10000, 0x3ff0000, 0x3ff0001]
    for _ in range(count):
        if rng.random() < 0.2:
            x = rng.choice(edges) + rng.randrange(-0x1000, 0x1000)
            y = rng.choice(edges) + rng.randrange(-0x1000, 0x1000)
        else:
            x = rng.randrange(0, 1024 << 16)
            y = rng.randrange(0, 1024 << 16)
        if rng.random() < 0.6:
            z = terrain_byte(x, y) * 16384 + rng.randrange(-0x40000, 0x40000)
        else:
            z = rng.randrange(-0x100000, 0x4200000)
        step = [rng.randrange(-0x20000, 0x20001) for _ in range(3)]
        limit = rng.choice([0, 0x1000, 0x10000, 0x40000, rng.randrange(0, 0x800000)])
        cases.append((limit, x, y, z, step[0], step[1], step[2]))
    return cases


def compare_marches():
    cases = march_cases(args.march_cases)
    lines = [' '.join(str(value) for value in case) for case in cases]
    native = probe_lines('march', lines)
    detail_native = probe_lines('march-detail', lines)
    mismatches = []
    hits = misses = detail_hits = 0
    for index, case in enumerate(cases):
        limit, x, y, z, dx, dy, dz = case
        u.mem_write(SCRATCH, struct.pack('<3i', x, y, z) + struct.pack('<3i', dx, dy, dz))
        result = call_original(0x47e390, SCRATCH, SCRATCH + 12, limit, count=200000)
        # The maps are always present here, so zero is a hit at sample index 0.
        if result == 1:
            expected = '0 65536'
        elif result == 0:
            expected = '1 0'
        else:
            expected = f'1 {result}'
        if native[index] != expected:
            mismatches.append(('march', case, native[index], expected))
        hits += expected.startswith('1')
        misses += expected.startswith('0')
        result = call_original(0x47e4b0, SCRATCH, SCRATCH + 12, limit, count=200000)
        if result == 1:
            expected = '0 65536'
        elif result == 0:
            expected = '1 0'
        else:
            expected = f'1 {result}'
        if detail_native[index] != expected:
            mismatches.append(('march-detail', case, detail_native[index], expected))
        detail_hits += expected.startswith('1')
        if len(mismatches) > 8:
            break
    return dict(cases=len(cases), mismatches=mismatches, march_hits=hits, march_misses=misses,
                detail_hits=detail_hits), native, detail_native


def point_cases(count):
    rng = random.Random(0x42D2A0)
    cases = []
    for _ in range(count):
        flags = rng.choice([1, 2, 4])
        vertices = []
        while True:
            vertices = [[rng.randrange(-4096, 4097) for _ in range(3)] for _ in range(3)]
            second = 0 if flags == 4 else (1 if flags == 1 else 2)
            runs = [abs(vertices[i + 1][second] - vertices[i][second]) for i in range(2)]
            runs.append(abs(vertices[0][second] - vertices[2][second]))
            if min(runs) >= 4:
                break
        if rng.random() < 0.5:
            # A point from the stored vertex range with a small offset.
            first = 0 if flags in (1, 2) else 1
            weights = [rng.randrange(200) for _ in range(3)]
            total = sum(weights) + 1
            point = [0, 0, 0]
            for axis in range(3):
                point[axis] = sum(vertices[i][axis] * weights[i] for i in range(3)) // total
                point[axis] = (point[axis] << 8) + rng.randrange(-0x2000, 0x2001)
        else:
            point = [rng.randrange(-0x140000, 0x140000) for _ in range(3)]
        cases.append((flags, point, vertices))
    return cases


def compare_points():
    cases = point_cases(args.point_cases)
    lines = []
    for flags, point, vertices in cases:
        row = [flags, *point]
        for vertex in vertices:
            row.extend(vertex)
        lines.append(' '.join(str(value) for value in row))
    native = probe_lines('point', lines)
    mismatches = []
    inside = 0
    for index, (flags, point, vertices) in enumerate(cases):
        u.mem_write(POINT, struct.pack('<3i', *point))
        face = bytearray(72)
        struct.pack_into('<3h', face, 0x1c, 0, 1, 2)
        u.mem_write(FACE, bytes(face))
        u.mem_write(VERTS, b''.join(struct.pack('<hhhH', *vertex, 0) for vertex in vertices))
        result = call_original(0x42d2a0, POINT, FACE, VERTS, flags)
        expected = str(result & 1)
        if native[index] != expected:
            mismatches.append(('point', flags, point, vertices, native[index], expected))
        inside += expected == '1'
        if len(mismatches) > 8:
            break
    return dict(cases=len(cases), mismatches=mismatches, inside=inside), native


def face_cases(count):
    """Segment, normal, plane, and vertex values for the face ray stage."""
    rng = random.Random(0x42D75D)
    cases = []
    for _ in range(count):
        flags = rng.choice([1, 2, 4])
        second = 0 if flags == 4 else (1 if flags == 1 else 2)
        vertices = []
        while True:
            vertices = [[rng.randrange(-3000, 3001) for _ in range(3)] for _ in range(3)]
            runs = [abs(vertices[i + 1][second] - vertices[i][second]) for i in range(2)]
            runs.append(abs(vertices[0][second] - vertices[2][second]))
            if min(runs) >= 4:
                break
        normal = [rng.randrange(-16384, 16385) for _ in range(3)]
        if normal == [0, 0, 0]:
            normal = [16384, 0, 0]
        weights = [rng.randrange(100) for _ in range(3)]
        total = sum(weights) + 1
        anchor = [sum((int(vertices[i][axis]) << 8) * weights[i] for i in range(3)) // total
                  for axis in range(3)]
        if rng.random() < 0.8:
            plane = -sum(((normal[axis] << 8) * anchor[axis]) >> 22 for axis in range(3))
            span = rng.randrange(1, 0x8000)
        else:
            plane = rng.randrange(-0x100000, 0x100001)
            span = rng.randrange(1, 0x40000)
        p1 = [anchor[axis] + rng.randrange(-span, span + 1) for axis in range(3)]
        p2 = [anchor[axis] + rng.randrange(-span, span + 1) for axis in range(3)]
        if p1 == p2:
            p2[0] += 0x10000
        threshold = 0 if rng.random() < 0.8 else rng.randrange(-0x8000, 0x8001)
        material = rng.choice([0, 2, 0x1000000, 2 | 0x1000000])
        cases.append((p1, p2, normal, flags, plane, threshold, material, vertices))
    return cases


def compare_face_ray():
    cases = face_cases(args.face_cases)
    lines = []
    for p1, p2, normal, flags, plane, threshold, material, vertices in cases:
        row = [*p1, *p2, *normal, flags, plane, threshold, material]
        for vertex in vertices:
            row.extend(vertex)
        lines.append(' '.join(str(value) for value in row))
    native = probe_lines('faceray', lines)
    mismatches = []
    accepted = 0
    for index, (p1, p2, normal, flags, plane, threshold, material, vertices) in enumerate(cases):
        u.mem_write(FRAME + 0x50, struct.pack('<3i', *p2))
        u.mem_write(FRAME + 0x70, struct.pack('<3i', *p1))
        u.mem_write(FRAME + 0xbc, struct.pack('<i', 0))
        u.mem_write(FRAME + 0xc4, struct.pack('<I', MESH))
        u.mem_write(FRAME + 0xe0, struct.pack('<I', CONTEXT))
        u.mem_write(NORMAL, struct.pack('<hhhH', *normal, flags))
        face = bytearray(72)
        struct.pack_into('<3h', face, 0x1c, 0, 1, 2)
        struct.pack_into('<i', face, 0x28, plane)
        struct.pack_into('<I', face, 0x44, MATERIAL)
        u.mem_write(FACE, bytes(face))
        u.mem_write(VERTS, b''.join(struct.pack('<hhhH', *vertex, 0) for vertex in vertices))
        mesh = bytearray(112)
        struct.pack_into('<I', mesh, 0x08, VERTS)
        struct.pack_into('<I', mesh, 0x18, NORMAL)
        u.mem_write(MESH, bytes(mesh))
        u.mem_write(MATERIAL, b'\x00' * 0x10 + struct.pack('<I', material))
        zero_registers()
        u.reg_write(UC_X86_REG_ESP, FRAME)
        u.reg_write(UC_X86_REG_EFLAGS, 2)
        u.reg_write(UC_X86_REG_EDI, FACE)
        u.reg_write(UC_X86_REG_ESI, NORMAL)
        u.reg_write(UC_X86_REG_EBX, threshold & 0xffffffff)
        reached = run_until(0x42d75d, (0x42d6b7, 0x42d98f))
        if reached is None:
            mismatches.append(('faceray-missing', p1, p2, normal, flags, plane, threshold))
            break
        if reached == 0x42d98f and u.reg_read(UC_X86_REG_EAX) & 0xffffffff:
            point = struct.unpack('<3i', u.mem_read(FRAME + 0x60, 12))
            expected = '1 ' + ' '.join(str(value) for value in point)
            accepted += 1
        else:
            expected = '0'
        if native[index] != expected:
            mismatches.append(('faceray', p1, p2, normal, flags, plane, threshold, material, vertices,
                               native[index], expected))
        if len(mismatches) > 8:
            break
    return dict(cases=len(cases), mismatches=mismatches, accepted=accepted), native


def compare_gates():
    """Mesh and face box gates plus the material flag gate."""
    rng = random.Random(0x42D592)
    cases = []
    for _ in range(1000):
        p1 = [rng.randrange(-0x100000, 0x100000) for _ in range(3)]
        p2 = [rng.randrange(-0x100000, 0x100000) for _ in range(3)]
        tolerance = rng.choice([0, 1, 0x1000, rng.randrange(0x20000)])
        mesh_bounds = []
        face_bounds = []
        for axis in range(3):
            middle = (p1[axis] + p2[axis]) // 2 + rng.randrange(-0x40000, 0x40001)
            half = rng.randrange(0, 0x40000)
            mesh_bounds += [middle - half, middle + half]
            middle = (p1[axis] + p2[axis]) // 2 + rng.randrange(-0x40000, 0x40001)
            half = rng.randrange(0, 0x40000)
            face_bounds += [middle - half, middle + half]
        material = rng.choice([0, 0x1000000, 2])
        cases.append((p1, p2, tolerance, mesh_bounds, face_bounds, material))
    mesh_probe = probe_lines('box', [' '.join(str(v) for v in [*p1, *p2, tolerance, *mesh_bounds])
                                     for p1, p2, tolerance, mesh_bounds, _face, _material in cases])
    face_probe = probe_lines('box', [' '.join(str(v) for v in [*p1, *p2, tolerance, *face_bounds])
                                     for p1, p2, tolerance, _mesh, face_bounds, _material in cases])
    mismatches = []
    mesh_passed = face_passed = 0

    def prepare():
        u.mem_write(FRAME + 0x50, struct.pack('<3i', *p2))
        u.mem_write(FRAME + 0x70, struct.pack('<3i', *p1))
        u.mem_write(FRAME + 0x90, struct.pack('<I', 1))
        u.mem_write(FRAME + 0x94, struct.pack('<I', 0))
        u.mem_write(FRAME + 0xb8, struct.pack('<I', CONTEXT))
        u.mem_write(FRAME + 0xc4, struct.pack('<I', MESH))
        u.mem_write(CONTEXT + 0x3c, b'\x00')
        zero_registers()
        u.reg_write(UC_X86_REG_ESP, FRAME)
        u.reg_write(UC_X86_REG_EFLAGS, 2)
        u.reg_write(UC_X86_REG_EBX, tolerance & 0xffffffff)

    for index, (p1, p2, tolerance, mesh_bounds, face_bounds, material) in enumerate(cases):
        mesh = bytearray(112)
        struct.pack_into('<6i', mesh, 0x40, *mesh_bounds)
        struct.pack_into('<I', mesh, 0x0c, 0)
        u.mem_write(MESH, bytes(mesh))
        prepare()
        reached = run_until(0x42d592, (0x42d694, 0x42d504))
        mesh_expected = mesh_probe[index] == '1'
        if reached is None:
            mismatches.append(('mesh-gate-missing', p1, p2, tolerance, mesh_bounds))
            break
        if (reached == 0x42d694) != mesh_expected:
            mismatches.append(('mesh-gate', p1, p2, tolerance, mesh_bounds, reached, mesh_probe[index]))
            if len(mismatches) > 8:
                break
            continue
        if reached != 0x42d694:
            continue
        mesh_passed += 1
        struct.pack_into('<I', mesh, 0x0c, 1)
        struct.pack_into('<I', mesh, 0x10, FACE)
        u.mem_write(MESH, bytes(mesh))
        face = bytearray(72)
        struct.pack_into('<6i', face, 0x2c, *face_bounds)
        struct.pack_into('<I', face, 0x44, MATERIAL)
        u.mem_write(FACE, bytes(face))
        u.mem_write(MATERIAL, b'\x00' * 0x10 + struct.pack('<I', material))
        prepare()
        reached = run_until(0x42d592, (0x42d75d, 0x42d6b7, 0x42d504))
        if reached is None:
            mismatches.append(('face-gate-missing', p1, p2, tolerance, face_bounds))
            break
        face_expected = reached == 0x42d75d
        expected = (face_probe[index] == '1') and (material & 0x1000000) == 0
        if face_expected != expected:
            mismatches.append(('face-gate', p1, p2, tolerance, face_bounds, material, reached, face_probe[index]))
            if len(mismatches) > 8:
                break
            continue
        if face_expected:
            face_passed += 1
    return dict(cases=len(cases), mismatches=mismatches, mesh_passed=mesh_passed, face_passed=face_passed)


def compare_real_models():
    """Decodes stored v5 fixtures and replays sampled faces against the original stage."""
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from pff import Archive
    from model_report import decode
    outdir = Path('.local/collision-real')
    outdir.mkdir(parents=True, exist_ok=True)
    report = dict(models=[], volume_invariants=0, ranges=0, sampled_faces=0, accepted=0,
                  centered_boxes=0, cylinder_bases=0)
    face_rows = []
    face_cases = []
    for name in ('BARREL.3DI', 'AKCRATE.3DI', 'LIB1.3DI', 'MEDTOWER.3DI'):
        path = outdir / name
        if not path.exists():
            Archive(args.game / 'Df.pff').extract(name, path)
        data = path.read_bytes()
        model = decode(data)
        geometry = decode(data, geometry=True)
        volume_rows = probe_lines('volumes', [], str(path))
        face_rows_out = probe_lines('faces', [], str(path))
        counted_volumes = 0
        for lod_index, lod in enumerate(model['lods']):
            counted_volumes += lod['collision_count']
        for row in volume_rows:
            words = row.split()
            if words[0] != 'volume':
                continue
            values = [int(v) for v in words[2:]]
            bounds = values[11:17]
            degenerate = bounds[0] == -2147483648
            for axis in range(3):
                if not bounds[axis * 2] <= bounds[axis * 2 + 1]:
                    raise SystemExit(f'{name}: unordered volume bounds')
                if not degenerate:
                    middle = (bounds[axis * 2 + 1] - bounds[axis * 2]) >> 1
                    if abs(values[8 + axis] - middle) > 1:
                        raise SystemExit(f'{name}: volume extent differs from bounds')
            if values[0] == 1 and not degenerate:
                for axis in range(3):
                    if abs(values[2 + axis] - ((bounds[axis * 2] + bounds[axis * 2 + 1]) >> 1)) <= 2:
                        report['centered_boxes'] += 1
            if values[0] == 3 and not degenerate and values[4] == bounds[4]:
                report['cylinder_bases'] += 1
            report['volume_invariants'] += 1
        counted_ranges = {}
        for row in volume_rows:
            words = row.split()
            if words[0] != 'mesh':
                continue
            counted_ranges.setdefault(int(words[1]), [0, 0])
            counted_ranges[int(words[1])][0] += int(words[2])
            counted_ranges[int(words[1])][1] += int(words[3])
        for lod_index, lod in enumerate(model['lods']):
            start, count = counted_ranges.get(lod_index, [0, 0])
            if count != lod['collision_count']:
                raise SystemExit(f'{name}: mesh ranges differ from the LOD count')
            report['ranges'] += 1
        report['models'].append(dict(name=name, sha256=hashlib.sha256(data).hexdigest(),
                                     lods=len(model['lods']), volumes=counted_volumes,
                                     faces=sum(len(lod['faces']) for lod in geometry['lods'])))
        # Sample stored faces for the original replay.
        plane_rows = []
        for row in face_rows_out:
            words = row.split()
            if words[0] == 'face':
                plane_rows.append([int(v) for v in words[2:]])
        rng = random.Random(0x3D1)
        lod_state = geometry['lods'][0]
        if not lod_state['faces']:
            continue
        for face_index in rng.sample(range(len(lod_state['faces'])), min(60, len(lod_state['faces']))):
            face = lod_state['faces'][face_index]
            if face['normals'][0] >= len(lod_state['normals']):
                continue
            normal = lod_state['normals'][face['normals'][0]]
            positions = [lod_state['vertices'][i][:3] for i in face['vertices']]
            weights = [rng.randrange(50) for _ in range(3)]
            total = sum(weights) + 1
            anchor = [sum(positions[i][axis] * weights[i] for i in range(3)) * 256 // total for axis in range(3)]
            span = rng.randrange(0x100, 0x4000)
            direction = [normal[axis] << 8 for axis in range(3)]
            p1 = [anchor[axis] + direction[axis] * span // 0x4000 for axis in range(3)]
            p2 = [anchor[axis] - direction[axis] * span // 0x4000 for axis in range(3)]
            plane = plane_rows[face_index][0]
            flags = normal[3] & 7
            row = [*p1, *p2, *normal[:3], flags, plane, 0, 0]
            for position in positions:
                row.extend(position)
            face_rows.append(' '.join(str(value) for value in row))
            face_cases.append((path, face_index, p1, p2, normal[:3], flags, plane, positions))
    native = probe_lines('faceray', face_rows) if face_rows else []
    mismatches = []
    for index, (path, face_index, p1, p2, normal, flags, plane, positions) in enumerate(face_cases):
        u.mem_write(FRAME + 0x50, struct.pack('<3i', *p2))
        u.mem_write(FRAME + 0x70, struct.pack('<3i', *p1))
        u.mem_write(FRAME + 0xbc, struct.pack('<i', 0))
        u.mem_write(FRAME + 0xc4, struct.pack('<I', MESH))
        u.mem_write(FRAME + 0xe0, struct.pack('<I', CONTEXT))
        u.mem_write(NORMAL, struct.pack('<hhhH', *normal, flags))
        face = bytearray(72)
        struct.pack_into('<3h', face, 0x1c, 0, 1, 2)
        struct.pack_into('<i', face, 0x28, plane)
        struct.pack_into('<I', face, 0x44, MATERIAL)
        u.mem_write(FACE, bytes(face))
        u.mem_write(VERTS, b''.join(struct.pack('<hhhH', *position, 0) for position in positions))
        mesh = bytearray(112)
        struct.pack_into('<I', mesh, 0x08, VERTS)
        struct.pack_into('<I', mesh, 0x18, NORMAL)
        u.mem_write(MESH, bytes(mesh))
        u.mem_write(MATERIAL, b'\x00' * 0x10)
        zero_registers()
        u.reg_write(UC_X86_REG_ESP, FRAME)
        u.reg_write(UC_X86_REG_EFLAGS, 2)
        u.reg_write(UC_X86_REG_EDI, FACE)
        u.reg_write(UC_X86_REG_ESI, NORMAL)
        reached = run_until(0x42d75d, (0x42d6b7, 0x42d98f))
        if reached == 0x42d98f and u.reg_read(UC_X86_REG_EAX) & 0xffffffff:
            point = struct.unpack('<3i', u.mem_read(FRAME + 0x60, 12))
            expected = '1 ' + ' '.join(str(value) for value in point)
            report['accepted'] += 1
        else:
            expected = '0'
        report['sampled_faces'] += 1
        if native[index] != expected:
            mismatches.append((str(path), face_index, p1, p2, normal, flags, plane, positions,
                               native[index], expected))
        if len(mismatches) > 4:
            break
    report['mismatches'] = mismatches
    return report


march_report = compare_marches()[0]
real_report = compare_real_models()
point_report = compare_points()[0]
face_report = compare_face_ray()[0]
gate_report = compare_gates()
report = dict(source_sha256=SOURCE_HASH, unicorn_version=unicorn.__version__,
              marches=march_report, points=point_report, faces=face_report, gates=gate_report,
              real_models=real_report)
for section in ('marches', 'points', 'faces', 'gates', 'real_models'):
    section_report = report[section]
    if section_report['mismatches']:
        print('MISMATCH', json.dumps(section_report['mismatches'][:3], default=str)[:2000])
        raise SystemExit(1)
destination = args.report or Path('.local/reports/collision-original-check.json')
destination.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({key: {k: v for k, v in value.items() if k != 'mismatches'} for key, value in report.items()
                  if isinstance(value, dict)}))
