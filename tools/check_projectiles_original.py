#!/usr/bin/env python3
"""Check the native projectile module against original DF1 instructions.

Replays generated cases on the original Df.exe code with unicorn and compares
them with tools/projectiles_probe output for the same inputs:

  477510 sine table build + 4776C0 interpolated sincos
  45C910/45C970/45CAE0/45CB40/45CDA0/45D1E0 matrix helpers
  482D90 muzzle origin + aim angles
  4847D0 barrel tip (item mesh offset input)
  45EA00 spread + kind plan (RNG and burst counter inputs)
  46FB40 spawn record build (ground callback stub)
  46EA40 spawn record decode (dispatcher frame, ground callback stub)
  462F20 projectile entity spawn (velocity, fields, item pointers)
  464EB0 per-tick motion core with scripted collision queries

Ballistic primitives (47E390 terrain step, 464400/463790/464BA0 hit
queries), effects (4657D0/45DC70/460D20/45FCF0/45DAB0), damage, sounds
(43E7D0) and the message queue (46F3E0) stay stubbed inputs.

Usage: check_projectiles_original.py GAME_FOLDER PROJECTILES_PROBE [--report R]
"""
import argparse
import hashlib
import json
import random
import struct
import subprocess
import sys
import unicorn
from pathlib import Path
import pefile
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_FPCW

SOURCE_SHA256 = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
BASE = 0x400000
STACK = 0x2008000
RET_MAGIC = 0x4000000
ENTITY = 0x9F3098
ENTITY_INDEX = 0xA158A4
ACTOR = 0x1200000
ACTOR_DEF = 0x1210000
SCRATCH = 0x1300000
PAYLOAD = 0x13F0000
PEOPLE = 0x7CCA80
PEOPLE_STRIDE = 0x114


class Rig:
    def __init__(self, game):
        raw = Path(game, 'Df.exe').read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        if digest != SOURCE_SHA256:
            raise SystemExit('Unsupported original executable hash: ' + digest)
        pe = pefile.PE(data=raw, fast_load=True)
        self.u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        self.u.mem_map(BASE, 0xA00000)
        self.u.mem_write(BASE, pe.get_memory_mapped_image())
        self.u.mem_map(0x1100000, 0x100000)
        self.u.mem_map(ACTOR & ~0xFFFFF, 0x100000)
        self.u.mem_map(SCRATCH, 0x100000)
        self.u.mem_map(0x2000000, 0x10000)
        self.events = []
        self.spawn_stub = False
        self.entity_stub = False
        self.ground = 0
        self.queries = {'terrain': 1, 'a': None, 'b': None, 's': None, 'x': None}
        self.stubs = {
            0x49A620: self.stub_memset,
            0x46F3E0: self.stub_queue,
            0x46FB40: self.stub_spawn_call,
            0x462F20: self.stub_entity_call,
            0x470B00: self.stub_melee,
            0x47DF40: self.stub_ground,
            0x47E390: self.stub_terrain,
            0x464400: self.stub_actor_hit,
            0x463790: self.stub_special_s,
            0x464BA0: self.stub_special_x,
            0x463350: self.stub_move,
            0x43E7D0: self.stub_sound,
            0x43E720: self.stub_void,
            0x43F4D0: self.stub_ret1,
            0x45DAB0: self.stub_void,
            0x45DC70: self.stub_void,
            0x460D20: self.stub_void,
            0x45FCF0: self.stub_void,
            0x4657D0: self.stub_void,
            0x43EF80: self.stub_void,
            0x45D6F0: self.stub_void,
            0x464C50: self.stub_void,
            0x46DAA3: self.stub_stop,
            0x493A80: self.stub_sprintf,
        }
        self.u.hook_add(unicorn.UC_HOOK_CODE, self.hook)

    # ---------- stubs ----------
    def finish(self, retval=0):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        ra = struct.unpack('<I', self.u.mem_read(sp, 4))[0]
        self.u.reg_write(UC_X86_REG_ESP, sp + 4)
        self.u.reg_write(UC_X86_REG_EAX, retval & 0xFFFFFFFF)
        self.u.reg_write(UC_X86_REG_EIP, ra)

    def stub_void(self): self.finish(0)
    def stub_ret1(self): self.finish(1)
    def stub_stop(self): self.u.emu_stop()

    def stub_memset(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        dst, val, size = struct.unpack('<3I', self.u.mem_read(sp + 4, 12))
        self.u.mem_write(dst, bytes([val & 0xFF]) * (size & 0xFFFFF))
        self.finish(dst)

    def stub_ground(self): self.finish(self.ground)

    def stub_queue(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        kind, payload, size = struct.unpack('<3I', self.u.mem_read(sp + 4, 12))
        self.events.append(('queue', kind, bytes(self.u.mem_read(payload, min(size, 64)))))
        self.finish(0)

    def stub_spawn_call(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        vector, actor, kind = struct.unpack('<3I', self.u.mem_read(sp + 4, 12))
        data = struct.unpack('<6i', self.u.mem_read(vector, 24))
        self.events.append(('spawn_call', kind, data))
        self.finish(0)

    def stub_entity_call(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        vector, actor, kind = struct.unpack('<3I', self.u.mem_read(sp + 4, 12))
        data = struct.unpack('<6i', self.u.mem_read(vector, 24))
        self.events.append(('entity_call', kind, data, actor))
        self.finish(0)

    def stub_melee(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        actor = struct.unpack('<I', self.u.mem_read(sp + 4, 4))[0]
        self.events.append(('melee', actor))
        self.finish(0)

    def stub_terrain(self): self.finish(self.queries['terrain'])

    def stub_actor_hit(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        array, count, point = struct.unpack('<3I', self.u.mem_read(sp + 4, 12))
        value = self.queries['a'] if array == 0x867E80 else self.queries['b']
        if value is None:
            self.finish(1)
            return
        self.u.mem_write(point + 0x58, struct.pack('<i', value))
        self.u.mem_write(point + 0x50, struct.pack('<I', PEOPLE))
        self.u.mem_write(point + 0x5C, struct.pack('<i', 0))
        self.finish(0)

    def stub_special_s(self): self.special('s')
    def stub_special_x(self): self.special('x')

    def special(self, key):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        point = struct.unpack('<I', self.u.mem_read(sp + 4, 4))[0]
        value = self.queries[key]
        if value is None:
            self.finish(1)
            return
        self.u.mem_write(point + 0x58, struct.pack('<i', value))
        self.u.mem_write(point + 0x50, struct.pack('<I', PEOPLE))
        self.u.mem_write(point + 0x5C, struct.pack('<i', 0))
        self.finish(0)

    def stub_move(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        args = struct.unpack('<7i', self.u.mem_read(sp + 4, 28))
        self.events.append(('move', args))
        self.finish(0)

    def stub_sound(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        sid, x, y, z = struct.unpack('<4i', self.u.mem_read(sp + 4, 16))
        self.events.append(('sound', sid, x, y, z))
        self.finish(0)

    def stub_sprintf(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        dst = struct.unpack('<I', self.u.mem_read(sp + 4, 4))[0]
        self.u.mem_write(dst, b'text\0')
        self.finish(dst)

    def hook(self, machine, addr, size, user):
        if addr == RET_MAGIC:
            machine.emu_stop()
            return
        if addr == 0x46FB40 and not self.spawn_stub:
            return
        if addr == 0x462F20 and not self.entity_stub:
            return
        fn = self.stubs.get(addr)
        if fn:
            fn()

    # ---------- helpers ----------
    def poke(self, addr, value):
        self.u.mem_write(addr, struct.pack('<i', value))

    def poke_u(self, addr, value):
        self.u.mem_write(addr, struct.pack('<I', value & 0xFFFFFFFF))

    def peek(self, addr):
        return struct.unpack('<i', self.u.mem_read(addr, 4))[0]

    def call(self, fn, *args, count=4000000):
        sp = STACK
        stack = struct.pack('<I', RET_MAGIC) + b''.join(
            struct.pack('<I', a & 0xFFFFFFFF) for a in args)
        self.u.mem_write(sp, stack)
        self.u.reg_write(UC_X86_REG_ESP, sp)
        self.u.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.u.emu_start(fn, RET_MAGIC, count=count)
        if self.u.reg_read(UC_X86_REG_EIP) != RET_MAGIC:
            raise RuntimeError(f"Original 0x{fn:x} did not return")

    def call_frame(self, fn, count=4000000):
        self.u.reg_write(UC_X86_REG_ESP, STACK)
        self.u.reg_write(UC_X86_REG_FPCW, 0x037f)
        self.u.emu_start(fn, STACK + 0x1000, count=count)
        if self.u.reg_read(UC_X86_REG_EIP) != 0x46DAA3:
            raise RuntimeError(f"Original decode did not reach its stop")

    def take_events(self):
        events = self.events
        self.events = []
        return events

    def setup(self):
        self.u.mem_write(ACTOR & ~0xFFFFF, b'\0' * 0x100000)
        self.u.mem_write(SCRATCH, b'\0' * 0x100000)
        self.u.mem_write(ENTITY, b'\0' * 0x22800)
        self.poke(ENTITY_INDEX, 0)
        self.poke_u(0x00AD3ED0, SCRATCH + 0x4000)
        self.poke_u(0x00AD3EDC, 39)
        self.poke_u(0x00AD3EE4, SCRATCH + 0x6000)
        self.poke_u(0x00AD3EE0, 28)
        self.poke_u(0x00C23810, SCRATCH + 0x8000)
        self.poke_u(0x00C23814, 4)
        for index, ptr in enumerate((0xA158A8, 0xA158AC, 0xA158B0, 0xA158B4, 0xA158B8, 0xA1589C)):
            self.poke_u(ptr, SCRATCH + 0x9000 + index * 0x10)
        self.poke(0x0067F520, 0)
        self.poke(0x0067F549, 0)
        self.poke(0x00BD5248, 0)
        self.poke_u(0x0067DBC8, 0)
        self.call(0x477510)  # sine table build

    def weapon_record(self, muzzle, aim, aim_field, item_id, weapon_class, index=7):
        record = SCRATCH + 0x4000 + index * 0xA8
        for offset, value in zip((0x78, 0x7C, 0x80), muzzle): self.poke(record + offset, value)
        for offset, value in zip((0x84, 0x88, 0x8C), aim): self.poke(record + offset, value)
        self.poke(record + 0x90, aim_field)
        self.poke(record + 0x94, item_id)
        self.poke(record + 0x74, weapon_class)
        return record

    def class_record(self, index, spread, speed, f10, f14, f18, flags):
        record = SCRATCH + 0x6000 + index * 0x34
        self.poke(record + 0x0C, speed)
        self.poke(record + 0x10, f10)
        self.poke(record + 0x14, f14)
        self.poke(record + 0x18, f18)
        self.poke(record + 0x1C, spread)
        self.poke(record + 0x30, flags)
        return record


def run_probe(probe, script):
    out = subprocess.check_output([str(probe)], input=script.encode())
    return [json.loads(line) for line in out.decode().splitlines() if line.strip()]


class Checker:
    def __init__(self, probe):
        self.probe = probe
        self.report = {'source_sha256': SOURCE_SHA256, 'cases_matched': 0, 'cases_compared': 0,
                       'mismatches': [], 'original_functions': [
                           '477510', '4776C0', '45C910', '45C970', '45CAE0', '45CB40', '45CDA0',
                           '45D1E0', '482D90', '4847D0', '45EA00', '46FB40', '46EA40', '462F20',
                           '464EB0 motion core', '464E00', '463350', '477620'],
                       'stubbed_functions': {
                           '47E390': 'terrain step query -> scripted',
                           '464400': 'actor hit query -> scripted',
                           '463790/464BA0': 'special hit queries -> scripted',
                           '47DF40': 'ground height -> scripted constant',
                           '46F3E0': 'message queue -> captured',
                           '462F20': 'entity spawn -> captured in the decode test',
                           '46FB40': 'spawn call -> captured in the plan test',
                           '470B00': 'melee -> captured',
                           '4657D0/45DC70/460D20/45FCF0/45DAB0/43EF80': 'impact effects -> no-op',
                           '43E7D0': 'sound -> captured (0x41 = world edge)',
                           '43F4D0': 'sound helper -> 1'}}

    def battery(self, name, script_lines, original_rows, key_map=None):
        native = run_probe(self.probe, ''.join(script_lines))
        if len(native) != len(original_rows):
            self.report['mismatches'].append({'case': name, 'error': 'count'})
            print(f'{name}: COUNT MISMATCH {len(native)} vs {len(original_rows)}')
            return
        bad = 0
        for index, (a, b) in enumerate(zip(native, original_rows)):
            if key_map:
                b = key_map(b)
            if a != b:
                bad += 1
                if bad <= 2:
                    self.report['mismatches'].append({'case': f'{name}[{index}]', 'native': a, 'original': b})
                    print(f'{name}[{index}] MISMATCH')
                    print('  native  :', json.dumps(a))
                    print('  original:', json.dumps(b))
        self.report['cases_compared'] += len(native)
        if bad == 0:
            self.report['cases_matched'] += 1
            print(f'{name}: {len(native)} cases match')


def matrix_row(rig, address):
    return list(struct.unpack('<12i', rig.u.mem_read(address, 48)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    rig = Rig(args.game)
    rig.setup()
    random.seed(20261016)
    checker = Checker(args.probe)

    table = list(struct.unpack('<2561i', rig.u.mem_read(0xbd0068, 2561*4)))
    checker.battery('sine_table', ['sine_table\n'], [{'op':'sine_table','values':table}])

    # ---- sincos ----
    angles = [0, 0x20000000, 0x10000000, 0x40000000, 0x80000000, 0xFFFFFFFF, 0xFFFF0000,
              0x12345678, 0x87654321, 0x00010000, 0x001FFFFF, 0x7FFFF000] + \
             [random.randrange(0, 2**32) for _ in range(52)]
    rows = []
    for angle in angles:
        rig.call(0x4776C0, angle, SCRATCH + 0x2000, SCRATCH + 0x2004)
        rows.append({'op': 'sincos', 'sin': rig.peek(SCRATCH + 0x2000), 'cos': rig.peek(SCRATCH + 0x2004)})
    checker.battery('sincos', [f'sincos {a}\n' for a in angles], rows)

    # ---- rotation / diagonal / multiply / point / pose ----
    cases = [(random.randrange(0, 3), random.randrange(-0x4000000, 0x4000000),
              random.randrange(-0x4000000, 0x4000000)) for _ in range(24)]
    rows = []
    for axis, first, second in cases:
        rig.call(0x45C970, SCRATCH + 0x2100, axis, first, second)
        rows.append({'op': 'rot', 'm': matrix_row(rig, SCRATCH + 0x2100)})
    checker.battery('rotation', [f'rot {a} {b} {c}\n' for a, b, c in cases], rows)

    cases = [(random.randrange(-0x4000000, 0x4000000), random.randrange(-0x4000000, 0x4000000),
              random.randrange(-0x4000000, 0x4000000)) for _ in range(16)]
    rows = []
    for x, y, z in cases:
        rig.call(0x45CAE0, SCRATCH + 0x2100, x, y, z)
        rows.append({'op': 'diag', 'm': matrix_row(rig, SCRATCH + 0x2100)})
    checker.battery('diagonal', [f'diag {x} {y} {z}\n' for x, y, z in cases], rows)

    mul_cases = []
    for _ in range(16):
        a = [random.randrange(-0x4000000, 0x4000000) for _ in range(12)]
        b = [random.randrange(-0x4000000, 0x4000000) for _ in range(12)]
        mul_cases.append((a, b))
    rows = []
    for a, b in mul_cases:
        rig.u.mem_write(SCRATCH + 0x2400, struct.pack('<12i', *a))
        rig.u.mem_write(SCRATCH + 0x2500, struct.pack('<12i', *b))
        rig.call(0x45CB40, SCRATCH + 0x2400, SCRATCH + 0x2500, SCRATCH + 0x2600)
        rows.append({'op': 'mul', 'm': matrix_row(rig, SCRATCH + 0x2600)})
    checker.battery('multiply', ['mul ' + ' '.join(map(str, a + b)) + '\n' for a, b in mul_cases], rows)

    point_cases = []
    for _ in range(16):
        m = [random.randrange(-0x4000000, 0x4000000) for _ in range(12)]
        p = [random.randrange(-0x4000000, 0x4000000) for _ in range(3)]
        point_cases.append((m, p))
    rows = []
    for m, p in point_cases:
        rig.u.mem_write(SCRATCH + 0x2400, struct.pack('<12i', *m))
        rig.u.mem_write(SCRATCH + 0x2480, struct.pack('<3i', *p))
        rig.call(0x45CDA0, SCRATCH + 0x2400, SCRATCH + 0x2480, SCRATCH + 0x2490)
        x, y, z = struct.unpack('<3i', rig.u.mem_read(SCRATCH + 0x2490, 12))
        rows.append({'op': 'point', 'x': x, 'y': y, 'z': z})
    checker.battery('point', ['point ' + ' '.join(map(str, m + p)) + '\n' for m, p in point_cases], rows)

    pose_cases = []
    for _ in range(16):
        position = [random.randrange(-0x4000000, 0x4000000) for _ in range(3)]
        angles = [random.randrange(0, 2**32) for _ in range(3)]
        pose_cases.append((position, angles))
    rows = []
    for position, angles in pose_cases:
        rig.u.mem_write(SCRATCH + 0x2400, struct.pack('<3i3I', *position, *angles))
        rig.call(0x45D1E0, SCRATCH + 0x2500, SCRATCH + 0x2400)
        rows.append({'op': 'pose', 'm': matrix_row(rig, SCRATCH + 0x2500)})
    checker.battery('pose', ['pose ' + ' '.join(map(str, p + a)) + '\n' for p, a in pose_cases], rows)

    # ---- muzzle ----
    muzzle_cases = [([1000,2000,3000],[0,0,0],[10,-20,30],[2,-3,4],[0,0,0],0xB333)]
    for _ in range(16):
        actor_pos = [random.randrange(-0x2000000, 0x2000000) for _ in range(3)]
        actor_angles = [random.randrange(0, 2**32) for _ in range(3)]
        view = [random.randrange(-0x10000, 0x10000) for _ in range(3)]
        weapon_muzzle = [random.randrange(-500, 500) for _ in range(3)]
        weapon_aim = [random.randrange(-0x100000, 0x100000) for _ in range(3)]
        eye = random.randrange(0, 0x20000)
        muzzle_cases.append((actor_pos, actor_angles, view, weapon_muzzle, weapon_aim, eye))
    rows = []
    for actor_pos, actor_angles, view, weapon_muzzle, weapon_aim, eye in muzzle_cases:
        rig.weapon_record(weapon_muzzle, weapon_aim, 0, 0, 7)
        rig.poke(0x00C239B8, 0)
        rig.poke(0x00C239C0, 7)
        rig.poke_u(0x00C23820, ACTOR)
        for address, value in zip((ACTOR + 0x24, ACTOR + 0x28, ACTOR + 0x2C), actor_pos): rig.poke(address, value)
        for address, value in zip((ACTOR + 0x30, ACTOR + 0x34, ACTOR + 0x38), actor_angles): rig.poke_u(address, value)
        for offset, value in zip((0xE8, 0xEC, 0xF0), view): rig.poke(0x00C23900 + offset, value)
        rig.poke(0x00C23A18, eye)
        rig.call(0x482D90)
        rows.append({'op': 'muzzle', 'x': rig.peek(0x00C23A74), 'y': rig.peek(0x00C23A78),
                     'z': rig.peek(0x00C23A7C), 'yaw': rig.peek(0x00C23A80) & 0xFFFFFFFF,
                     'pitch': rig.peek(0x00C23A84) & 0xFFFFFFFF, 'roll': rig.peek(0x00C23A88) & 0xFFFFFFFF})
    lines = []
    for actor_pos, actor_angles, view, weapon_muzzle, weapon_aim, eye in muzzle_cases:
        lines.append('muzzle ' + ' '.join(map(str, actor_pos + actor_angles + view + weapon_muzzle + weapon_aim + [eye])) + '\n')
    checker.battery('muzzle', lines, rows)

    # ---- barrel tip ----
    barrel_cases = []
    for _ in range(16):
        muzzle = [random.randrange(-0x2000000, 0x2000000) for _ in range(3)]
        aim = [random.randrange(0, 2**32) for _ in range(3)]
        offset = [random.randrange(-0x100000, 0x100000) for _ in range(3)]
        barrel_cases.append((muzzle, aim, offset))
    rows = []
    for muzzle, aim, offset in barrel_cases:
        rig.weapon_record([0, 0, 0], [0, 0, 0], 2, 1, 7)
        rig.poke(0x00C239B8, 0)
        rig.poke(0x00C239C0, 7)
        for address, value in zip((0x00C23A74, 0x00C23A78, 0x00C23A7C), muzzle): rig.poke(address, value)
        for address, value in zip((0x00C23A80, 0x00C23A84, 0x00C23A88), aim): rig.poke_u(address, value)
        item = SCRATCH + 0x8000 + 1 * 0x13C
        rig.poke_u(item + 0x6C, SCRATCH + 0xA000)
        rig.poke_u(SCRATCH + 0xA000 + 0x50, SCRATCH + 0xA100)
        rig.poke_u(SCRATCH + 0xA100 + 0xA4, SCRATCH + 0xA200)
        rig.u.mem_write(SCRATCH + 0xA200 + 2 * 12, struct.pack('<3i', *offset))
        rig.call(0x4847D0, SCRATCH + 0x2A00, SCRATCH + 0x2A04, SCRATCH + 0x2A08)
        x, y, z = struct.unpack('<3i', rig.u.mem_read(SCRATCH + 0x2A00, 12))
        rows.append({'op': 'barrel', 'x': x, 'y': y, 'z': z})
    checker.battery('barrel', ['barrel ' + ' '.join(map(str, m + a + o)) + '\n' for m, a, o in barrel_cases], rows)

    # ---- plan (spread + kind) ----
    plan_cases = []
    for _ in range(40):
        weapon = random.choice([0, 1, 4, 5, 6, 7, 8, 9, 10, 12, 13, 15, 20, 21, 22, 28, 32, 33, 35, 36, 37])
        weapon_class = random.randrange(0, 28)
        burst = random.choice([0, 0, 1, 2])
        spread = random.choice([1, 1, 3, 4, 5, 8])
        seed = random.randrange(1, 0xFFFFFFFF)
        yaw = random.randrange(0, 2**32)
        pitch = random.randrange(0, 2**32)
        plan_cases.append((weapon, weapon_class, burst, spread, seed, yaw, pitch))
    rows = []
    for weapon, weapon_class, burst, spread, seed, yaw, pitch in plan_cases:
        rig.weapon_record([0, 0, 0], [0, 0, 0], 0, 0, weapon_class, weapon)
        rig.poke_u(0x00C23820, ACTOR)
        rig.poke(ACTOR + 0x94, weapon)
        rig.spawn_stub = True
        rig.poke_u(ACTOR + 0xC, ACTOR_DEF)
        rig.poke(ACTOR_DEF + 0x6C, burst)
        rig.class_record(weapon_class, spread, 0, 0, 0, 0, 0)
        rig.call(0x477850, seed)
        rig.u.mem_write(SCRATCH + 0x2400, struct.pack('<3i3I', 0, 0, 0, yaw, pitch, 0))
        rig.call(0x45EA00, ACTOR, SCRATCH + 0x2400)
        events = rig.take_events()
        spawns = [e for e in events if e[0] == 'spawn_call']
        melee = [e for e in events if e[0] == 'melee']
        vector = struct.unpack('<6i', rig.u.mem_read(SCRATCH + 0x2400, 24))
        rows.append({'op': 'plan', 'kind': int(spawns[0][1]) if spawns else 0,
                     'spawns': bool(spawns), 'melee': bool(melee),
                     'burst': rig.peek(ACTOR_DEF + 0x6C) & 0xFFFFFFFF,
                     'yaw': vector[3] & 0xFFFFFFFF, 'pitch': vector[4] & 0xFFFFFFFF,
                     'rng': rig.peek(0x00BD0060) & 0xFFFFFFFF})
    checker.battery('plan', ['plan ' + ' '.join(map(str, c)) + '\n' for c in plan_cases], rows)

    # ---- spawn record build (46FB40) ----
    record_cases = []
    for _ in range(24):
        origin = [random.randrange(-0x4000000, 0x4000000) for _ in range(3)]
        yaw = random.randrange(0, 2**32)
        pitch = random.randrange(0, 2**32)
        slot = random.randrange(0, 64)
        kind = random.randrange(0, 0x20)
        ground = random.randrange(-0x1000000, 0x1000000)
        record_cases.append((origin, yaw, pitch, slot, kind, ground))
    rig.spawn_stub = False
    rows = []
    for origin, yaw, pitch, slot, kind, ground in record_cases:
        rig.ground = ground
        actor = PEOPLE + slot * PEOPLE_STRIDE
        rig.u.mem_write(SCRATCH + 0x2400, struct.pack('<3i3I', origin[0], origin[1], origin[2], yaw, pitch, 0))
        rig.call(0x46FB40, SCRATCH + 0x2400, actor, kind)
        events = rig.take_events()
        queue = [e for e in events if e[0] == 'queue']
        payload = queue[0][2] if queue else b''
        px, py = struct.unpack_from('<ii', payload, 0)
        rows.append({'op': 'record', 'px': px, 'py': py,
                     'slot': struct.unpack_from('<H', payload, 8)[0],
                     'yaw': struct.unpack_from('<H', payload, 10)[0],
                     'pitch': struct.unpack_from('<H', payload, 12)[0],
                     'kind': payload[14]})
    lines = []
    for origin, yaw, pitch, slot, kind, ground in record_cases:
        lines.append('record ' + ' '.join(map(str, origin + [yaw, pitch, slot, kind, ground])) + '\n')
    checker.battery('record', lines, rows)

    # ---- spawn record decode (46EA40) ----
    decode_cases = []
    for _ in range(24):
        px = random.randrange(-0x4000000, 0x4000000)
        py = random.randrange(-0x4000000, 0x4000000)
        slot = random.randrange(0, 64)
        yaw_word = random.randrange(0, 0x10000)
        pitch_word = random.randrange(0, 0x10000)
        kind = random.randrange(0, 0x20)
        ground = random.randrange(-0x1000000, 0x1000000)
        decode_cases.append((px, py, slot, yaw_word, pitch_word, kind, ground))
    rig.entity_stub = True
    rows = []
    for px, py, slot, yaw_word, pitch_word, kind, ground in decode_cases:
        rig.ground = ground
        rig.poke(PEOPLE + slot * PEOPLE_STRIDE, 1)
        rig.u.mem_write(PAYLOAD, struct.pack('<iiHHHB', px, py, slot, yaw_word, pitch_word, kind) + b'\0')
        rig.u.mem_write(STACK + 0x44, struct.pack('<I', PAYLOAD))
        rig.u.mem_write(STACK + 0x80, struct.pack('<I', 9))
        rig.call_frame(0x46EA40)
        events = rig.take_events()
        entity_calls = [e for e in events if e[0] == 'entity_call']
        data = entity_calls[0][2] if entity_calls else (0,) * 6
        rows.append({'op': 'decode', 'x': data[0], 'y': data[1], 'z': data[2],
                     'yaw': data[3] & 0xFFFFFFFF, 'pitch': data[4] & 0xFFFFFFFF})
    lines = []
    for px, py, slot, yaw_word, pitch_word, kind, ground in decode_cases:
        lines.append('decode ' + ' '.join(map(str, [px, py, slot, yaw_word, pitch_word, kind, ground])) + '\n')
    checker.battery('decode', lines, rows)

    # ---- entity spawn (462F20) ----
    spawn_cases = []
    for _ in range(32):
        origin = [random.randrange(-0x4000000, 0x4000000) for _ in range(3)]
        aim = [random.randrange(0, 2**32) for _ in range(3)]
        speed = random.choice([20, 72, 250, 295, 950, 1000, 1200])
        f10, f14, f18 = (random.randrange(0, 0x10000) for _ in range(3))
        flags = random.choice([0, 1, 0x10, 0x40])
        items = (SCRATCH + 0x9000, SCRATCH + 0x9010)
        item_field = random.randrange(0, 0x10000)
        team = random.choice([0, 1, 2])
        gbit = random.choice([False, True])
        spawn_cases.append((origin, aim, speed, f10, f14, f18, flags, item_field, team, gbit))
    for flags in (0x11,0x41,0x50,0x51):
        for team in (1,2):
            for gbit in (False,True):
                spawn_cases.append(([0,0,65536],[0,0,0],1000,3,8,10,flags,7,team,gbit))
    def item_pair(flags):
        if flags & 0x01: return (SCRATCH + 0x9030, SCRATCH + 0x9010)
        if flags & 0x10: return (SCRATCH + 0x9040, SCRATCH + 0x9000)
        if flags & 0x40: return (SCRATCH + 0x9020, SCRATCH + 0x9050)
        return (0, 0)
    rig.entity_stub = False
    rows = []
    for origin, aim, speed, f10, f14, f18, flags, item_field, team, gbit in spawn_cases:
        rig.class_record(3, 1, speed, f10, f14, f18, flags)
        for pointer in range(6):
            rig.poke(SCRATCH + 0x9000 + pointer * 0x10 + 0x6C, item_field)
        rig.poke(ACTOR + 0x70, team)
        rig.poke_u(0x008CF982, 0x10 if gbit else 0)
        rig.poke_u(0x00C23820, ACTOR)
        rig.poke_u(ACTOR + 0x94, 7)
        rig.u.mem_write(SCRATCH + 0x2400, struct.pack('<3i3I', origin[0], origin[1], origin[2], aim[0], aim[1], aim[2]))
        rig.poke(ENTITY_INDEX, 0)
        rig.u.mem_write(ENTITY, b'\0' * 0x22800)
        rig.call(0x462F20, SCRATCH + 0x2400, ACTOR, 3)
        index = rig.peek(ENTITY_INDEX)
        record = ENTITY + index * 0x114
        rows.append({
            'op': 'spawn', 'x': rig.peek(record + 0x24), 'y': rig.peek(record + 0x28),
            'z': rig.peek(record + 0x2C), 'yaw': rig.peek(record + 0x30) & 0xFFFFFFFF,
            'pitch': rig.peek(record + 0x34) & 0xFFFFFFFF,
            'vx': rig.peek(record + 0x3C), 'vy': rig.peek(record + 0x40), 'vz': rig.peek(record + 0x44),
            'ttl': rig.peek(record + 0xFC), 'item0': rig.peek(record + 0x00),
            'item1': rig.peek(record + 0x04), 'itemfield': rig.peek(record + 0x10),
            'color': rig.peek(record + 0x88), 'flags': rig.peek(record + 0x6C),
            'f7c': rig.peek(record + 0x7C), 'f80': rig.peek(record + 0x80),
            'f84': rig.peek(record + 0x84)})
    lines = []
    for origin, aim, speed, f10, f14, f18, flags, item_field, team, gbit in spawn_cases:
        team_color = 0xB9A0A0FF if team == 1 else (0xB9FFA0A0 if team == 2 else 0xFFFFFFFF)
        items = item_pair(flags)
        lines.append('spawn ' + ' '.join(map(str, origin + aim + [speed, f10, f14, f18, flags,
                                                                  items[0], items[1], item_field, 0,
                                                                  team_color, 1 if gbit else 0])) + '\n')
    checker.battery('spawn', lines, rows)

    # ---- per-tick motion (464EB0) ----
    def tick_scenario(name, entity, world, steps, special_b=False, alt_move=False):
        script = ['entity ' + ' '.join(map(str, entity)) +
                  f' {1 if special_b else 0} {1 if alt_move else 0}\n',
                  'world ' + ' '.join(map(str, world)) + '\n',
                  f'tick {len(steps)}\n']
        for step in steps:
            terrain = step.get('terrain', 1)
            def token(value):
                return 'none' if value is None else str(value)
            script.append(f'q {terrain} {token(step.get("a"))} {token(step.get("b"))} '
                          f'{token(step.get("s"))} {token(step.get("x"))} {step.get("action",0)}\n')
        native = run_probe(args.probe, ''.join(script))
        original = []
        for step in steps:
            rig.queries = {'terrain': step.get('terrain', 1), 'a': step.get('a'),
                           'b': step.get('b'), 's': step.get('s'), 'x': step.get('x')}
            rig.poke(0x00BD5248, 1 if special_b else 0)
            rig.poke(0x0067F549, 0x10 if alt_move else 0)
            x, y, z, vx, vy, vz, ttl, flags = entity
            record = ENTITY
            rig.poke(record + 0x00, 0x1000)
            for offset, value in zip((0x24, 0x28, 0x2C), (x, y, z)): rig.poke(record + offset, value)
            for offset, value in zip((0x3C, 0x40, 0x44), (vx, vy, vz)): rig.poke(record + offset, value)
            rig.poke(record + 0x6C, flags)
            rig.poke(record + 0x88, 0)
            rig.poke(record + 0xFC, ttl)
            rig.poke(record + 0xF8, 0)
            rig.poke_u(0x006E4708, world[0] & 0xFFFFFFFF)
            rig.poke_u(0x006E4704, world[1] & 0xFFFFFFFF)
            rig.poke_u(0x006E4700, world[2] & 0xFFFFFFFF)
            def impact_mutation():
                if step.get('action')==1:rig.poke(0x67f549,0x10)
                if step.get('action')==2:
                    for at,value in zip((0x24,0x28,0x2c),(123,456,789)):rig.poke(record+at,value)
                rig.finish(0)
            def move_mutation():
                rig.stub_move()
                if step.get('action')==3:
                    for at in (0x6e4708,0x6e4704,0x6e4700):rig.poke(at,0)
                    for at in (0x24,0x28,0x2c):rig.poke(record+at,0)
            def terrain_mutation():
                if step.get('action')==4:
                    for at,value in zip((0x24,0x28,0x2c),(123,456,789)):rig.poke(record+at,value)
                    for at,value in zip((0x3c,0x40,0x44),(1000,2000,3000)):rig.poke(record+at,value)
                rig.finish(rig.queries['terrain'])
            rig.stubs[0x47e390]=terrain_mutation
            rig.stubs[0x45dab0]=impact_mutation;rig.stubs[0x463350]=move_mutation
            rig.call(0x464EB0)
            events = rig.take_events()
            mapped = []
            for event in events:
                if event[0] == 'move':
                    move_args = event[1]
                    mapped.append(['move', 0, move_args[3], move_args[4], move_args[5],
                                   move_args[0], move_args[1], move_args[2]])
                elif event[0] == 'sound' and event[1] == 0x41:
                    mapped.append(['edge', 0x41, event[2], event[3], event[4], 0, 0, 0])
            original.append({'op': 'tick', 'x': rig.peek(record + 0x24), 'y': rig.peek(record + 0x28),
                             'z': rig.peek(record + 0x2C), 'vx': rig.peek(record + 0x3C),
                             'vy': rig.peek(record + 0x40), 'vz': rig.peek(record + 0x44),
                             'ttl': rig.peek(record + 0xFC), 'item': rig.peek(record + 0x00),
                             'color': rig.peek(record + 0x88), 'events': mapped})
            entity = (rig.peek(record + 0x24), rig.peek(record + 0x28), rig.peek(record + 0x2C),
                      rig.peek(record + 0x3C), rig.peek(record + 0x40), rig.peek(record + 0x44),
                      rig.peek(record + 0xFC), flags)
        for index, row in enumerate(original):
            row['op'] = f'tick{index}'
        native_fixed = []
        for row in native:
            events = [e for e in row['events'] if e[0] != 'impact']
            native_fixed.append({'op': row['op'], 'x': row['x'], 'y': row['y'], 'z': row['z'],
                                 'vx': row['vx'], 'vy': row['vy'], 'vz': row['vz'], 'ttl': row['ttl'],
                                 'item': row['item'], 'color': row['color'], 'events': events})
        bad = 0
        if len(native_fixed)!=len(original):raise ValueError('tick probe row count differs')
        for index, (a, b) in enumerate(zip(native_fixed, original)):
            if a != b:
                bad += 1
                if bad <= 2:
                    checker.report['mismatches'].append({'case': f'{name}[{index}]', 'native': a, 'original': b})
                    print(f'{name}[{index}] MISMATCH')
                    print('  native  :', json.dumps(a))
                    print('  original:', json.dumps(b))
        checker.report['cases_compared'] += len(original)
        if bad == 0:
            checker.report['cases_matched'] += 1
            print(f'{name}: {len(original)} ticks match')

    base_entity = (0x1000000, 0x2000000, 0x640000, 1057032, 0, 0, 50, 0)
    tick_scenario('tick_straight', base_entity, (0x4000000, 0x4000000, 0),
                  [{} for _ in range(12)])
    tick_scenario('tick_gravity', (0x1000000, 0x2000000, 0x640000, 0, 0, 0, 8, 0),
                  (0x4000000, 0x4000000, 0), [{} for _ in range(6)])
    tick_scenario('tick_diagonal', (0x1000000, 0x2000000, 0x640000, 500000, -300000, 200000, 20, 0),
                  (0x4000000, 0x4000000, 0), [{} for _ in range(10)])
    tick_scenario('tick_edge', (-100, 0, 0x640000, 1057032, 0, 0, 10, 0), (0, 0, 0),
                  [{} for _ in range(4)])
    tick_scenario('tick_no_gravity', (0x1000000, 0x2000000, 0x640000, 0, 0, 0, 5, 0x40),
                  (0x4000000, 0x4000000, 0), [{} for _ in range(4)])
    tick_scenario('tick_no_move', (0x1000000, 0x2000000, 0x640000, 1057032, 0, 0, 6, 0x90),
                  (0x4000000, 0x4000000, 0), [{} for _ in range(4)])
    tick_scenario('tick_alt_move', (0x1000000, 0x2000000, 0x640000, 1057032, 0, 0, 6, 0x20),
                  (0x4000000, 0x4000000, 0), [{} for _ in range(4)], alt_move=True)
    tick_scenario('tick_terrain_hit', (0x1000000, 0x2000000, 0x640000, 1057032, 0, 0, 10, 0),
                  (0x4000000, 0x4000000, 0),
                  [{'terrain': -100000}] + [{} for _ in range(4)])
    tick_scenario('tick_actor_hit', (0x1000000, 0x2000000, 0x640000, 1057032, 0, 0, 10, 0),
                  (0x4000000, 0x4000000, 0),
                  [{'a': 200000}] + [{} for _ in range(4)])
    tick_scenario('tick_closest_hit', (0x1000000, 0x2000000, 0x640000, 1057032, 0, 0, 10, 0),
                  (0x4000000, 0x4000000, 0),
                  [{'terrain': -300000, 'a': 100000, 'b': 50000}] + [{} for _ in range(4)])
    tick_scenario('tick_special_hit', (0x1000000, 0x2000000, 0x640000, 1057032, 0, 0, 10, 0),
                  (0x4000000, 0x4000000, 0),
                  [{'s': 400000, 'x': 200000}] + [{} for _ in range(4)], special_b=True)

    tick_scenario('tick_final_x_gate', (-0x300000,0,0x640000,0x400000,0,0,10,0), (0,0,0), [{}])
    tick_scenario('tick_final_y_gate', (0,-0x300000,0x640000,0,0x400000,0,10,0), (0,0,0), [{}])
    tick_scenario('tick_impact_filter_read', (0,0,65536,131072,0,0,10,0), (0x4000000,0x4000000,0), [{'terrain':-65536,'action':1}])
    tick_scenario('tick_impact_from_read', (0,0,65536,131072,0,0,10,0), (0x4000000,0x4000000,0), [{'terrain':-65536,'action':2}])
    tick_scenario('tick_move_center_read', (-100,0,0x640000,1057032,0,0,10,0), (0x4000000,0x4000000,0), [{'action':3}])
    tick_scenario('tick_query_position_read', (0,0,65536,131072,0,0,10,0), (0x4000000,0x4000000,0), [{'action':4}])
    tick_scenario('tick_query_impact_position_read', (0,0,65536,131072,0,0,10,0), (0x4000000,0x4000000,0), [{'terrain':-65536,'action':4}])
    tick_scenario('tick_wrap_gate', (-2147483648,0,0x640000,131072,0,0,10,0), (0,0,0), [{}])
    tick_scenario('tick_wrap_position', (2147483600,0,0x640000,131072,0,0,10,0), (-2147483648,0,0), [{}])

    print('\nscenarios matched:', checker.report['cases_matched'])
    print('cases compared:', checker.report['cases_compared'])
    if args.report:
        args.report.write_text(json.dumps(checker.report, indent=2) + '\n')
    return 1 if checker.report['mismatches'] else 0


if __name__ == '__main__':
    sys.exit(main())
