#!/usr/bin/env python3
"""Check the resource plan against original DF1 code and data. Read game files only.

Expected calls, steps, removals, and notes are rebuilt in Python from the
original constants (0x402EC0 soldier pairs, 0x483CD0 weapon passes and fixed
items, 0x45DD40 effect list, 0x402E00 squad removal, 0x402E70 powerup removal)
and the stock item, weapon, and mission files. Loader banks are emulated from
0x45D6F0 under Unicorn with stubbed loaders; the ITEMS.DEF text is decoded by
the original 0x456EB0 transform under Unicorn. The probe output must match all
rebuilt fields.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP,
)
from pff import Archive

SHA = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
IMAGE_BASE = 0x400000
WEAPON_TABLE_VA = 0x4F9934
WEAPON_COUNT = 39
WEAPON_STRIDE = 0xA8

TYPE_NUMBERS = {'person': 3, 'vehicle': 1, 'building': 5, 'decoration': 2,
                'marker': 4, 'object': 6, 'effect': 8}
BANKS = {1: 1, 2: 2, 3: 3, 5: 4, 6: 5}
SOLDIER_PAIRS = {('snow', 'night'): (5046, 5047), ('snow', 'day'): (5010, 5011),
                 ('plain', 'night'): (5044, 5045), ('plain', 'day'): (5003, 5004)}
EFFECT_IDS = [7016, 7015, 7013, 7014] + list(range(7001, 7013)) + [4046, 4051] + \
    list(range(4058, 4063)) + list(range(4064, 4071)) + [4075]
WEAPON_FIXED = [4014, 4040, 4020]
SQUAD_IDS = (5002, 5038, 5048, 5049)
STAGE_NAMES = ('soldier', 'weapons', 'effects', 'group0', 'group1', 'group3', 'group2')


def raw_offset(pe, va):
    return pe.get_offset_from_rva(va - IMAGE_BASE)


def code_bytes(exe, pe, start, end):
    return exe[raw_offset(pe, start):raw_offset(pe, end)]


def contains_constant(exe, pe, start, end, value):
    return struct.pack('<I', value) in code_bytes(exe, pe, start, end)


def read_weapon_table(exe, pe):
    data = exe[raw_offset(pe, WEAPON_TABLE_VA):
               raw_offset(pe, WEAPON_TABLE_VA) + WEAPON_COUNT * WEAPON_STRIDE]
    return [(struct.unpack_from('<I', data, i * WEAPON_STRIDE + 0x64)[0],
             struct.unpack_from('<I', data, i * WEAPON_STRIDE + 0x68)[0])
            for i in range(WEAPON_COUNT)]


def emulate_banks(exe, pe):
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(IMAGE_BASE, 0xa00000)
    cpu.mem_write(IMAGE_BASE, pe.get_memory_mapped_image())
    cpu.mem_map(0x2000000, 0x100000)
    table, record, return_address = 0x2800000, 0x2808000, 0x200F000
    cpu.mem_map(0x2800000, 0x10000)
    for stub in (0x486530, 0x42cbb0, 0x42d200, 0x4818f0, 0x481a00):
        cpu.mem_write(stub, b'\xc3')
    cpu.mem_write(0xc23810, struct.pack('<I', table))
    cpu.mem_write(0xc23814, struct.pack('<I', 1))
    calls = []

    def name_at(address):
        return bytes(cpu.mem_read(address, 16)).split(b'\0')[0]

    def hook(uc, address, size, user):
        if address not in (0x486530, 0x42cbb0, 0x42d200):
            return
        esp = uc.reg_read(UC_X86_REG_ESP)
        argument = struct.unpack('<I', uc.mem_read(esp + 4, 4))[0]
        if address == 0x486530:
            bank = struct.unpack('<I', uc.mem_read(esp + 8, 4))[0]
            flag = struct.unpack('<I', uc.mem_read(esp + 0xc, 4))[0]
            uc.mem_write(flag, bytes(4))
            uc.reg_write(UC_X86_REG_EAX, record)
            calls.append(('model', name_at(argument), bank))
        else:
            calls.append(('chr' if address == 0x42cbb0 else 'anim', name_at(argument), None))
            uc.reg_write(UC_X86_REG_EAX, 0)

    cpu.hook_add(UC_HOOK_CODE, hook)
    results = {}
    for type_number in range(0, 10):
        for color32 in (True, False):
            cpu.mem_write(table, bytes(0x13c))
            cpu.mem_write(table + 0x20, struct.pack('<I', 100000 + type_number))
            cpu.mem_write(table + 0x28, struct.pack('<I', type_number))
            cpu.mem_write(table + 0x2c, b'gmodel\0')
            cpu.mem_write(table + 0x4c, b'cmodel\0')
            cpu.mem_write(table + 0x5c, b'amodel\0')
            cpu.mem_write(0x503138, struct.pack('<I', 1 if color32 else 0))
            stack = 0x2008000
            cpu.mem_write(stack, struct.pack('<II', return_address, 0))
            cpu.reg_write(UC_X86_REG_ESP, stack)
            calls.clear()
            cpu.emu_start(0x45d6f0, return_address, count=20000)
            if cpu.reg_read(UC_X86_REG_EIP) != return_address:
                raise AssertionError('0x45D6F0 did not return for type %d' % type_number)
            results[(type_number, color32)] = list(calls)
    return results


def decode_script(pe, data):
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0x456000, 0x1000)
    cpu.mem_write(0x456000, pe.get_data(0x56000, 0x1000))
    cpu.mem_map(0x2000000, 0x10000)
    cpu.mem_map(0x3000000, 0x100000)
    cpu.mem_map(0x4000000, 0x1000)
    cpu.mem_write(0x3000000, data)
    cpu.mem_write(0x2000800, struct.pack('<4I', 0x4000000, 0x3000000, len(data), 0x04960552))
    cpu.reg_write(UC_X86_REG_ESP, 0x2000800)
    cpu.emu_start(0x456eb0, 0x4000000, count=20 * len(data) + 100)
    if cpu.reg_read(UC_X86_REG_EIP) != 0x4000000:
        raise AssertionError('Original script decoder did not return')
    return bytes(cpu.mem_read(0x3000000, len(data)))


def read_items(game, pe):
    for archive_name in ('Dfbase.pff', 'Df.pff'):
        archive = Archive(game / archive_name)
        with (game / archive_name).open('rb') as source:
            for entry in archive.entries:
                if not entry.deleted and entry.name.upper() == 'ITEMS.DEF':
                    source.seek(entry.offset)
                    text = decode_script(pe, source.read(entry.size)).decode('latin-1')
                    return parse_items(text)
    raise AssertionError('ITEMS.DEF is missing')


def parse_items(text):
    items = []
    current = None
    for line in text.splitlines():
        if line.lstrip().startswith('//'):
            continue
        line = line.split('/')[0]
        words = line.split()
        if not words:
            continue
        command = words[0].lower()
        if command == 'begin':
            current = {'name': words[1].strip('"') if len(words) > 1 else '',
                       'source_id': None, 'type': '', 'fields': {}}
        elif command == 'end':
            if current is not None:
                if current['source_id'] is None or not current['type']:
                    raise AssertionError('Incomplete item record')
                current['rid'] = (current['source_id'] - 100000) & 0xffffffff
                items.append(current)
                current = None
        elif current is not None:
            value = ' '.join(words[1:])
            if command == 'id':
                current['source_id'] = int(words[1])
            elif command == 'type':
                current['type'] = words[1].lower()
            else:
                current['fields'].setdefault(command, []).append(value)
    if not items:
        raise AssertionError('No item records')
    return items


def has_powerup(item):
    return any(value.lower() == 'powerup'
               for value in ' '.join(item['fields'].get('attrib:', [])).split())


def item_field(item, key):
    values = item['fields'].get(key)
    return values[0].split()[0] if values else ''


class Planner:
    def __init__(self, items, weapons, options):
        self.items = items
        self.weapons = weapons
        self.options = options
        self.calls, self.steps, self.removals, self.notes = [], [], [], []
        self.model_names, self.chr_names, self.anim_names = set(), set(), set()
        self.settled_graphic, self.settled_husk = set(), set()
        self.null_lookups = self.skipped_nulls = 0

    def find_index(self, rid):
        for index, item in enumerate(self.items):
            if item['rid'] == rid:
                return index, True
        return 0, False

    def bank(self, item):
        number = TYPE_NUMBERS.get(item['type'], 0)
        return BANKS.get(number, 0) if self.options['color32'] else 0

    def emit_step(self, stage, kind, item, index, slot, name, bank, registry):
        step = {'stage': stage, 'kind': kind, 'id': item['rid'], 'index': index, 'slot': slot,
                'name': name, 'bank': bank, 'first': name.lower() not in registry}
        registry.add(name.lower())
        self.steps.append(step)

    def load_item(self, stage, slot, item, index):
        number = TYPE_NUMBERS.get(item['type'], 0)
        bank = self.bank(item)
        graphic = item_field(item, 'graphic')
        if graphic and index not in self.settled_graphic:
            self.settled_graphic.add(index)
            self.emit_step(stage, 'model', item, index, slot, graphic, bank, self.model_names)
            if number == 3:
                character = item_field(item, 'chr_file')
                if character:
                    self.emit_step(stage, 'character', item, index, slot, character, bank,
                                   self.chr_names)
                animation = item_field(item, 'anim_def')
                if animation:
                    self.emit_step(stage, 'animation', item, index, slot, animation, bank,
                                   self.anim_names)
        husk = item_field(item, 'husk')
        if husk and index not in self.settled_husk:
            self.settled_husk.add(index)
            self.emit_step(stage, 'husk', item, index, slot, husk, bank, self.model_names)

    def call(self, stage, slot, rid, chr_flag):
        index, found = self.find_index(rid)
        if not found:
            self.null_lookups += 1
        self.calls.append({'stage': stage, 'id': rid, 'index': index, 'slot': slot,
                           'found': found, 'chr': chr_flag})
        self.load_item(stage, slot, self.items[index], index)

    def direct(self, stage, index):
        item = self.items[index]
        self.calls.append({'stage': stage, 'id': item['rid'], 'index': index, 'slot': None,
                           'found': True, 'chr': False})
        self.load_item(stage, None, item, index)

    def run_group(self, stage, group, chr_flag):
        for index, (rid, flags) in enumerate(group):
            if flags & 0x90:
                continue
            item_index, _ = self.find_index(rid)
            if item_index == 0:
                self.skipped_nulls += 1
                continue
            self.call(stage, index, rid, chr_flag)

    def soldiers(self, theme, night):
        snow = theme == 2
        key = ('snow' if snow else 'plain', 'night' if night else 'day')
        first, second = SOLDIER_PAIRS[key]
        self.call('soldier', None, first, False)
        self.call('soldier', None, second, False)

    def weapon_stage(self):
        for player, _ in self.weapons:
            self.call('weapons', None, player, False)
        for _, world in self.weapons:
            self.call('weapons', None, world, False)
        for rid in WEAPON_FIXED:
            self.call('weapons', None, rid, False)

    def effect_stage(self):
        self.direct('effects', 0)
        for rid in EFFECT_IDS:
            self.call('effects', None, rid, False)

    def mission_stages(self, groups):
        self.run_group('group0', groups[0], False)
        self.run_group('group1', groups[1], False)
        self.run_group('group3', groups[3], True)
        if self.options['squad']:
            for index, (rid, flags) in enumerate(groups[3]):
                if flags & 0x90:
                    continue
                item = self.items[self.find_index(rid)[0]]
                if item['rid'] in SQUAD_IDS:
                    self.removals.append({'stage': 'group3', 'slot': index, 'id': item['rid'],
                                          'callee': 0x45DA30, 'reason': 'squad'})
        if self.options['powerup']:
            for index, (rid, flags) in enumerate(groups[0]):
                if flags & 0x90:
                    continue
                item = self.items[self.find_index(rid)[0]]
                if has_powerup(item):
                    self.removals.append({'stage': 'group0', 'slot': index, 'id': item['rid'],
                                          'callee': 0x45DA30, 'reason': 'powerup'})
        if self.options['multiplayer']:
            self.notes.append({'stage': 'group2',
                               'text': 'multiplayer flag set; plan keeps the single-player object '
                                       'filter and removal gates'})
        self.notes.append({'stage': 'group2',
                           'text': '0x402EC0 does not preload group 2 objects; stock markers '
                                   'have no graphic or husk'})
        for index, (rid, flags) in enumerate(groups[2]):
            if flags & 0x90:
                continue
            item = self.items[self.find_index(rid)[0]]
            if item_field(item, 'graphic') or item_field(item, 'husk'):
                self.notes.append({'stage': 'group2',
                                   'text': 'group2 slot %d item %d has a graphic; 0x402EC0 does '
                                           'not preload group 2' % (index, item['rid'])})

    def result(self):
        return {'calls': self.calls, 'steps': self.steps, 'removals': self.removals,
                'notes': self.notes, 'null_lookups': self.null_lookups,
                'skipped_nulls': self.skipped_nulls}


def read_missions(game):
    missions = {}
    for archive_name in ('Dfbase.pff', 'Df.pff'):
        archive = Archive(game / archive_name)
        with (game / archive_name).open('rb') as source:
            for entry in archive.entries:
                if entry.deleted or not entry.name.lower().endswith('.bms'):
                    continue
                if any(name.upper() == entry.name.upper() for name in missions):
                    continue
                source.seek(entry.offset)
                data = source.read(entry.size)
                counts = struct.unpack_from('<4I', data, 152)
                groups = [[] for _ in range(4)]
                offset = 268
                for group in range(4):
                    for _ in range(counts[group]):
                        groups[group].append((struct.unpack_from('<I', data, offset)[0],
                                              struct.unpack_from('<I', data, offset + 12)[0]))
                        offset += 96
                missions[entry.name] = {
                    'theme': struct.unpack_from('<I', data, 0x74)[0],
                    'night': (data[0x7A] & 0x10) != 0,
                    'groups': groups}
    return missions


def check_constants(exe, pe):
    soldier_ids = [rid for pair in SOLDIER_PAIRS.values() for rid in pair]
    for rid in set(soldier_ids):
        if not contains_constant(exe, pe, 0x402EC0, 0x403160, rid):
            raise AssertionError('Soldier id 0x%X is not in 0x402EC0' % rid)
    for rid in EFFECT_IDS:
        if not contains_constant(exe, pe, 0x45DD40, 0x45E100, rid):
            raise AssertionError('Effect id 0x%X is not in 0x45DD40' % rid)
    for rid in WEAPON_FIXED:
        if not contains_constant(exe, pe, 0x483CD0, 0x483E00, rid):
            raise AssertionError('Weapon id 0x%X is not in 0x483CD0' % rid)
    for rid in SQUAD_IDS:
        if not contains_constant(exe, pe, 0x402E00, 0x402E70, rid):
            raise AssertionError('Squad id 0x%X is not in 0x402E00' % rid)


def check(game, probe, options, limit):
    exe = (game / 'Df.exe').read_bytes()
    if hashlib.sha256(exe).hexdigest() != SHA:
        raise ValueError('Unsupported original executable hash')
    pe = pefile.PE(data=exe)
    check_constants(exe, pe)

    banks = emulate_banks(exe, pe)
    for color32 in (True, False):
        for type_number in range(0, 10):
            sequence = banks[(type_number, color32)]
            expected = BANKS.get(type_number, 0) if color32 else 0
            if sequence[0][0] != 'model' or sequence[0][2] != expected:
                raise AssertionError('Bank for type %d differs' % type_number)
            names = [entry[0] for entry in sequence]
            if type_number == 3:
                if names != ['model', 'chr', 'anim']:
                    raise AssertionError('Type 3 loader order changed')
            elif names != ['model']:
                raise AssertionError('Unexpected loader calls for type %d' % type_number)

    items = read_items(game, pe)
    weapon_table = read_weapon_table(exe, pe)
    stock = json.loads(subprocess.check_output([str(probe.resolve()), str(game), '--weapons-only']))
    if [(entry['player'], entry['world']) for entry in stock] != weapon_table:
        raise AssertionError('Probe stock weapon table differs from 0x4F9934')

    missions = read_missions(game)
    probe_options = 'color32=%d,multiplayer=%d,squad=%d,powerup=%d,patch=%d' % (
        options['color32'], options['multiplayer'], options['squad'], options['powerup'],
        options['patch'])
    rows = json.loads(subprocess.check_output(
        [str(probe.resolve()), str(game), '--all', '--options', probe_options], timeout=600))
    if len(rows) != len(missions):
        raise AssertionError('Probe mission count differs from the corpus')
    checked = calls = steps = 0
    for row in rows:
        name = row['mission']
        if name not in missions:
            raise AssertionError('Probe returned unknown mission ' + name)
        if limit and name not in set(sorted(missions)[:limit]):
            continue
        mission = missions[name]
        planner = Planner(items, weapon_table, options)
        planner.soldiers(mission['theme'], mission['night'])
        planner.weapon_stage()
        planner.effect_stage()
        planner.mission_stages(mission['groups'])
        expected = planner.result()
        if row['theme'] != mission['theme'] or row['night'] != mission['night']:
            raise AssertionError('Mission header differs for ' + name)
        for key in ('calls', 'steps', 'removals', 'notes', 'null_lookups', 'skipped_nulls'):
            if row[key] != expected[key]:
                raise AssertionError('Resource plan %s differs for %s' % (key, name))
        checked += 1
        calls += len(row['calls'])
        steps += len(row['steps'])
    constants = {'soldier': len({rid for pair in SOLDIER_PAIRS.values() for rid in pair}),
                 'effects': len(EFFECT_IDS), 'weapon_fixed': len(WEAPON_FIXED),
                 'squad': len(SQUAD_IDS)}
    return {'source_sha256': SHA, 'missions': checked, 'calls': calls, 'steps': steps,
            'weapon_records': len(weapon_table), 'banks_emulated': len(banks),
            'checked_constants': constants,
            'scope': 'single-player ordered plan from stock items, weapons, and missions; '
                     'loader banks and type 3 loader order emulated from 0x45D6F0; '
                     'stage constants verified in original code ranges'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--limit', type=int, default=0)
    parser.add_argument('--options', default='color32=1,multiplayer=0,squad=0,powerup=1,patch=1')
    args = parser.parse_args()
    parsed = {}
    for item in args.options.split(','):
        key, value = item.split('=')
        parsed[key] = bool(int(value))
    result = check(args.game, args.probe, parsed, args.limit)
    output = json.dumps(result, indent=2)
    print(output)
    if args.report:
        args.report.write_text(output + '\n')
