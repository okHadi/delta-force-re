#!/usr/bin/env python3
"""Check the 0x477D40 gameplay command dispatcher against original DF1 x86.

Replays every branch of the original dispatcher under Unicorn, logs its state
writes and external calls, and compares them with the native module through the
gameplay_dispatch_probe line protocol. Exits nonzero on any mismatch.
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
from unicorn import UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP

SOURCE_HASH = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
IMAGE, STACK, HEAP, STOP = 0x400000, 0x2000000, 0x3000000, 0x4000000
ACTOR = HEAP + 0x1000
ACTOR_PTR_ADDR = 0xc23820
FNINIT = HEAP + 0x800
THREAD_CREATE, THREAD_TEXT = 0x51510001, 0x51510002
# Argument tokens, matching include/dfre/gameplay_dispatch.hpp.
TOKEN_TEXT, TOKEN_CREATED, TOKEN_TEXT_HANDLE = 0xffffff01, 0xffffff02, 0xffffff03
TOKEN_UNINIT, TOKEN_ACTOR = 0xffffff04, 0xffffff05

# name, address, byte width. Order matches the probe "fields" mode.
STATE = [
    ('multiplayer_67f520', 0x67f520, 4), ('mission_67f528', 0x67f528, 4),
    ('mission2_67f52c', 0x67f52c, 4), ('menu_flag_67dbc0', 0x67dbc0, 4),
    ('server_67dbb8', 0x67dbb8, 4), ('resolution_index_67f4f4', 0x67f4f4, 4),
    ('resolution_max_503134', 0x503134, 4), ('lb_size_67f4f8', 0x67f4f8, 4),
    ('mouse_disabled_503130', 0x503130, 4), ('command_tick_6e471c', 0x6e471c, 4),
    ('tick_base_bd4844', 0xbd4844, 4), ('pause_6e4714', 0x6e4714, 4),
    ('pause2_6e4718', 0x6e4718, 4), ('toggle_6e558c', 0x6e558c, 4),
    ('toggle_6e5590', 0x6e5590, 4), ('toggle_6e5594', 0x6e5594, 4),
    ('saturation_67f4f0', 0x67f4f0, 4), ('mouse_scale_67f500', 0x67f500, 4),
    ('toggle_67f4fc', 0x67f4fc, 4), ('toggle_67f504', 0x67f504, 4),
    ('hud_color_index_67f508', 0x67f508, 4), ('hud_color_value_67db58', 0x67db58, 4),
    ('hud_detail_67f50c', 0x67f50c, 4), ('toggle_67f510', 0x67f510, 4),
    ('toggle_67f514', 0x67f514, 4), ('toggle_67f518', 0x67f518, 4),
    ('toggle_67f564', 0x67f564, 4), ('toggle_503428', 0x503428, 4),
    ('toggle_67db78', 0x67db78, 4), ('toggle_67db7c', 0x67db7c, 4),
    ('toggle_67db80', 0x67db80, 4), ('toggle_67db84', 0x67db84, 4),
    ('toggle_67db88', 0x67db88, 4), ('toggle_67db8c', 0x67db8c, 4),
    ('toggle_67db90', 0x67db90, 4), ('flag_67db70', 0x67db70, 4),
    ('flag_67db74', 0x67db74, 4), ('type_flag_67db6c', 0x67db6c, 4),
    ('menu_state_67dbbc', 0x67dbbc, 4), ('flags_67dbc8', 0x67dbc8, 1),
    ('flags_67dbc9', 0x67dbc9, 1), ('screenshot_67dbf8', 0x67dbf8, 4),
    ('screenshot_bfbb00', 0xbfbb00, 4), ('fo_toggle_c23a34', 0xc23a34, 4),
    ('fov_c23a28', 0xc23a28, 4), ('map_terrain_c23a2c', 0xc23a2c, 4),
    ('map_elevation_c23a30', 0xc23a30, 4), ('camera_mode_bd0040', 0xbd0040, 4),
    ('control_c23a00', 0xc23a00, 4), ('pulse_c23a04', 0xc23a04, 4),
    ('pulse_c23a08', 0xc23a08, 4), ('pulse_c23a0c', 0xc23a0c, 4),
    ('camera_6ba424', 0x6ba424, 4), ('camera_6ba428', 0x6ba428, 4),
    ('camera_6ba42c', 0x6ba42c, 4),
]
ACTOR_FIELDS = [('look_yaw_30', 0x30, 4), ('look_pitch_34', 0x34, 4), ('stance_74', 0x74, 2)]
SEED_ORDER = [
    'actor_flags',
    'pause_6e4714',
    'pause2_6e4718',
    'menu_flag_67dbc0',
    'multiplayer_67f520',
    'mission_67f528',
    'mission2_67f52c',
    'server_67dbb8',
    'command_tick_6e471c',
    'tick_base_bd4844',
    'resolution_index_67f4f4',
    'resolution_max_503134',
    'lb_size_67f4f8',
    'mouse_disabled_503130',
    'camera_mode_bd0040',
    'saturation_67f4f0',
    'mouse_scale_67f500',
    'fov_c23a28',
    'map_terrain_c23a2c',
    'map_elevation_c23a30',
    'fo_toggle_c23a34',
    'hud_color_index_67f508',
    'hud_detail_67f50c',
    'toggle_67f4fc',
    'toggle_67f504',
    'toggle_67f510',
    'toggle_67f514',
    'toggle_67f518',
    'toggle_67f564',
    'toggle_503428',
    'toggle_67db78',
    'toggle_67db7c',
    'toggle_67db80',
    'toggle_67db84',
    'toggle_67db88',
    'toggle_67db8c',
    'toggle_67db90',
    'flag_67db70',
    'flag_67db74',
    'type_flag_67db6c',
    'toggle_6e558c',
    'toggle_6e5590',
    'toggle_6e5594',
    'menu_state_67dbbc',
    'flags_67dbc8',
    'flags_67dbc9',
    'screenshot_67dbf8',
    'screenshot_bfbb00',
    'control_c23a00',
    'pulse_c23a04',
    'pulse_c23a08',
    'pulse_c23a0c',
    'camera_6ba424',
    'camera_6ba428',
    'camera_6ba42c',
    'look_yaw_30',
    'look_pitch_34',
    'stance_74',
    'hud0',
    'hud1',
    'hud2',
    'hud3',
    'hud4',
    'hud5',
]


CALL_ARITY = {
    0x401d50: 4, 0x4027d0: 0, 0x402d50: 1, 0x403280: 1, 0x403480: 1, 0x404ba0: 1,
    0x404cf0: 5, 0x428810: 1, 0x42ac10: 2, 0x444570: 1, 0x446630: 0, 0x447970: 2,
    0x447fb0: 1, 0x4483f0: 0, 0x448410: 1, 0x449600: 0, 0x4604c0: 1, 0x46f3e0: 3,
    0x470ff0: 0, 0x471830: 1, 0x473eb0: 2, 0x477270: 0, 0x477280: 0, 0x477290: 0,
    0x4772b0: 0, 0x4772d0: 0, 0x4772e0: 0, 0x4772f0: 0, 0x477300: 0, 0x4799d0: 4,
    0x479f00: 2, 0x483bd0: 0, 0x483c70: 0, 0x483dd0: 1, 0x484320: 1, 0x4847c0: 0,
    0x4848c0: 0, 0x484a50: 0, 0x484b70: 1, 0x484c20: 0, 0x484c80: 0, 0x484d00: 0,
    0x484d80: 0, 0x493a80: 3,
}
KIND_OF = {
    0x404ba0: 'ui_button', 0x404cf0: 'dialog', 0x4799d0: 'dialog', 0x401d50: 'dialog_text',
    0x479f00: 'message', 0x493a80: 'format_int', 0x46f3e0: 'game_event',
    0x483c70: 'weapon_fire', 0x483bd0: 'weapon_reload', 0x483dd0: 'weapon_select',
    0x484b70: 'weapon_cycle', 0x484c20: 'weapon_aim', 0x4848c0: 'weapon_supply',
    0x484320: 'weapon_supply', 0x484a50: 'weapon_supply', 0x484c80: 'weapon_supply',
    0x484d00: 'weapon_supply', 0x484d80: 'weapon_supply', 0x4847c0: 'weapon_unknown',
    0x4027d0: 'screen_mode', 0x403280: 'screen_mode', 0x403480: 'screen_mode',
    0x402d50: 'screen_mode', 0x428810: 'screen_mode', 0x4604c0: 'actor_call',
    0x447fb0: 'multiplayer_call', 0x471830: 'multiplayer_call', 0x447970: 'multiplayer_call',
    0x444570: 'multiplayer_call', 0x4483f0: 'multiplayer_call', 0x448410: 'multiplayer_call',
    0x446630: 'multiplayer_call', 0x449600: 'mission_call', 0x470ff0: 'mission_call',
    0x42ac10: 'camera_mode', 0x473eb0: 'screenshot',
}
for _picture in (0x477270, 0x477280, 0x477290, 0x4772b0, 0x4772d0, 0x4772e0, 0x4772f0, 0x477300):
    KIND_OF[_picture] = 'camera_picture'
# Ranges that run as original code instead of being stubbed: the dispatcher
# itself, its recursive entry, the FPU round helper, the three team toggles and
# the 0x484960 double-ammo thunk.
RUN_REAL_RANGES = ((0x477d40, 0x4790a0), (0x49bae6, 0x49bb03), (0x448c90, 0x448ce0),
                   (0x484960, 0x48496c))


class Original:
    def __init__(self, raw):
        pe = pefile.PE(data=raw)
        self.u = unicorn.Uc(UC_ARCH_X86, UC_MODE_32)
        self.u.mem_map(IMAGE, 0x1000000)
        for section in pe.sections:
            if section.PointerToRawData and not section.Characteristics & 0x80:
                chunk = raw[section.PointerToRawData:section.PointerToRawData + section.SizeOfRawData]
                self.u.mem_write(IMAGE + section.VirtualAddress, chunk)
        pe.close()
        for at, size in ((STACK, 0x10000), (HEAP, 0x10000), (STOP, 0x1000)):
            self.u.mem_map(at, size)
        # fninit; ret: unicorn's x87 control word drifts after the 49BAE6 helper,
        # so each replay starts from the power-on FPU state.
        self.u.mem_write(FNINIT, b'\xdb\xe3\xc3')
        self.calls = []
        self.u.hook_add(UC_HOOK_CODE, self.hook)

    def read_word(self, at):
        return struct.unpack('<I', self.u.mem_read(at, 4))[0]

    def write_word(self, at, value):
        self.u.mem_write(at, struct.pack('<I', value & 0xffffffff))

    def hook(self, machine, at, size, user):
        if at in (STOP, FNINIT) or any(lo <= at < hi for lo, hi in RUN_REAL_RANGES):
            return
        if at not in CALL_ARITY:
            raise ValueError(f'original called unknown address {at:#x}')
        sp = self.u.reg_read(UC_X86_REG_ESP)
        args = [self.read_word(sp + 4 + 4 * i) for i in range(6)]
        self.calls.append((at, args))
        result = THREAD_CREATE if at == 0x404cf0 else THREAD_TEXT if at == 0x401d50 else 0
        self.u.reg_write(UC_X86_REG_EAX, result)
        self.u.reg_write(UC_X86_REG_EIP, self.read_word(sp))
        self.u.reg_write(UC_X86_REG_ESP, sp + 4)

    def seed(self, seeds):
        self.u.mem_write(ACTOR, bytes(0x200))
        self.write_word(ACTOR_PTR_ADDR, ACTOR)
        for name, address, width in STATE:
            value = (seeds.get(name, 0) if name in SEED_ORDER else 0) & ((1 << (8 * width)) - 1)
            self.u.mem_write(address, value.to_bytes(width, 'little'))
        actor = seeds.get('actor', {})
        self.write_word(ACTOR + 0x30, actor.get('look_yaw_30', 0x11111111))
        self.write_word(ACTOR + 0x34, actor.get('look_pitch_34', 0x22222222))
        self.u.mem_write(ACTOR + 0x74, struct.pack('<H', actor.get('stance_74', 0x3333)))
        self.write_word(ACTOR + 0x6c, seeds.get('actor_flags', 0))
        for index in range(6):
            self.write_word(0x67db40 + 4 * index, seeds.get(f'hud{index}', 0))
        self.calls = []

    def run(self, cid, value):
        self.u.emu_start(FNINIT, FNINIT + 2, count=1)
        sp = STACK + 0x8000
        self.u.mem_write(sp, struct.pack('<III', STOP, cid, value & 0xffffffff))
        self.u.reg_write(UC_X86_REG_ESP, sp)
        self.u.emu_start(0x477d40, STOP, count=300000, timeout=0)
        if self.u.reg_read(UC_X86_REG_EIP) != STOP:
            raise ValueError('original dispatcher did not return')

    def state(self):
        out = {}
        for name, address, width in STATE:
            out[name] = int.from_bytes(self.u.mem_read(address, width), 'little')
        for name, offset, width in ACTOR_FIELDS:
            out[name] = int.from_bytes(self.u.mem_read(ACTOR + offset, width), 'little')
        return out

    def normalized_calls(self):
        out = []
        for address, args in self.calls:
            arity = CALL_ARITY[address]
            values = list(args[:arity])
            in_stack = lambda v: STACK <= v < STACK + 0x10000
            if address == 0x493a80 and in_stack(values[0]):
                values[0] = TOKEN_TEXT
            if address == 0x404cf0:
                if in_stack(values[2]):
                    values[2] = TOKEN_TEXT
                # 0x404CF0 has a 3-argument message form and a 5-argument dialog form;
                # the -1 timeout slot marks the message form.
                if values[2] == 0xffffffff:
                    values = values[:3]
            if address == 0x401d50:
                if in_stack(values[1]):
                    values[1] = TOKEN_TEXT
                if values[0] == THREAD_CREATE:
                    values[0] = TOKEN_CREATED
            if address == 0x4799d0 and values[0] == THREAD_CREATE:
                values[0] = TOKEN_CREATED
            if address == 0x479f00:
                if values[0] == THREAD_TEXT:
                    values[0] = TOKEN_TEXT_HANDLE
                elif values[0] == THREAD_CREATE:
                    values[0] = TOKEN_CREATED
            if address == 0x46f3e0 and in_stack(values[1]):
                values[1] = TOKEN_UNINIT
            if address in (0x42ac10, 0x4604c0):
                if values[0] != ACTOR:
                    raise ValueError('actor argument changed')
                values[0] = TOKEN_ACTOR
            threaded = any(v in (TOKEN_CREATED, TOKEN_TEXT_HANDLE) for v in values)
            out.append({'address': address, 'args': values, 'threaded': threaded})
        return out


def seed_values(seeds):
    actor = seeds.get('actor', {})
    values = {'actor_flags': seeds.get('actor_flags', 0),
              'look_yaw_30': actor.get('look_yaw_30', 0x11111111),
              'look_pitch_34': actor.get('look_pitch_34', 0x22222222),
              'stance_74': actor.get('stance_74', 0x3333)}
    for name, _, _ in STATE:
        values[name] = seeds.get(name, 0)
    for index in range(6):
        values[f'hud{index}'] = seeds.get(f'hud{index}', 0)
    return values


def probe_run(probe, cases):
    lines = []
    for case in cases:
        values = seed_values(case['seeds'])
        parts = [str(case['id']), str(case['value'])]
        for name in SEED_ORDER:
            parts.append(str(values[name] & 0xffffffff))
        lines.append(' '.join(parts) + '\n')
    result = subprocess.run([probe, 'run'], input=''.join(lines).encode(),
                            stdout=subprocess.PIPE, check=True)
    out = result.stdout.decode().splitlines()
    if len(out) != len(cases):
        raise ValueError(f'probe returned {len(out)} lines for {len(cases)} cases')
    return [json.loads(line) for line in out]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    raw = (args.game / 'Df.exe').read_bytes()
    sha = hashlib.sha256(raw).hexdigest()
    if sha != SOURCE_HASH:
        raise SystemExit('Unsupported original executable hash')
    probe = str(args.probe.resolve())
    mismatches = []

    # Field-order and kind-name alignment with the probe.
    fields = subprocess.check_output([probe, 'fields']).decode().split()
    if fields != ['id', 'value'] + SEED_ORDER:
        raise ValueError('probe field order changed')
    kinds = subprocess.check_output([probe, 'kinds']).decode().split()
    kind_index = {name: index for index, name in enumerate(kinds)}
    commands = json.loads(subprocess.check_output([probe, 'commands']))

    # Shipped command table at 0x4FB2FC (121 rows of 0x2C bytes).
    pe = pefile.PE(data=raw)
    image = pe.get_memory_mapped_image()
    pe.close()
    rows = []
    for index in range(121):
        base = 0x4fb2fc - IMAGE + index * 0x2c
        rows.append((struct.unpack_from('<I', image, base)[0],)
                    + tuple(struct.unpack_from('<4H', image, base + 0x24)))
    if len(commands) != len(rows):
        raise ValueError('native command table count changed')
    for index, entry in enumerate(commands):
        native_row = (entry['id'], entry['key'], entry['mouse'], entry['joystick'], entry['flags'])
        if native_row != rows[index]:
            mismatches.append({'mode': 'command_table', 'row': index, 'key': 'row',
                               'native': native_row, 'original': rows[index]})
    table_ids = sorted(row[0] for row in rows)

    original = Original(raw)

    base_seeds = {}
    toggle_seeds = {name: 1 for name in (
        'toggle_67f4fc', 'toggle_67f504', 'toggle_67f510', 'toggle_67f514', 'toggle_67f518',
        'toggle_67f564', 'toggle_503428', 'toggle_67db78', 'toggle_67db7c', 'toggle_67db80',
        'toggle_67db84', 'toggle_67db88', 'toggle_67db8c', 'toggle_67db90', 'type_flag_67db6c',
        'toggle_6e558c', 'toggle_6e5590', 'toggle_6e5594')}
    toggle_seeds.update({'hud_color_index_67f508': 5, 'hud_detail_67f50c': 2,
                         'flags_67dbc8': 0x20, 'flags_67dbc9': 0x09, 'fo_toggle_c23a34': 1,
                         'map_terrain_c23a2c': 1, 'map_elevation_c23a30': 1,
                         'camera_mode_bd0040': 1, 'server_67dbb8': 1,
                         'resolution_index_67f4f4': 5, 'resolution_max_503134': 5,
                         'lb_size_67f4f8': 1, 'mouse_disabled_503130': 1,
                         'command_tick_6e471c': 20, 'tick_base_bd4844': 0})
    mp_seeds = dict(toggle_seeds)
    mp_seeds.update({'multiplayer_67f520': 1, 'mission_67f528': 1, 'mission2_67f52c': 1,
                     'camera_mode_bd0040': 0, 'menu_flag_67dbc0': 0})
    effects = set()
    gated_cases = 0
    case_count = 0

    def compare(cases):
        nonlocal gated_cases, case_count
        case_count += len(cases)
        native_list = probe_run(probe, cases)
        for case, native in zip(cases, native_list):
            original.seed(case['seeds'])
            before = original.state()
            original.run(case['id'], case['value'])
            after = original.state()
            calls = original.normalized_calls()
            if before != after or calls:
                effects.add(case['id'])
            if native['gated']:
                gated_cases += 1
                if calls or before != after:
                    mismatches.append({'mode': case['label'], 'id': case['id'], 'value': case['value'],
                                       'key': 'gate', 'native': 'gated', 'original': 'acted'})
                continue
            for name, _, _ in STATE:
                if native['state'][name] != after[name]:
                    mismatches.append({'mode': case['label'], 'id': case['id'], 'value': case['value'],
                                       'seed': case['seeds'], 'key': name,
                                       'native': native['state'][name], 'original': after[name]})
            for field, _, _ in ACTOR_FIELDS:
                if native['actor'][field] != after[field]:
                    mismatches.append({'mode': case['label'], 'id': case['id'], 'value': case['value'],
                                       'key': f'actor.{field}', 'native': native['actor'][field],
                                       'original': after[field]})
            if len(native['calls']) != len(calls):
                mismatches.append({'mode': case['label'], 'id': case['id'], 'value': case['value'],
                                   'key': 'call-count', 'native': [c['address'] for c in native['calls']],
                                   'original': [c['address'] for c in calls]})
                continue
            for index, (native_call, original_call) in enumerate(zip(native['calls'], calls)):
                if native_call['address'] != original_call['address']:
                    mismatches.append({'mode': case['label'], 'id': case['id'], 'value': case['value'],
                                       'key': f'call{index}.address', 'native': hex(native_call['address']),
                                       'original': hex(original_call['address'])})
                    continue
                if native_call['args'] != original_call['args']:
                    mismatches.append({'mode': case['label'], 'id': case['id'], 'value': case['value'],
                                       'key': f'call{index}.args', 'native': native_call['args'],
                                       'original': original_call['args']})
                if bool(native_call['threaded']) != original_call['threaded']:
                    mismatches.append({'mode': case['label'], 'id': case['id'], 'value': case['value'],
                                       'key': f'call{index}.threaded', 'native': native_call['threaded'],
                                       'original': original_call['threaded']})
                expected_kind = KIND_OF.get(native_call['address'])
                if expected_kind is not None and native_call['kind'] != kind_index[expected_kind]:
                    mismatches.append({'mode': case['label'], 'id': case['id'], 'value': case['value'],
                                       'key': f'call{index}.kind', 'native': native_call['kind'],
                                       'original': kind_index[expected_kind]})

    def case(cid, value, seeds, label):
        return {'id': cid, 'value': value, 'seeds': seeds, 'label': label}

    # Pass 1: every id 0x00..0x1FF with open gates.
    compare([case(cid, 25, base_seeds, 'all-ids') for cid in range(0, 0x200)])
    # Pass 2: value coverage on ids with effects.
    pass2 = []
    for cid in sorted(effects):
        for value in (0, 1, -1, 0x7fffffff, -0x80000000):
            pass2.append(case(cid, value, base_seeds, 'values'))
    compare(pass2)
    # Pass 3: gate variants over every id.
    for name, overrides in (('actor4', {'actor_flags': 4}), ('pause', {'pause_6e4714': 1}),
                            ('menu-mp', {'menu_flag_67dbc0': 1, 'multiplayer_67f520': 1}),
                            ('mp', {'multiplayer_67f520': 1}),
                            ('all', {'actor_flags': 4, 'pause_6e4714': 1, 'menu_flag_67dbc0': 1,
                                     'multiplayer_67f520': 1})):
        compare([case(cid, 25, overrides, f'gate-{name}') for cid in range(0, 0x200)])
    # Pass 4: toggled and multiplayer seeds on table ids plus effect ids.
    pass4 = []
    for cid in sorted(set(table_ids) | effects):
        pass4.append(case(cid, 25, toggle_seeds, 'toggles'))
        pass4.append(case(cid, 25, mp_seeds, 'multiplayer'))
        pass4.append(case(cid, 25, dict(mp_seeds, menu_flag_67dbc0=1), 'menu-mp'))
    compare(pass4)
    # Pass 5: deterministic random coverage of the shipped command ids.
    rng = random.Random(0x477d40)
    names = [name for name, _, _ in STATE if name in SEED_ORDER]
    pass5 = []
    for _ in range(300):
        cid = rng.choice(table_ids)
        seeds = {name: rng.getrandbits(32) for name in rng.sample(names, 12)}
        seeds.update({'multiplayer_67f520': rng.randrange(2), 'menu_flag_67dbc0': rng.randrange(2),
                      'pause_6e4714': rng.randrange(2), 'actor_flags': rng.choice((0, 4))})
        pass5.append(case(cid, rng.choice((0, 1, -1, rng.randint(-10**6, 10**6),
                                           rng.getrandbits(32))), seeds, 'random'))
    compare(pass5)
    # Pass 6: zoom and camera edges.
    pass6 = []
    for fov in (0, 1, 0x3fff, 0x4000, 0x39999, 0x3999a, 100000, 123456, 0x7fffffff,
                -0x80000000, 0xffffffff, 235929, 235930):
        for cid in (0x168, 0x169):
            pass6.append(case(cid, 0, {'fov_c23a28': fov}, 'zoom'))
    for camera in (0, 1, 0x3fffffff, 0x40000000, 0x40000001, 0x7fffffff, -0x7fffffff,
                   -0x30000001, -0x30000000, 0x80000000, 0xffffffff):
        for cid in range(0x19a, 0x1a0):
            pass6.append(case(cid, 0, {'camera_6ba424': camera, 'camera_6ba428': camera,
                                       'camera_6ba42c': camera}, 'camera'))
            pass6.append(case(cid, 0, {'camera_6ba424': camera, 'camera_6ba428': camera,
                                       'camera_6ba42c': camera, 'camera_mode_bd0040': 1},
                              'camera-picture'))
    for seed in (0x3fffffff, 0x40000001, 0x80000000, 0xafffffff, 0xcfffffff, 0xd0000001):
        for cid in (0x19a, 0x19b, 0x19c, 0x19d):
            pass6.append(case(cid, 0, {'camera_6ba424': seed, 'camera_6ba428': seed}, 'camera-seed'))
    compare(pass6)

    report = {'source_sha256': sha, 'cases': case_count, 'command_ids': len(table_ids),
              'gated_cases': gated_cases, 'ids_with_effects': len(effects),
              'mismatch_count': len(mismatches), 'mismatches': mismatches[:10],
              'original_functions': ['0x477D40'],
              'scope': 'Full dispatcher replay: gate variants, every id 0x00..0x1FF, signed values, '
                       'toggle and multiplayer seeds, zoom and camera edges, call order and arguments, '
                       'owned state bytes, and the shipped command table'}
    text = json.dumps(report, indent=2)
    print(text)
    if args.report:
        args.report.write_text(text + '\n')
    return int(bool(mismatches))


if __name__ == '__main__':
    sys.exit(main())
