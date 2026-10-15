#!/usr/bin/env python3
"""Check the native weapon state against original DF1 instructions.

Runs the same scripted scenarios through:
  1. the static weapon/class tables in src/weapons.cpp (compared to the
     original data at 0x4F9934 / 0x4F9154), and
  2. the original local-player weapon routines with unicorn, using the
     shipped Df.exe as read-only evidence.

Covered original routines: 4848C0 resupply, 483DD0 slot select,
484B70 cycle, 483C70 fire, 483BD0 change magazine, 484C20 aim toggle,
482B00 tick (weapon part): 483B10 (0x483AD8), 483B70 (0x483B44),
483BD0 (0x483BA4), 483C30 (0x483C1C), 483240/4832F0/483390,
482F90, 483410, 483580, 483610, 483670, 4837D0, 483860, 4838C0,
4839F0, 483A30, 483AA0, 4840B0/4841F0 (placement call only),
470F40/470F50 (queue call only).

Ballistic calls (45EA00 projectile creation, 46FB40 bullet entity) and
recoil/eye-height view state stay outside the comparison. The 45EA00 stub
only reproduces the statically read burst counter side (45EB17-45EB5E:
actor item definition +0x6C), which is timing-visible state.

Usage: check_weapons_original.py GAME_FOLDER WEAPONS_PROBE [--report REPORT]
"""
import argparse
import hashlib
import json
import struct
import subprocess
import sys
import unicorn
from pathlib import Path
import pefile
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP,
)

SOURCE_SHA256 = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
BASE = 0x400000
STACK = 0x2008000
RET_MAGIC = 0x4000000
ACTOR = 0x1200000
ACTOR_DEF = 0x1210000
ITEM_TABLE = 0x1300000
RTXT_BUF = 0x1400000

G = {
    'item_table': 0x00C23810,
    'item_count': 0x00C23814,
    'actor_ptr': 0x00C23820,
    'crouch': 0x00C23828,
    'fire_timer': 0x00C23830,
    'reload_timer': 0x00C23834,
    'pending': 0x00C23838,
    'magazine': 0x00C238B8,
    'reserve': 0x00C23938,
    'slot': 0x00C239B8,
    'slot_map': 0x00C239C0,
    'eye': 0x00C23A18,
    'raise': 0x00C23A3C,
    'double_ammo': 0x00C23A4C,
    'aim_block': 0x00C23B64,
    'dbc9': 0x0067DBC9,
    'multiplayer': 0x0067F528,
    'camera_mode': 0x006BA3EC,
}

CLASS_COUNT = 28
SLOT_COUNT = 10


class OriginalRig:
    def __init__(self, game):
        self.game = game
        raw = self.raw = Path(self.game, 'Df.exe').read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        if digest != SOURCE_SHA256:
            raise SystemExit('Unsupported original executable hash: ' + digest)
        pe = pefile.PE(data=raw, fast_load=True)
        self.image = pe.get_memory_mapped_image()
        self.sections = [(s.VirtualAddress, s.PointerToRawData, s.SizeOfRawData)
                         for s in pe.sections if s.PointerToRawData]
        u = self.u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        u.mem_map(BASE, 0xA00000)
        u.mem_write(BASE, self.image)
        u.mem_map(0x1000000, 0x100000)
        u.mem_map(0x1100000, 0x100000)
        u.mem_map(0x1200000, 0x100000)
        u.mem_map(ITEM_TABLE, 0x200000)
        u.mem_map(0x2000000, 0x10000)
        self.events = []
        self.trigger_held = False
        self.stubs = {
            0x43E7D0: self.stub_sound,
            0x43E720: self.stub_sound2,
            0x45EA00: self.stub_shot,
            0x470F60: self.stub_place,
            0x46F3E0: self.stub_queue,
            0x479F00: self.stub_message,
            0x4776C0: self.stub_sincos,
            0x477860: self.stub_rand,
            0x49AF6C: self.stub_rand,
            0x47A290: self.stub_key,
            0x49A620: self.stub_memset,
            0x45E7A0: self.stub_void,
            0x484740: self.stub_void,
            0x4847D0: self.stub_vec3,
            0x482D90: self.stub_void,
            0x49BAE6: self.stub_fp,
            0x404CF0: self.stub_rtxt,
            0x493A80: self.stub_sprintf,
        }
        u.hook_add(unicorn.UC_HOOK_CODE, self.hook)
        self.reset()

    def reset(self):
        self.events = []
        self.trigger_held = False
        # Clear the original player-control globals (C23810-C23BFF) so each
        # scenario starts like a fresh process.
        self.u.mem_write(0x00C23800, b'\0' * 0x400)
        self.u.mem_write(0x1200000, b'\0' * 0x20000)
        self.u.mem_write(0x1100000, b'\0' * 0x10000)
        self.u.mem_write(0x1000000, b'\0' * 0x100000)
        self.poke(G['actor_ptr'], ACTOR)
        self.poke(ACTOR + 0xC, ACTOR_DEF)
        self.poke(G['item_table'], ITEM_TABLE)
        self.poke(G['item_count'], 2)
        self.poke(G['eye'], 0xB333)
        self.poke(G['slot'], 0)
        self.poke(G['multiplayer'], 0)
        self.poke(G['camera_mode'], 0)
        self.poke(G['dbc9'] & ~3, 0) # 67DBC8 flags dword (67DBC9 bit 0 = double ammo)
        self.poke(0x00AD3ED0, 0x4F9934) # weapon table 46BFB0
        self.poke(0x00AD3EDC, 39)
        self.poke(0x00AD3EE0, 28)
        self.poke(0x00AD3EE4, 0x4F9154) # class table 46BFB0
        self.u.mem_write(RTXT_BUF, b'probe-text\0')

    # ---------- stubs ----------
    def finish(self, retval=0):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', self.u.mem_read(sp, 4))[0]
        self.u.reg_write(UC_X86_REG_ESP, sp + 4)
        self.u.reg_write(UC_X86_REG_EAX, retval & 0xFFFFFFFF)
        self.u.reg_write(UC_X86_REG_EIP, ret)

    def stub_void(self):
        self.finish(0)

    def stub_sound(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        sid = struct.unpack('<I', self.u.mem_read(sp + 4, 4))[0]
        self.events.append(['sound', sid])
        self.finish(0)

    def stub_sound2(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        sid = struct.unpack('<I', self.u.mem_read(sp + 4, 4))[0]
        self.events.append(['sound2', sid])
        self.finish(0)

    def stub_shot(self):
        # 45EA00 burst counter part only (45EB17-45EB5E); projectile stays
        # outside the check.
        sp = self.u.reg_read(UC_X86_REG_ESP)
        actor = struct.unpack('<I', self.u.mem_read(sp + 4, 4))[0]
        index = self.peek(actor + 0x94)
        def_ptr = self.peek(actor + 0xC)
        counter = self.peek(def_ptr + 0x6C)
        if index == 6:
            if counter == 0:
                self.poke(def_ptr + 0x6C, 2)
            else:
                self.poke(def_ptr + 0x6C, (counter - 1) & 0xFFFFFFFF)
        self.events.append(['shot', index])
        self.finish(0)

    def stub_place(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        item = struct.unpack('<I', self.u.mem_read(sp + 4, 4))[0]
        self.events.append(['place', item])
        self.finish(0)

    def stub_queue(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        kind = struct.unpack('<I', self.u.mem_read(sp + 4, 4))[0]
        self.events.append(['queue', kind])
        self.finish(0)

    def stub_message(self):
        self.events.append(['message', 0])
        self.finish(0)

    def stub_sincos(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        out1, out2 = struct.unpack('<2I', self.u.mem_read(sp + 8, 8))
        self.u.mem_write(out1, struct.pack('<I', 0))
        self.u.mem_write(out2, struct.pack('<I', 0x10000))
        self.finish(0)

    def stub_rand(self):
        self.finish(0x1000)

    def stub_key(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        control = struct.unpack('<I', self.u.mem_read(sp + 4, 4))[0]
        self.finish(1 if control == 0x8C and self.trigger_held else 0)

    def stub_memset(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        dst, val, size = struct.unpack('<3I', self.u.mem_read(sp + 4, 12))
        self.u.mem_write(dst, bytes([val & 0xFF]) * (size & 0xFFFFF))
        self.finish(dst)

    def stub_vec3(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        for at in range(3):
            ptr = struct.unpack('<I', self.u.mem_read(sp + 4 + at * 4, 4))[0]
            self.u.mem_write(ptr, b'\0\0\0\0')
        self.finish(0)

    def stub_fp(self):
        self.finish(0)

    def stub_rtxt(self):
        self.finish(RTXT_BUF)

    def stub_sprintf(self):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        dst = struct.unpack('<I', self.u.mem_read(sp + 4, 4))[0]
        self.u.mem_write(dst, b'text\0')
        self.finish(dst)

    def hook(self, machine, addr, size, user):
        if addr == RET_MAGIC:
            machine.emu_stop()
            return
        fn = self.stubs.get(addr)
        if fn:
            fn()

    # ---------- helpers ----------
    def poke(self, addr, value):
        self.u.mem_write(addr, struct.pack('<I', value & 0xFFFFFFFF))

    def peek(self, addr):
        return struct.unpack('<I', self.u.mem_read(addr, 4))[0]

    def peek_signed(self, addr):
        value = self.peek(addr)
        return value if value < 0x80000000 else value - 0x100000000

    def call(self, fn, *args):
        sp = STACK
        stack = struct.pack('<I', RET_MAGIC) + b''.join(
            struct.pack('<I', a & 0xFFFFFFFF) for a in args)
        self.u.mem_write(sp, stack)
        self.u.reg_write(UC_X86_REG_ESP, sp)
        self.u.emu_start(fn, RET_MAGIC, count=4000000)
        if self.u.reg_read(UC_X86_REG_EIP) != RET_MAGIC:
            raise RuntimeError(f'Original weapon call {fn:08x} did not return')

    def snapshot(self, events=None):
        magazine = [self.peek_signed(G['magazine'] + 4 * i) for i in range(CLASS_COUNT)]
        reserve = [self.peek_signed(G['reserve'] + 4 * i) for i in range(CLASS_COUNT)]
        pending = [self.peek_signed(G['pending'] + 4 * i) for i in range(CLASS_COUNT)]
        return {
            'slot': self.peek(G['slot']),
            'weapon': self.peek(G['slot_map'] + 4 * self.peek(G['slot'])) & 0xFF,
            'fire_timer': self.peek_signed(G['fire_timer']),
            'reload_timer': self.peek_signed(G['reload_timer']),
            'aim': 1 if self.peek(G['aim_block']) else 0,
            'burst': self.peek_signed(self.peek(ACTOR + 0xC) + 0x6C),
            'raise': self.peek_signed(G['raise']),
            'magazine': magazine,
            'reserve': reserve,
            'pending': pending,
            'events': events if events is not None else [],
        }

    # ---------- script ops ----------
    def set_slot_map(self, values):
        for i in range(SLOT_COUNT):
            self.poke(G['slot_map'] + 4 * i, values[i])

    def set_unlimited(self, on):
        dword = self.peek(G['dbc9'] & ~3)          # 67DBC8, bit 8 is 67DBC9 bit 0
        dword = (dword | 0x100) if on else (dword & ~0x100)
        self.poke(G['dbc9'] & ~3, dword)

    def resupply(self, double_primary):
        self.poke(G['double_ammo'], 1 if double_primary else 0)
        self.call(0x4848C0)
        return self.snapshot(self.take_events())

    def select(self, slot):
        self.call(0x483DD0, slot)
        return self.snapshot(self.take_events())

    def cycle(self, direction):
        self.call(0x484B70, direction)
        return self.snapshot(self.take_events())

    def fire(self):
        self.call(0x483C70)
        return self.snapshot(self.take_events())

    def reload_press(self):
        self.call(0x483BD0)
        return self.snapshot(self.take_events())

    def aim(self):
        self.call(0x484C20)
        return self.snapshot(self.take_events())

    def tick(self, trigger):
        self.trigger_held = bool(trigger)
        self.call(0x482B00)
        return self.snapshot(self.take_events())

    def take_events(self):
        events = self.events
        self.events = []
        return events


def run_probe(probe, script):
    out = subprocess.check_output([str(probe)], input=script.encode())
    lines = [line for line in out.decode().splitlines() if line.strip()]
    return [json.loads(line) for line in lines]


def expected_lines(lines):
    keep = {'op', 'slot', 'weapon', 'fire_timer', 'reload_timer', 'aim', 'burst',
            'raise', 'magazine', 'reserve', 'pending', 'events'}
    return [{k: v for k, v in line.items() if k in keep} for line in lines]


def run_original(rig, script):
    rig.reset()
    out = []
    for line in script.splitlines():
        words = line.split()
        if not words or words[0].startswith('#'):
            continue
        op = words[0]
        if op == 'slot_map':
            rig.set_slot_map([int(w) for w in words[1:11]])
        elif op == 'unlimited':
            rig.set_unlimited(int(words[1]) != 0)
        elif op == 'resupply':
            state = rig.resupply(int(words[1]) != 0)
            state['op'] = 'resupply'
            out.append(state)
        elif op == 'select':
            state = rig.select(int(words[1]))
            state['op'] = 'select'
            out.append(state)
        elif op == 'cycle':
            state = rig.cycle(int(words[1]))
            state['op'] = 'cycle'
            out.append(state)
        elif op == 'fire':
            state = rig.fire()
            state['op'] = 'fire'
            out.append(state)
        elif op == 'reload':
            state = rig.reload_press()
            state['op'] = 'reload'
            out.append(state)
        elif op == 'aim':
            state = rig.aim()
            state['op'] = 'aim'
            out.append(state)
        elif op == 'tick':
            count, trigger = int(words[1]), int(words[2]) != 0
            for i in range(count):
                state = rig.tick(trigger)
                state['op'] = 'tick%d' % i
                out.append(state)
        else:
            raise SystemExit('unknown script op: ' + op)
    return out


# scenarios: same scripts are sent to the native probe and replayed on the
# original code. M4 burst is weapon 6, M4 single weapon 7, M249 weapon 12,
# HS22 weapon 4, knife weapon 1, M203 weapon 10, Barrett weapon 15,
# LAW weapon 28, satchel 29, detonator 30, grenade 33, claymore 37/38,
# laser 27, M40 32.
MAP_MAIN = 'slot_map 1 4 6 10 12 15 28 29 33 37'
MAP_SINGLE = 'slot_map 1 4 7 10 12 15 28 29 33 37'
MAP_M40 = 'slot_map 1 4 6 10 12 15 28 29 33 37  # slots beyond 9 ignored'

SCENARIOS = [
    ('resupply', f'''{MAP_MAIN}
resupply 0
resupply 1
'''),
    ('m4_burst_click', f'''{MAP_MAIN}
resupply 0
select 2
fire
tick 30 0
'''),
    ('m4_single_clicks', f'''{MAP_SINGLE}
resupply 0
select 2
fire
tick 12 0
fire
tick 12 0
fire
tick 6 0
'''),
    ('m249_auto_hold', f'''{MAP_MAIN}
resupply 0
select 4
fire
tick 26 1
tick 8 0
'''),
    ('m1911_semis', f'''{MAP_MAIN}
resupply 0
select 1
fire
tick 11 0
fire
tick 11 0
fire
tick 11 0
'''),
    ('pistol_empty_reload', f'''{MAP_MAIN}
resupply 0
select 1
fire
tick 11 0
fire
tick 11 0
fire
tick 11 0
fire
tick 11 0
fire
tick 11 0
fire
tick 11 0
fire
tick 11 0
fire
tick 11 0
fire
tick 130 0
fire
tick 12 0
'''),
    ('reload_press_and_cancel', f'''{MAP_SINGLE}
resupply 0
select 2
fire
tick 11 0
reload
tick 10 0
select 1
fire
tick 11 0
reload
tick 130 0
fire
tick 11 0
'''),
    ('switch_to_empty_autoreload', f'''{MAP_SINGLE}
resupply 0
select 2
fire
tick 11 0
fire
tick 11 0
reload
tick 130 0
fire
tick 11 0
fire
tick 11 0
fire
tick 11 0
fire
tick 11 0
'''),
    ('cycle_slots', f'''{MAP_MAIN}
resupply 0
select 3
cycle 1
cycle 1
cycle 1
cycle -1
cycle -1
cycle 1
'''),
    ('cycle_single_filled', f'''slot_map 1 0 0 0 0 0 0 0 0 0
resupply 0
select 1
cycle 1
cycle -1
'''),
    ('aim_block', f'''{MAP_MAIN}
resupply 0
select 2
aim
fire
tick 2 0
aim
fire
tick 10 0
'''),
    ('unlimited_ammo', f'''{MAP_MAIN}
unlimited 1
resupply 0
select 2
fire
tick 11 0
fire
tick 11 0
fire
tick 11 0
reload
tick 130 0
'''),
    ('knife_spam', f'''{MAP_MAIN}
resupply 0
select 0
fire
tick 70 0
fire
tick 70 0
fire
tick 20 0
fire
'''),
    ('grenades', f'''{MAP_MAIN}
resupply 0
select 8
fire
tick 91 0
fire
tick 91 0
fire
tick 91 0
fire
tick 91 0
fire
tick 91 0
fire
tick 91 0
fire
tick 91 0
fire
tick 20 0
fire
tick 20 0
'''),
    ('claymore', f'''{MAP_MAIN}
resupply 0
select 9
fire
tick 91 0
fire
tick 91 0
fire
tick 91 0
fire
tick 20 0
'''),
    ('law_launcher', f'''{MAP_MAIN}
resupply 0
select 6
fire
tick 131 0
fire
tick 91 0
reload
tick 10 0
'''),
    ('m203_launcher', f'''{MAP_MAIN}
resupply 0
select 3
fire
tick 151 0
fire
tick 151 0
fire
'''),
    ('satchel_and_detonator', f'''{MAP_MAIN}
resupply 0
select 7
fire
tick 91 0
fire
tick 91 0
fire
tick 91 0
fire
tick 20 0
select 1
'''),
    ('claymore38', '''slot_map 1 4 6 10 12 15 28 29 33 38
resupply 0
select 9
fire
tick 91 0
fire
tick 91 0
fire
tick 91 0
fire
tick 20 0
'''),
    ('m249_empty_spin', f'''{MAP_MAIN}
resupply 0
select 4
unlimited 1
fire
tick 200 1
unlimited 0
tick 60 1
reload
tick 130 0
fire
tick 7 1
'''),
    ('hs22_drain_select', MAP_MAIN + '\nresupply 0\nselect 1\n'
     + 'fire\ntick 11 0\n' * 10
     + 'select 0\nselect 1\ntick 121 0\nfire\ntick 11 0\n'
     + 'select 1\ntick 121 0\n'),
    ('laser', f'''{MAP_MAIN}
resupply 0
select 0
select 1
'''),
    ('detonator_queue', '''slot_map 1 4 6 10 12 15 28 29 30 37
resupply 0
select 8
fire
fire
fire
tick 10 0
'''),
    ('laser_fire', '''slot_map 1 27 6 10 12 15 28 29 33 37
resupply 0
select 1
fire
tick 180 0
fire
tick 10 0
'''),
    ('barrett_bolt', f'''{MAP_MAIN}
resupply 0
select 5
fire
tick 56 0
fire
tick 56 0
fire
tick 20 0
reload
tick 121 0
fire
'''),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()

    rig = OriginalRig(args.game)

    report = {
        'source_sha256': SOURCE_SHA256,
        'tables_verified': False,
        'scenarios': [],
        'lines_compared': 0,
        'mismatches': [],
        'original_functions': [
            '4848C0', '483DD0', '484B70', '483C70', '483B10', '483B70',
            '483BD0', '483C30', '483240', '4832F0', '483390', '482F90',
            '483410', '483580', '483610', '483670', '4837D0', '483860',
            '4838C0', '4839F0', '483A30', '483AA0', '4840B0', '4841F0',
            '482B00 (weapon part)',
        ],
        'stubbed_functions': {
            '43E7D0': 'positional sound -> event',
            '43E720': 'global sound -> event',
            '45EA00': 'projectile entry -> burst counter model + event',
            '470F60': 'placement call -> event (entity stays input)',
            '46F3E0': 'message queue -> event',
            '479F00': 'HUD text -> event',
            '4776C0': 'sincos -> fixed 0/0x10000',
            '477860': 'rand -> 0x1000',
            '49AF6C': 'rand -> 0x1000',
            '47A290': 'control 0x8C state -> scenario trigger flag',
            '49A620': 'memset',
            '45E7A0': 'facing helper',
            '484740': 'aim assist check -> 0',
            '4847D0': 'aim point -> zeros',
            '482D90': 'muzzle origin -> none',
            '49BAE6': 'float helper',
            '404CF0': 'RTXT lookup -> constant text',
            '493A80': 'sprintf -> constant text',
        },
    }

    # 1. verify native tables against the original data
    tables = run_probe(args.probe, 'tables\n')[0]
    raw = rig.raw
    pe = pefile.PE(data=raw, fast_load=True)

    def va_to_off(va):
        for s in pe.sections:
            start = BASE + s.VirtualAddress
            if start <= va < start + max(s.Misc_VirtualSize, s.SizeOfRawData):
                return s.PointerToRawData + (va - start)
        return None

    weapon_off = va_to_off(0x4F9934)
    class_off = va_to_off(0x4F9154)
    table_errors = []
    for i in range(39):
        d = struct.unpack_from('<17i', raw, weapon_off + i * 0xA8 + 0x64)
        want = [d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9],
                d[10], d[11], d[14], d[15], d[16] & 0xFFFFFFFF]
        got = tables['weapons'][i]
        if want != got:
            table_errors.append(('weapon', i, want, got))
    for i in range(28):
        d = struct.unpack_from('<13I', raw, class_off + i * 0x34)
        want = list(d)
        got = tables['classes'][i]
        if want != got:
            table_errors.append(('class', i, want, got))
    report['tables_verified'] = not table_errors
    if table_errors:
        report['mismatches'].append({'scenario': 'tables', 'errors': table_errors[:5]})
        print('TABLE MISMATCHES:', table_errors[:5])
    else:
        print('tables: native weapon/class tables match the original data')

    # 2. scenario comparisons
    for name, script in SCENARIOS:
        native = expected_lines(run_probe(args.probe, script))
        original = run_original(rig, script)
        entry = {'name': name, 'lines': len(native)}
        if len(native) != len(original):
            entry['error'] = f'line count native={len(native)} original={len(original)}'
            report['mismatches'].append(entry)
            print(f'{name}: LINE COUNT MISMATCH {entry["error"]}')
            continue
        bad = None
        for index, (a, b) in enumerate(zip(native, original)):
            if a != b:
                bad = index
                break
        if bad is None:
            report['lines_compared'] += len(native)
            report['scenarios'].append(entry)
            print(f'{name}: {len(native)} lines match')
        else:
            entry['error'] = f'first mismatch at line {bad}'
            entry['native'] = native[bad]
            entry['original'] = original[bad]
            report['mismatches'].append(entry)
            print(f'{name}: MISMATCH at line {bad}')
            print('  native  :', json.dumps(native[bad]))
            print('  original:', json.dumps(original[bad]))

    print(f"\nscenarios matched: {len(report['scenarios'])}/{len(SCENARIOS)}")
    print(f"lines compared: {report['lines_compared']}")
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    if report['mismatches'] or not report['tables_verified']:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
