#!/usr/bin/env python3
"""Replay hash-bound DF1 damage, water, bone and muzzle instructions silently."""
import argparse
import hashlib
import json
import random
import struct
import subprocess
from pathlib import Path
import pefile
import unicorn as uc
from unicorn.x86_const import *

MASK = 0xffffffff
HASH = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
TARGET, OWNER, DEF, CALLBACK, PERSON = 0x2201000, 0x2202000, 0x2203000, 0x2204000, 0x2205000
PROJECTILE, MATERIAL, QUERY = 0x2206000, 0x2207000, 0x2208000
STACK, STOP = 0x2008000, 0x4000000


def signed(n):
    n &= MASK
    return n if n < 0x80000000 else n - 0x100000000


def pack(*values):
    return struct.pack('<%dI' % len(values), *(n & MASK for n in values))


def setword(image, at, value):
    image[at:at + 4] = pack(value)


def words(data):
    return struct.unpack('<%dI' % (len(data) // 4), data)


class Replay:
    def __init__(self, source):
        self.u = uc.Uc(uc.UC_ARCH_X86, uc.UC_MODE_32)
        for at, size in ((0x400000, 0x1000000), (0x2000000, 0x20000),
                         (0x2200000, 0x100000), (STOP, 0x1000)):
            self.u.mem_map(at, size)
        self.u.mem_write(0x400000, pefile.PE(data=source).get_memory_mapped_image())
        self.hooks = []

    def read(self, at, count=1):
        return words(self.u.mem_read(at, count * 4))

    def write(self, at, *values):
        self.u.mem_write(at, pack(*values))

    def hook(self, at, fn):
        def callback(u, address, size, _):
            fn()
            esp = u.reg_read(UC_X86_REG_ESP)
            ret = self.read(esp)[0]
            u.reg_write(UC_X86_REG_ESP, esp + 4)
            u.reg_write(UC_X86_REG_EIP, ret)
        self.hooks.append(self.u.hook_add(uc.UC_HOOK_CODE, callback, begin=at, end=at))
        self.u.ctl_remove_cache(at, at + 1)

    def clear_hooks(self):
        for hook in self.hooks:
            self.u.hook_del(hook)
        self.hooks.clear()

    def call(self, function, *args):
        self.write(STACK, STOP, *args)
        for reg in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
            self.u.reg_write(reg, 0)
        self.u.reg_write(UC_X86_REG_ESP, STACK)
        self.u.reg_write(UC_X86_REG_EFLAGS, 2)
        self.u.reg_write(UC_X86_REG_FPCW, 0x37f)
        self.u.emu_start(function, STOP, timeout=10000000, count=500000)
        assert self.u.reg_read(UC_X86_REG_EIP) == STOP, (hex(function), hex(self.u.reg_read(UC_X86_REG_EIP)))
        return self.u.reg_read(UC_X86_REG_EAX)

    def projectile(self, values, velocity=(0, 0, 0), flags=0, owner=0):
        self.u.mem_write(PROJECTILE, bytes(0x114))
        for at, value in zip((0x7c, 0x80, 0x84, 0xfc), values):
            self.write(PROJECTILE + at, value)
        self.write(PROJECTILE + 0x3c, *velocity)
        self.write(PROJECTILE + 0x6c, flags)
        self.write(PROJECTILE + 0xf8, owner)

    def query(self, q):
        self.u.mem_write(QUERY, bytes(0x70))
        for at, values in ((0, q[:3]), (0x10, q[3:6]), (0x20, q[6:9]),
                           (0x30, q[9:16]), (0x4c, q[16:17])):
            self.write(QUERY + at, *values)


def probe(executable, mode, cases, extra=()):
    rows = [' '.join(str(n if isinstance(n, str) else n & MASK) for n in row) for row in cases]
    done = subprocess.run([str(executable), mode, *map(str, extra)], input='\n'.join(rows) + '\n',
                          capture_output=True, text=True)
    assert done.returncode == 0, (mode, done.stderr)
    return done.stdout.splitlines()


def match(executable, mode, cases, expected, extra=()):
    actual = probe(executable, mode, cases, extra)
    assert len(actual) == len(expected), (mode, len(actual), len(expected))
    for index, (found, wanted) in enumerate(zip(actual, expected)):
        assert found == wanted, (mode, index, cases[index], found, wanted)


def run(args):
    source = (args.game / 'Df.exe').read_bytes()
    assert hashlib.sha256(source).hexdigest() == HASH, 'Unsupported executable'
    r = Replay(source)
    rng = random.Random(0x464c50)
    counts = {}
    cases, expected = [], []
    edges = [0, 1, 2, 7, 31, 32, 63, 0x7fffffff, 0x80000000, MASK]
    for _ in range(args.cases):
        row = [rng.choice(edges) if rng.randrange(3) else rng.getrandbits(32) for _ in range(5)]
        threshold, *p = row
        r.projectile(p)
        expected.append(str(r.call(0x464c00, threshold, PROJECTILE)))
        cases.append(row)
    match(args.probe, 'amount', cases, expected)
    counts['464C00'] = len(cases)

    cases, expected = [], []
    faults = 0
    for i in range(args.cases):
        water = rng.choice(edges) if i % 5 == 0 else rng.randint(-1000000, 1000000)
        start = signed(water + rng.choice([0, 1, 511, 512, 513, -1, rng.randint(-200000, 200000)]))
        end = signed(water + rng.randint(-200000, 200000))
        direction = rng.choice([-65536, -32768, -1, 0, 1, 32768, 65536])
        q = [0, 0, start, 0, 0, end, 0, 0, direction, 0x100000, 0, 0, 0, 0, min(start, end), max(start, end), 0]
        saved = rng.getrandbits(32)
        r.query(q)
        r.write(0xbd5248, water)
        r.write(QUERY + 0x58, saved)
        try:
            code = r.call(0x464ba0, QUERY)
        except uc.UcError:
            faults += 1
            r = Replay(source)
            continue
        expected.append(f'{code} {r.read(QUERY + 0x58)[0]}')
        cases.append([water, *q, saved])
    match(args.probe, 'water', cases, expected)
    counts['464BA0'] = len(cases)
    counts['464BA0_original_divide_faults'] = faults

    cases, expected = [], []
    for i in range(args.cases):
        a, owner = bytearray(rng.randbytes(0x114)), bytearray(rng.randbytes(0x114))
        setword(a, 4, DEF if i % 23 else 0)
        setword(a, 0xc, PERSON)
        setword(a, 0x110, CALLBACK)
        a[0x6c] = rng.choice([0, 1, 2, 4, 0x80, 0x81])
        setword(a, 0x70, (rng.getrandbits(16) << 16) | rng.randrange(3))
        setword(owner, 0x70, rng.randrange(3))
        p = [rng.choice([0, 1, 31, 65536]), rng.choice(edges), rng.getrandbits(32), rng.getrandbits(32)]
        velocity = rng.choice([(0, 0, 0), (65536, 0, 0), (-65536, 0, 0), (0, 65536, 0), (0, -65536, 0)])
        flags = rng.choice([0, 0x100])
        d = [rng.choice([0, 1, 3, 3]), rng.choice([0, 0x80]), rng.choice(edges)]
        second = [rng.randrange(5), 0, rng.choice(edges)]
        state = [rng.choice([0, 1, 1]), rng.choice([0, 0x40]), rng.choice([0, TARGET]),
                 rng.choice([0, signed(struct.unpack_from('<I', a, 0x2c)[0]), 1000]), rng.getrandbits(32), rng.getrandbits(32)]
        ownerid = rng.choice([0, OWNER, TARGET])
        kind, mutation, person = rng.choice([0, 1, 2, 3, MASK]), rng.randrange(4), rng.randrange(256)
        row = [a.hex(), owner.hex(), *p, *velocity, flags, *d, *second, *state, ownerid, kind, mutation, person]
        r.u.mem_write(TARGET, bytes(a));r.u.mem_write(OWNER, bytes(owner))
        for address, definition in ((DEF, d), (DEF + 0x100, second)):
            r.write(address + 0x28, definition[0]);r.write(address + 0x24, definition[1]);r.write(address + 0x98, definition[2])
        r.projectile(p, velocity, flags, ownerid)
        r.write(MATERIAL + 0x60, kind)
        for address, value in zip((0x67f528, 0x67f549, 0xc23820, 0xbd5248, 0xa158c0, 0xa158bc), state):
            r.write(address, value)
        r.u.mem_write(PERSON + 0x9c, bytes([person]))
        trace = []
        def snapshot(tag):
            trace.append(f'{tag}:{r.u.mem_read(TARGET, 0x114).hex()}:{r.read(0xa158c0)[0]}:{r.read(0xa158bc)[0]}:{r.u.mem_read(PERSON + 0x9c, 1)[0]};')
        def alert():
            slot, direction = r.read(r.u.reg_read(UC_X86_REG_ESP) + 4, 2)
            snapshot(f'a,{signed(slot)},{direction}')
            if mutation & 1:r.write(TARGET + 4, DEF + 0x100)
        def update():
            actor, message = r.read(r.u.reg_read(UC_X86_REG_ESP) + 4, 2)
            assert actor == TARGET and message == 1
            snapshot(f'u,{CALLBACK},{message}')
            if mutation & 2:r.write(TARGET + 0x74, 0x55667788)
        r.hook(0x44a3e0, alert);r.hook(CALLBACK, update);r.hook(0x484a20, lambda:snapshot('l'))
        code = r.call(0x464c50, PROJECTILE, TARGET, MATERIAL)
        expected.append(f'{code} {r.u.mem_read(TARGET, 0x114).hex()} {r.read(0xa158c0)[0]} {r.read(0xa158bc)[0]} {r.u.mem_read(PERSON + 0x9c, 1)[0]} {"".join(trace) or "-"}')
        r.clear_hooks();cases.append(row)
    match(args.probe, 'damage', cases, expected)
    counts['464C50'] = len(cases)

    # Bone matrices are declared foreign fixtures here. All ray tests, bone
    # order, ties, skips and writes execute in the original 0x463790 body.
    cases, expected = [], []
    for i in range(args.cases):
        q = [0, 0, 0, 0x80000, 0, 0, 65536, 0, 0, 0x80000, 0, 0x80000, 0, 0, 0, 0, rng.choice([0, 0x7cca80, 0x7ccb94])]
        material, record, mesh, face = [rng.getrandbits(32) for _ in range(4)]
        count = rng.randrange(5)
        row = [*q, material, record, mesh, face, count]
        r.query(q);r.write(QUERY + 0x50, material, 0, 0, record, mesh)
        r.write(0x8cfa18, count)
        states, calls = [], []
        for index in range(count):
            actor = bytearray(0x114)
            px, py, pz = rng.randint(-0x40000, 0xc0000), rng.randint(-0x30000, 0x30000), rng.randint(-0x30000, 0x30000)
            setword(actor, 0, rng.randrange(5) != 0);setword(actor, 0x24, px);setword(actor, 0x28, py);setword(actor, 0x2c, pz)
            actor[0x6c] = rng.choice([0, 0, 2]);setword(actor, 0x78, rng.randrange(9) == 0)
            graphic, n = rng.randrange(6) != 0, rng.choice([0, 3, 4, 6])
            instance = 0x2210000 + index * 0x10000
            setword(actor, 0x10, 1 if graphic else 0);setword(actor, 0x18, instance)
            r.u.mem_write(0x7cca80 + index * 0x114, bytes(actor))
            r.write(instance + 0x14, n);r.write(instance + 0x7c, instance + 0x100)
            spheres = []
            for bone in range(n):
                sphere = [rng.randint(-0x20000, 0xa0000), rng.randint(-0x10000, 0x10000), rng.randint(-0x10000, 0x10000), rng.randint(0, 0x20000)]
                spheres.append(sphere)
                r.write(instance + 0x100 + bone * 4, instance + 0x1000 + bone * 0x100)
                r.write(instance + 0x1000 + bone * 0x100 + 0x28, *sphere)
            states.append((instance, spheres));row.extend([actor.hex(), int(graphic), n, *[v for sphere in spheres for v in sphere]])
        current = [None]
        def world():
            destination, pose = r.read(r.u.reg_read(UC_X86_REG_ESP) + 4, 2)
            index = (pose - 0x7ccaa4) // 0x114
            calls.append(index);current[0] = index
        def bones():
            world_matrix, destination, instance, pitch = r.read(r.u.reg_read(UC_X86_REG_ESP) + 4, 4)
            assert instance == states[current[0]][0] and pitch == 0
            for bone in range(len(states[current[0]][1])):
                r.write(destination + bone * 0x40, 0x400000, 0, 0, 0, 0, 0x400000, 0, 0, 0, 0, 0x400000, 0)
        r.hook(0x45d1e0, world);r.hook(0x46bc00, bones)
        code = r.call(0x463790, QUERY)
        material_out, hit, distance, record_out, mesh_out = r.read(QUERY + 0x50, 5)
        expected.append(' '.join(map(str, [code, hit, distance, material_out, record_out, mesh_out, face, len(calls), *calls])))
        r.clear_hooks();cases.append(row)
    match(args.probe, 'bones', cases, expected)
    counts['463790_fixture_matrices'] = len(cases)

    r.call(0x477510)
    cases, expected = [], []
    model, graphic, slots, lod, hardpoints, matrix_callback = (0x220a000, 0x220b000, 0x220c000, 0x220d000, 0x220e000, 0x220f000)
    weapons, items = 0x2220000, 0x2230000
    identity = [0x400000, 0, 0, 0, 0, 0x400000, 0, 0, 0, 0, 0x400000, 0]
    r.write(0xad3ed0, weapons);r.write(0xc23810, items)
    r.write(model + 0x50, lod);r.write(lod + 0xa4, hardpoints)
    r.write(graphic + 0x4c, matrix_callback);r.write(graphic + 0x50, 0x12345678)
    for i in range(args.cases):
        actor = bytearray(rng.randbytes(0x114))
        weapon = 0 if i % 11 == 0 else 7
        model_present, graphic_present = i % 13 != 0, i % 17 != 0
        setword(actor, 0x94, weapon);setword(actor, 0x10, graphic if graphic_present else 0)
        for at in (0x24, 0x28, 0x2c):setword(actor, at, rng.randint(-0x400000, 0x400000))
        animated = i % 2 == 0
        actor[0x6c] = 4 if animated else 0
        setword(actor, 0x30, rng.choice([0, 0x40000000, 0x80000000, 0xc0000000]) if animated else rng.getrandbits(32))
        setword(actor, 0x34, rng.getrandbits(32));setword(actor, 0x38, 0 if animated else rng.getrandbits(32))
        slot, attachment = identity[:], identity[:]
        for matrix in (slot, attachment):
            for at in (3, 7, 11):matrix[at] = rng.randint(-0x20000, 0x20000)
        point = [rng.randint(-0x10000, 0x10000) for _ in range(3)]
        hardpoint = rng.randrange(4)
        row = [actor.hex(), model if model_present else 0, hardpoint, *slot, *attachment, *point]
        r.u.mem_write(TARGET, bytes(actor));r.write(slots + 0x1c0, *slot);r.write(0xc22b40, *attachment)
        r.write(weapons + weapon * 0xa8 + 0x90, hardpoint)
        r.write(weapons + weapon * 0xa8 + 0x98, 3)
        r.write(items + 3 * 0x13c + 0x6c, model if model_present else 0)
        r.write(hardpoints + hardpoint * 12, *point)
        calls, captured = [], []
        def matrix_provider():
            data, actor_at, mode = r.read(r.u.reg_read(UC_X86_REG_ESP) + 4, 3)
            assert (data, actor_at, mode) == (0x12345678, TARGET, 0)
            assert r.read(TARGET + 0x34)[0] == struct.unpack_from('<I', actor, 0x34)[0]
            calls.append('matrix');r.u.reg_write(UC_X86_REG_EAX, slots)
        def shot():
            actor_at, pose = r.read(r.u.reg_read(UC_X86_REG_ESP) + 4, 2)
            assert actor_at == TARGET
            captured.extend(r.read(pose, 6));calls.append('shot')
        r.hook(matrix_callback, matrix_provider);r.hook(0x45ea00, shot)
        r.call(0x45e7a0, TARGET)
        assert calls in ([], ['matrix', 'shot'])
        expected.append(' '.join(map(str, [int(bool(calls)), r.u.mem_read(TARGET, 0x114).hex(), len(calls),
                                           *r.read(slots + 0x1c0, 12), *captured])))
        r.clear_hooks();cases.append(row)
    match(args.probe, 'muzzle', cases, expected)
    counts['45E7A0'] = len(cases)

    # Runtime model matrices and callbacks are fixtures. Plane arithmetic and
    # the original 0x42D2A0 triangle test execute without replacements.
    cases, expected = [], []
    for i in range(args.cases):
        reverse = i % 3 == 0
        start, end = (0x8000, -0x8000) if reverse else (-0x8000, 0x8000)
        q = [start, 0, 0, end, 0, 0, -65536 if reverse else 65536, 0, 0, 0x10000,
             -0x8000, 0x8000, 0, 0, 0, 0, rng.choice([0, 0x7cca80, 0x7ccb94])]
        material, record, mesh, face = [rng.getrandbits(32) for _ in range(4)]
        count = rng.randrange(5)
        row = [*q, material, record, mesh, face, count]
        r.query(q);r.write(QUERY + 0x50, material, 0, 0, record, mesh)
        matrices, callbacks, material_faces, trace = {}, {}, {}, []
        for index in range(count):
            base = 0x2240000 + index * 0x20000
            actor = bytearray(0x114)
            setword(actor, 0, rng.randrange(6) != 0)
            for at, value in zip((0x24, 0x28, 0x2c), (0, rng.choice([0, 0, 0x10000, 0x900000]), 0)):
                setword(actor, at, value)
            actor[0x6c] = rng.choice([0, 0, 2, 8])
            model_present = rng.randrange(9) != 0
            setword(actor, 0x10, base if model_present else 0)
            setword(actor, 0x14, base if (actor[0x6c] & 8) else 0)
            radius, present, mesh_count = rng.choice([0, 0x10000, 0x20000]), rng.randrange(5) != 0, rng.randrange(4)
            row.extend([actor.hex(), base, radius, int(present), mesh_count])
            r.u.mem_write(0x7cca80 + index * 0x114, bytes(actor))
            r.write(base + 0x60, radius)
            def radius_read(u, access, address, size, value, _, model=base):
                trace.append(f'r,{model};')
            r.hooks.append(r.u.hook_add(uc.UC_HOOK_MEM_READ, radius_read, begin=base + 0x60, end=base + 0x60))
            r.write(base + 0x4c, base + 0x80);r.write(base + 0x50, base + 0x100)
            r.write(base + 0x100 + 0x98, mesh_count, base + 0x1000)
            callbacks[base + 0x80] = (base, base + 0x200 if present else 0)
            for mesh_index in range(mesh_count):
                state_at = base + 0x200 + mesh_index * 0x40
                mesh_at = base + 0x1000 + mesh_index * 0x70
                arena = base + 0x2000 + mesh_index * 0x4000
                flags = rng.choice([0, 0, 0, 1, 2, 4])
                matrix = identity[:]
                matrix[3] = rng.choice([0, 0, 0x1000, -0x1000])
                matrix[7] = rng.choice([0, 0x1000, 0x100000])
                r.u.mem_write(state_at + 0x3c, bytes([flags]));matrices[state_at] = matrix
                face_count = rng.randrange(1, 5)
                row.extend([flags, *matrix, face_count])
                r.write(mesh_at + 8, arena, face_count, arena + 0x1000)
                r.write(mesh_at + 0x18, arena + 0x2000)
                for face_index in range(face_count):
                    face_at, normal_at, material_at = arena + 0x1000 + face_index * 0x48, arena + 0x2000 + face_index * 8, arena + 0x3000 + face_index * 0x100
                    material_faces[material_at] = face_index
                    # A two-sided plane and triangle in original 24.8 units.
                    x = rng.choice([-64, 0, 0, 32, 64])
                    normal = rng.choice([(16384, 0, 0), (-16384, 0, 0)])
                    normal_flags = 4
                    plane = -normal[0] * 4 * x // 256
                    box = [x * 256, x * 256, -0x10000, 0x10000, -0x10000, 0x10000]
                    points = [(x, -256, -256), (x, 256, -256), (x, 0, 256)]
                    material_flags = rng.choice([0, 0, 0x01000000, 2, 0x800, 0x868c])
                    row.extend([plane, *box, *normal, normal_flags, material_flags, material_at,
                                *[n for point in points for n in point]])
                    r.u.mem_write(face_at, bytes(0x48));r.write(face_at, face_index << 16)
                    r.u.mem_write(face_at + 0x1c, struct.pack('<3H', *(face_index * 3 + n for n in range(3))))
                    r.write(face_at + 0x28, plane, *box, material_at)
                    r.u.mem_write(normal_at, struct.pack('<hhhH', *normal, normal_flags))
                    r.write(material_at + 0x10, material_flags)
                    for corner, point in enumerate(points):
                        r.u.mem_write(arena + (face_index * 3 + corner) * 8, struct.pack('<hhhH', *point, 0))
        def matrix_fixture():
            state_at, destination = r.read(r.u.reg_read(UC_X86_REG_ESP) + 4, 2)
            r.write(destination, *matrices[state_at])
        def model_provider(at):
            model, returned = callbacks[at]
            trace.append(f's,{model};')
            r.u.reg_write(UC_X86_REG_EAX, returned)
        for at in callbacks:
            r.hook(at, lambda at=at:model_provider(at))
        r.hook(0x45d170, matrix_fixture)
        def divide_message():
            values = r.read(r.u.reg_read(UC_X86_REG_ESP) + 4, 4)
            trace.append(f'd,{signed(values[2])},{signed(values[3])};')
        r.hook(0x493a80, divide_message);r.hook(0x4564b0, lambda:None)
        code = r.call(0x464400, 0x7cca80, count, QUERY)
        material_out, hit, distance, record_out, mesh_out = r.read(QUERY + 0x50, 5)
        face_out = material_faces[material_out] if hit else face
        expected.append(' '.join(map(str, [code, hit, distance, material_out, record_out, mesh_out, face_out, ''.join(trace) or '-'])))
        r.clear_hooks();cases.append(row)
    match(args.probe, 'model', cases, expected)
    counts['464400_fixture_matrices'] = len(cases)

    cases, expected = [], []
    entity = 0x9f3098
    for i in range(args.cases):
        impact_type = i % 5
        target = 0x7dde80 if impact_type == 2 else 0x7cca80 if impact_type == 3 else 0x867e80
        actor, owner = bytearray(rng.randbytes(0x114)), bytearray(rng.randbytes(0x114))
        setword(actor, 4, DEF if i % 19 else 0);setword(actor, 0xc, PERSON);setword(actor, 0x110, CALLBACK)
        actor[0x6c] = rng.choice([0, 1, 4, 0x80]);setword(actor, 0x70, 0xffff0002);setword(owner, 0x70, rng.randrange(4))
        age = rng.choice([0, 1, 8, 9, 100, MASK])
        ttl = rng.randint(1, 100);p = [rng.choice([0, 1, 100]), rng.randrange(34), (ttl + age) & MASK, ttl]
        flags = rng.randrange(256) | rng.choice([0, 0x100])
        definition = [rng.choice([0, 1, 3, 3]), rng.choice([0, 0x40, 0x80, 0xc0]), rng.randrange(35)]
        material = rng.choice([0, 0x10000, 0x20000, 0x40000, 0x80000, 0x4000000, 0x8000000, 0xc0f0000])
        kind = rng.choice([0, 1, 2, 3, 5])
        state = [rng.choice([0, 1]), rng.choice([0, 0x40]), rng.choice([0, target]),
                 1 if impact_type == 4 else rng.choice([0, 1]), rng.getrandbits(32), rng.getrandbits(32)]
        ownerid, mutation, person = rng.choice([0, OWNER, target]), rng.randrange(4), rng.randrange(256)
        row = [actor.hex(), owner.hex(), *p, flags, *definition, material, impact_type, kind, *state, ownerid, mutation, person]
        r.u.mem_write(target, bytes(actor));r.u.mem_write(OWNER, bytes(owner))
        r.u.mem_write(entity, bytes(0x22800));r.projectile(p, (65536, 0, 0), flags, ownerid)
        r.u.mem_write(entity, bytes(r.u.mem_read(PROJECTILE, 0x114)))
        r.write(entity, 1);r.write(entity + 0x24, 0x1000000, 0x2000000, 0x640000)
        r.write(DEF + 0x28, definition[0]);r.write(DEF + 0x24, definition[1]);r.write(DEF + 0x98, definition[2])
        r.write(MATERIAL + 0x10, material)
        for address, value in zip((0x67f528, 0x67f549, 0xc23820, 0xbd5248, 0xa158c0, 0xa158bc), state):r.write(address, value)
        r.write(0x67f520, 0);r.write(0x67f548, 0);r.write(0x6e4700, 0x7000000, 0x7000000, 0x7000000)
        # Byte rules at 67F549 follows the movement flag byte at 67F548.
        r.u.mem_write(0x67f549, bytes([state[1]]))
        r.u.mem_write(PERSON + 0x9c, bytes([person]));trace = []
        def snapshot(tag):
            trace.append(f'{tag}:{r.u.mem_read(target, 0x114).hex()}:{r.read(0xa158c0)[0]}:{r.read(0xa158bc)[0]}:{r.u.mem_read(PERSON + 0x9c, 1)[0]}:{r.read(entity + 0x6c)[0]}:{r.read(entity + 0xfc)[0]};')
        def alert():
            slot, direction = r.read(r.u.reg_read(UC_X86_REG_ESP) + 4, 2);snapshot(f'a,{signed(slot)},{direction}')
        def update():
            actor_at, message = r.read(r.u.reg_read(UC_X86_REG_ESP) + 4, 2)
            assert actor_at == target and message == 1
            snapshot(f'u,{CALLBACK},{message}')
            if mutation & 2:r.write(target + 0x74, 0x55667788)
        r.hook(0x44a3e0, alert);r.hook(CALLBACK, update);r.hook(0x484a20, lambda:snapshot('l'))
        def terrain():r.u.reg_write(UC_X86_REG_EAX, -32768 & MASK if impact_type == 0 else 1)
        def query_hit(selected, context):
            if selected:
                r.write(context + 0x50, MATERIAL, 1, 32768, 0, kind)
            r.u.reg_write(UC_X86_REG_EAX, 0 if selected else 1)
        def model_hit():
            base, count, context = r.read(r.u.reg_read(UC_X86_REG_ESP) + 4, 3)
            query_hit((impact_type == 1 and base == 0x867e80) or (impact_type == 2 and base == 0x7dde80), context)
        def special_hit(which):
            context = r.read(r.u.reg_read(UC_X86_REG_ESP) + 4)[0];query_hit(impact_type == which, context)
        r.hook(0x47e390, terrain);r.hook(0x464400, model_hit);r.hook(0x463790, lambda:special_hit(3));r.hook(0x464ba0, lambda:special_hit(4))
        def effect(site):
            esp = r.u.reg_read(UC_X86_REG_ESP)
            if site == 0x45dab0:
                argument, owner_arg = 0, 0;xyz = r.read(esp + 4, 3)
            elif site == 0x45fcf0:
                x, y, z, argument, zero, zero2 = r.read(esp + 4, 6)
                assert zero == zero2 == 0
                xyz, owner_arg = (x, y, z), 0
            else:
                argument, x, y, z = r.read(esp + 4, 4);xyz = (x, y, z)
                owner_arg = r.read(esp + 20)[0] if site == 0x4657d0 else 0
            snapshot(f'e,{site},{argument},{owner_arg},{xyz[0]},{xyz[1]},{xyz[2]}')
            if mutation & 1 and site == 0x45dc70:r.write(entity + 0x6c, r.read(entity + 0x6c)[0] ^ 0x20)
        for site in (0x45dab0, 0x45fcf0, 0x4657d0, 0x45dc70, 0x43e7d0, 0x460d20):r.hook(site, lambda site=site:effect(site))
        r.hook(0x463350, lambda:None);r.hook(0x464e00, lambda:None)
        r.call(0x464eb0)
        assert impact_type != 4 or trace, 'Water impact did not reach its effects'
        expected.append(f'{r.u.mem_read(target, 0x114).hex()} {r.read(0xa158c0)[0]} {r.read(0xa158bc)[0]} {r.u.mem_read(PERSON + 0x9c, 1)[0]} {r.read(entity + 0x6c)[0]} {"".join(trace) or "-"}')
        r.clear_hooks();cases.append(row)
    match(args.probe, 'impact', cases, expected)
    counts['464EB0_impact_routes'] = len(cases)

    character_hash = None
    if args.character:
        raw = args.character.read_bytes()
        character_hash = hashlib.sha256(raw).hexdigest()
        def rawword(at):
            return struct.unpack_from('<I', raw, at)[0]
        assert rawword(0) == 0x31524843
        bone_at, bone_count, map_at, map_count, animations_at = [rawword(n) for n in (4, 8, 12, 16, 20)]
        bones = []
        for index in range(bone_count):
            bones.append(bone_at)
            bone_at += 92 if rawword(bone_at + 24) & 1 else 60
        bone_map = [rawword(map_at + i * 4) for i in range(map_count)]
        animations = []
        for slot in range(255):
            at = rawword(animations_at + slot * 4)
            if not at:continue
            frames = []
            frame_at = at + 20
            assert rawword(at + 4) == map_count
            for _ in range(rawword(at)):
                frames.append(frame_at)
                frame_at += rawword(frame_at)
            animations.append(frames)
        instance, bone_table, pose_at, matrix_at, world_at, point_at = 0x2210000, 0x2211000, 0x2212000, 0x2213000, 0x2214000, 0x2215000
        r.write(instance + 0x14, map_count);r.write(instance + 0x7c, bone_table);r.write(instance + 0x80, pose_at)
        sphere_fields = []
        for index, bone_index in enumerate(bone_map):
            offset = bones[bone_index]
            bone_address = 0x2220000 + index * 0x100
            r.write(bone_table + index * 4, bone_address)
            fields = {}
            for at in (28, 32, 36, 40, 44, 48, 52):
                fields[at] = (signed(rawword(offset + at)) * 0x1d2b0) >> 16
                r.write(bone_address + at, fields[at])
            r.write(bone_address + 56, rawword(offset + 56))
            sphere_fields.append([fields[n] for n in (40, 44, 48, 52)])
        cases, expected = [], []
        for i in range(min(args.cases, 400)):
            actor = bytearray(0x114);setword(actor, 0x10, i % 11 != 0)
            for at in (0x24, 0x28, 0x2c):setword(actor, at, rng.randint(-0x400000, 0x400000))
            for at in (0x30, 0x34, 0x38):setword(actor, at, rng.getrandbits(32))
            animation = rng.randrange(len(animations));frame = rng.randrange(len(animations[animation]))
            at = animations[animation][frame] + 52
            for bone in range(map_count):
                r.write(pose_at + bone * 24, *(int(raw[at + bone * 4 + j]) << 17 for j in range(3)), 65536, 65536, 65536)
            r.u.mem_write(TARGET, bytes(actor));r.call(0x45d1e0, world_at, TARGET + 0x24)
            r.call(0x46bc00, world_at, matrix_at, instance, 0)
            values = [int(i % 11 != 0), map_count]
            for bone, fields in enumerate(sphere_fields):
                r.write(point_at, *fields[:3]);r.call(0x45cda0, matrix_at + bone * 0x40, point_at, point_at)
                values.extend([*r.read(point_at, 3), fields[3] & MASK])
            expected.append(' '.join(map(str, values)));cases.append([actor.hex(), animation, frame])
        match(args.probe, 'spheres', cases, expected, [args.character])
        counts['46BC00_stock_character_spheres'] = len(cases)

    report = {'source_sha256': HASH, 'character_sha256': character_hash, 'cases': counts,
              'foreign_fixtures': ['44A3E0 effect', 'actor +110 update effect', '484A20 local hit effect',
                                   '463790 world and bone matrices', '45E7A0 graphic slot provider and 45EA00 shot effect',
                                   '464400 runtime model provider and 45D170 matrices',
                                   '464EB0 query results, visual/sound/allocation effects and movement effects'],
              'angle_coverage': '464C50 axis velocities; 45E7A0 flagged aim uses quarter-turn yaw'}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--cases', type=int, default=1000)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--character', type=Path)
    run(parser.parse_args())
