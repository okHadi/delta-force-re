#!/usr/bin/env python3
"""Compare org0 character callbacks 0x481130 and 0x481330 with original DF1 x86.

The fixture comes from real PLAYER01 data through the probe's fixture mode (first 3DI LOD
and first CHR animation frame), then varied per case: pitch signs, identity fill, attachment
variants, counters, LOD indices, draw context, camera, and attachment matrices.

Original side: Unicorn runs 0x481330 with 0x481130 inside it. 0x46b1d0 and 0x4828c0 are
stubbed so every mesh draw argument, matrix slot, and state write is recorded. Strict counts
cover 0x481130, 0x45d1e0, 0x45c910, 0x45cda0, 0x46b1d0, and 0x4828c0.
"""
import argparse
import hashlib
import os
import json
import random
import struct
import subprocess
import sys
from pathlib import Path

import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_FPCW

SOURCE_HASH = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
MESH_STRIDE = 0x70


def parse_fixture(line):
    values = [int(value) for value in line.split()]
    at = 0

    def s32(value):
        return value - 0x100000000 if value >= 0x80000000 else value

    def take(count):
        nonlocal at
        part = values[at:at + count]
        at += count
        return part

    fixture = {}
    fixture['lod_index'], fixture['counter'], mesh_count, has_character = take(4)
    fixture['mesh_count'] = mesh_count
    fixture['has_character'] = has_character
    fixture['camera'] = take(12)
    fixture['translation'] = [s32(value) for value in take(3)]
    fixture['angles'] = take(3)
    fixture['flags_6c'], fixture['flags_6d'], fixture['weapon_slot'] = take(3)
    if has_character:
        bone_count = take(1)[0]
        fixture['parents'] = [s32(value) for value in take(bone_count)]
        fixture['pose'] = [[s32(value) for value in take(3)] for _ in range(bone_count)]
    else:
        fixture['parents'] = []
        fixture['pose'] = []
    fixture['deltas'] = [[s32(value) for value in take(3)] for _ in range(mesh_count)]
    fixture['weapon_present'], fixture['weapon_type'] = take(2)
    weapon_lods = take(1)[0]
    fixture['weapon_lods'] = [take(1)[0] for _ in range(weapon_lods)] if weapon_lods else []
    fixture['secondary_present'] = take(1)[0]
    secondary_lods = take(1)[0]
    fixture['secondary_lods'] = [take(1)[0] for _ in range(secondary_lods)] if secondary_lods else []
    fixture['matrices'] = [take(12) for _ in range(4)]
    fixture['draw_context'] = take(1)[0]
    if at != len(values):
        raise ValueError('fixture line has trailing fields')
    return fixture


def make_case(rng, fixture, index):
    case = dict(fixture)
    case['lod_index'] = index % 2
    case['counter'] = index % 3
    case['has_character'] = 0 if index % 4 == 3 else 1
    case['angles'] = [rng.randrange(0x1000000), [0, 0x100000, 0x40000000, 0xC0000000, 0xFFFFFFFF][index % 5],
                      rng.randrange(0x1000000)]
    case['translation'] = [rng.randrange(-0x100000, 0x100000) for _ in range(3)]
    case['flags_6c'] = [0, 0x10, 0x10, 0, 0x10][index % 5]
    case['flags_6d'] = [0, 8, 0x10, 0x18, 0x18][index % 5]
    case['weapon_slot'] = [0, 1, 2, 2, 0][index % 5]
    case['draw_context'] = [0, 0x40, 7][index % 3]
    case['camera'] = fixture['camera'] if index % 2 else [
        ((i * 37) % 0x400000) if i in (0, 5, 10) else (i if i % 4 == 3 else 0) for i in range(12)]
    case['weapon_present'], case['weapon_type'] = [(1, 0xfc3), (1, 0xfc4), (1, 0x100), (0, 0), (1, 0xfc3)][index % 5]
    case['weapon_lods'] = [[], [1], [1, 0], [2, 1, 1], [1, 1]][index % 5]
    case['secondary_present'] = [1, 1, 1, 0, 1][index % 5]
    case['secondary_lods'] = [[3], [1], [0, 3], [], [2, 2, 2]][index % 5]
    case['matrices'] = [[(i * 13 + 1) if i in (0, 5, 10) else (index if i % 4 == 3 else 0) for i in range(12)]
                        for _ in range(4)]
    assert case['mesh_count'] > 0
    return case


def encode_case(case):
    output = [case['lod_index'], case['counter'], case['mesh_count'], case['has_character']]
    output.extend(case['camera'])
    output.extend(case['translation'])
    output.extend(case['angles'])
    output.extend([case['flags_6c'], case['flags_6d'], case['weapon_slot']])
    if case['has_character']:
        output.append(len(case['parents']))
        output.extend(case['parents'])
        for angles in case['pose']:
            output.extend(angles)
    for delta in case['deltas'][:case['mesh_count']]:
        output.extend(delta)
    output.extend([case['weapon_present'], case['weapon_type'], len(case['weapon_lods'])])
    output.extend(case['weapon_lods'])
    output.extend([case['secondary_present'], len(case['secondary_lods'])])
    output.extend(case['secondary_lods'])
    for matrix in case['matrices']:
        output.extend(matrix)
    output.append(case['draw_context'])
    return output


def check(executable, probe, model_path, character_path, count):
    source = Path(executable).read_bytes()
    digest = hashlib.sha256(source).hexdigest()
    if digest != SOURCE_HASH:
        raise ValueError('Unsupported original executable hash')
    fixture_line = subprocess.check_output([str(Path(probe).resolve()), 'fixture',
                                            str(Path(model_path).resolve()),
                                            str(Path(character_path).resolve())]).decode()
    fixture = parse_fixture(fixture_line)
    if not fixture['has_character'] or fixture['mesh_count'] < 3:
        raise ValueError('PLAYER01 fixture is missing character meshes')
    rng = random.Random(0x481330)
    cases = [make_case(rng, fixture, index) for index in range(count)]
    payload = [str(len(cases))]
    payload.extend(' '.join(map(str, encode_case(case))) for case in cases)
    lines = subprocess.check_output([str(Path(probe).resolve())],
                                    input=('\n'.join(payload) + '\n').encode()).splitlines()
    if len(lines) != count:
        raise ValueError(f'probe returned {len(lines)} plans for {count} cases')

    u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    u.mem_map(0x400000, 0x1000000)
    u.mem_write(0x400000, pefile.PE(data=source).get_memory_mapped_image())
    u.mem_map(0x2000000, 0x20000)
    u.mem_map(0x3000000, 0x100000)
    u.mem_map(0x6000000, 4096)
    state = {'calls': [], 'counters': {}, 'weapon_types': [], 'frame_writes': [], 'counter_writes': []}

    def word(address, value):
        u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    trace = []

    def call(address, *values):
        u.mem_write(0x200f000, struct.pack('<' + 'I' * (len(values) + 1), 0x6000000, *values))
        u.reg_write(UC_X86_REG_ESP, 0x200f000)
        # Original startup control word. Without it the x87 sine table initializer runs with
        # the Unicorn default and every entry drifts by one unit.
        u.reg_write(UC_X86_REG_FPCW, 0x037F)
        trace.clear()
        try:
            u.emu_start(address, 0x6000000, count=200000000)
        except unicorn.UcError as error:
            if os.environ.get('CHAR_DEBUG'):
                print('trace', [hex(entry) for entry in trace[-24:]], file=sys.stderr)
            raise ValueError(f'original emulation failed: {error}') from error
        if u.reg_read(UC_X86_REG_EIP) != 0x6000000:
            raise ValueError(f'original call did not return: {address:x}')
        return u.reg_read(UC_X86_REG_EAX)

    def hook_count(uc, address, size, user):
        state['counters'][address] = state['counters'].get(address, 0) + 1

    def hook_draw(uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        mesh, table, context, mesh_index = struct.unpack('<IIII', uc.mem_read(esp + 4, 16))
        frame = struct.unpack('<i', uc.mem_read(0xad3cf4, 4))[0]
        matrix = list(struct.unpack('<12i', bytes(uc.mem_read(table + mesh_index * 64, 48))))
        state['calls'].append(dict(mesh=mesh, table=table, context=context, mesh_index=mesh_index,
                                   frame=frame, matrix=matrix))
        uc.reg_write(UC_X86_REG_EAX, 0)
        uc.reg_write(UC_X86_REG_ESP, esp + 4)
        uc.reg_write(UC_X86_REG_EIP, struct.unpack('<I', uc.mem_read(esp, 4))[0])

    def hook_weapon(uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        state['weapon_types'].append(struct.unpack('<I', uc.mem_read(esp + 4, 4))[0])
        uc.reg_write(UC_X86_REG_EAX, 1)  # The checker builds the weapon record at index one.
        uc.reg_write(UC_X86_REG_ESP, esp + 4)
        uc.reg_write(UC_X86_REG_EIP, struct.unpack('<I', uc.mem_read(esp, 4))[0])

    def hook_frame(uc, access, address, size, value, user):
        state['frame_writes'].append(struct.unpack('<i', struct.pack('<I', value & 0xffffffff))[0])

    def hook_counter(uc, access, address, size, value, user):
        state['counter_writes'].append(value & 0xffffffff)

    for address in (0x481130, 0x45d1e0, 0x45c910, 0x45cda0):
        u.hook_add(unicorn.UC_HOOK_CODE, hook_count, None, address, address)
    if os.environ.get('CHAR_DEBUG'):
        def hook_multiply(uc, address, size, user):
            esp = uc.reg_read(UC_X86_REG_ESP)
            ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
            print(f'45cb40 return {ret:#x}', file=sys.stderr)
        u.hook_add(unicorn.UC_HOOK_CODE, hook_multiply, None, 0x45cb40, 0x45cb40)

        def hook_after_attach(uc, address, size, user):
            esp = uc.reg_read(UC_X86_REG_ESP)
            if address == 0x4814dc:
                print('after weapon 45cb40 scratch', struct.unpack('<12i', bytes(uc.mem_read(esp + 0xc, 48))),
                      file=sys.stderr)
            else:
                print('after secondary 45cb40 scratch', struct.unpack('<12i', bytes(uc.mem_read(esp + 0xc, 48))),
                      file=sys.stderr)
        u.hook_add(unicorn.UC_HOOK_CODE, hook_after_attach, None, 0x4814dc, 0x4814dc)
        u.hook_add(unicorn.UC_HOOK_CODE, hook_after_attach, None, 0x481546, 0x481546)

        def hook_trace(uc, address, size, user):
            trace.append(address)
            if len(trace) > 64:
                trace.pop(0)
        u.hook_add(unicorn.UC_HOOK_CODE, hook_trace, None, 0x400000, 0x6000000)
    u.hook_add(unicorn.UC_HOOK_CODE, hook_draw, None, 0x46b1d0, 0x46b1d0)
    u.hook_add(unicorn.UC_HOOK_CODE, hook_weapon, None, 0x4828c0, 0x4828c0)
    u.hook_add(unicorn.UC_HOOK_MEM_WRITE, hook_frame, None, 0xad3cf4, 0xad3cf7)
    u.hook_add(unicorn.UC_HOOK_MEM_WRITE, hook_counter, None, 0xc23800, 0xc23803)

    def hook_unmapped(uc, access, address, size, value, user):
        raise ValueError(f'unmapped access at {address:#x} from {uc.reg_read(UC_X86_REG_EIP):#x}')

    for kind in (unicorn.UC_HOOK_MEM_READ_UNMAPPED, unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,
                 unicorn.UC_HOOK_MEM_FETCH_UNMAPPED):
        u.hook_add(kind, hook_unmapped)

    call(0x477510)  # Original sine and cosine table initializer under control word 0x037F.
    # Compare all 2561 original table entries with the port table. No table is overwritten.
    original_table = struct.unpack('<2561i', u.mem_read(0xbd0068, 2561 * 4))
    cosine_pointer = struct.unpack('<I', u.mem_read(0x4fb2f8, 4))[0]
    if cosine_pointer != 0xbd0068 + 512 * 4:
        raise ValueError(f'cosine table pointer differs: {cosine_pointer:#x}')
    port_table = [int(value) for value in
                  subprocess.check_output([str(Path(probe).resolve()), 'sincos']).decode().split()]
    if len(port_table) != 2561:
        raise ValueError('probe sine table size differs')
    if list(original_table) != port_table:
        first = next(index for index, (got, want) in enumerate(zip(original_table, port_table)) if got != want)
        raise ValueError(f'sine table entry {first} original {original_table[first]} port {port_table[first]}')
    total_draws = 0
    total_writes = 0
    for number, (case, line) in enumerate(zip(cases, lines)):
        mesh_count = case['mesh_count']
        bone_count = len(case['parents']) if case['has_character'] else 0
        instance = 0x3000000
        word(instance + 0x98, mesh_count)
        word(instance + 0x9c, 0x3001000)
        for index, delta in enumerate(case['deltas'][:mesh_count]):
            u.mem_write(0x3001000 + index * MESH_STRIDE + 0x28, struct.pack('<iii', *delta))
        record = 0x3010000
        word(record + 0x98, mesh_count)  # 0x481355 reads the mesh count from the model record.
        word(record + 0x18, 0x3020000 if case['has_character'] else 0)
        word(record + 0x34, case['angles'][1])
        u.mem_write(record + 0x24, struct.pack('<3i', *case['translation']))
        u.mem_write(record + 0x30, struct.pack('<3I', *case['angles']))
        u.mem_write(record + 0x6c, struct.pack('<BB', case['flags_6c'], case['flags_6d']))
        u.mem_write(record + 0x70, struct.pack('<H', case['weapon_slot']))
        word(record + 0x90, 0)  # Clear stale attachment handles from earlier cases.
        word(record + 0x8c, 0)
        if case['has_character']:
            word(0x3020000 + 0x14, bone_count)
            word(0x3020000 + 0x7c, 0x3021000)
            word(0x3020000 + 0x80, 0x3022000)
            for index in range(bone_count):
                word(0x3021000 + index * 4, 0x3023000 + index * 0x40)
                word(0x3023000 + index * 0x40 + 0x38, case['parents'][index] & 0xffffffff)
                u.mem_write(0x3022000 + index * 24, struct.pack('<3i', *case['pose'][index]))
        # Weapon type rule, then the caller registry record.
        weapon_type = 0
        if case['weapon_present']:
            word(record + 0x90, 0x3030000)
            word(0x3030004, 0x3030100)
            word(0x3030120, case['weapon_type'])
            if case['weapon_type'] in (0xfc3, 0xfc4):
                weapon_type = case['weapon_type']
        if case['flags_6c'] & 0x10:
            if case['weapon_slot'] == 1:
                weapon_type = 0xfc3
            elif case['weapon_slot'] == 2:
                weapon_type = 0xfc4
        word(0xc23810, 0x3060000)  # 0xc23810 holds the table base pointer.
        weapon_arrays = []
        if weapon_type:
            weapon_record = 0x3040000
            word(0x3060000 + 0x13c + 0x6c, weapon_record)
            word(weapon_record + 0x14, len(case['weapon_lods']))
            for index, meshes in enumerate(case['weapon_lods']):
                if meshes <= 0:
                    if meshes < 0:
                        word(weapon_record + 0x50 + index * 4, 0)
                    else:
                        lod = 0x3041000 + index * 0x100
                        word(weapon_record + 0x50 + index * 4, lod)
                        word(lod + 0x98, 0)
                        word(lod + 0x9c, lod + 0x40)
                        weapon_arrays.append(lod + 0x40)
                    continue
                lod = 0x3041000 + index * 0x100
                word(weapon_record + 0x50 + index * 4, lod)
                word(lod + 0x98, meshes)
                word(lod + 0x9c, lod + 0x40)
                weapon_arrays.append(lod + 0x40)
        secondary_arrays = []
        if case['secondary_present']:
            secondary_record = 0x3050000
            word(record + 0x8c, 0x3051000)
            word(0x305106c, secondary_record)
            word(secondary_record + 0x14, len(case['secondary_lods']))
            for index, meshes in enumerate(case['secondary_lods']):
                lod = 0x3052000 + index * 0x100
                if meshes < 0:
                    word(secondary_record + 0x50 + index * 4, 0)
                    continue
                word(secondary_record + 0x50 + index * 4, lod)
                word(lod + 0x98, meshes)
                word(lod + 0x9c, lod + 0x40)
                secondary_arrays.append(lod + 0x40)
        u.mem_write(0x8d1340, struct.pack('<12i', *case['camera']))
        for target, matrix in zip((0xc22bc0, 0xc22b00, 0xc22d40, 0xc22b40), case['matrices']):
            u.mem_write(target, struct.pack('<12i', *matrix))
        word(0xc23800, case['counter'])
        state['calls'] = []
        state['counters'] = {}
        state['weapon_types'] = []
        state['frame_writes'] = []
        state['counter_writes'] = []
        call(0x481330, instance, case['draw_context'], record, case['lod_index'])
        plan = [int(value) for value in line.split()]
        at = 0

        def take(count):
            nonlocal at
            part = plan[at:at + count]
            at += count
            return part

        character_flag = take(1)[0]
        native_actor_local = take(12)
        if os.environ.get('CHAR_DEBUG') and number == 1:
            print('native actor_local', native_actor_local, file=sys.stderr)
        take(12)
        table_count = take(1)[0]
        tables = []
        for _ in range(table_count):
            size = take(1)[0]
            tables.append([take(12) for _ in range(size)])
        draw_count = take(1)[0]
        draws = [dict(source=take(1)[0], mesh=take(1)[0], matrix_index=take(1)[0],
                      table=take(1)[0], frame=take(1)[0], context=take(1)[0]) for _ in range(draw_count)]
        writes = take(take(1)[0])
        alternate = take(1)[0]
        native_weapon_type = take(1)[0]
        take(2)  # skip flags are implied by the draw list and lookup counts
        if at != len(plan):
            raise ValueError(f'case {number}: native plan has trailing fields')
        if character_flag != case['has_character']:
            raise ValueError(f'case {number}: character matrix branch differs')
        # Strict call counts.
        calls = state['counters']
        if calls.get(0x481130, 0) != 1:
            raise ValueError(f'case {number}: 481130 call count is not one')
        if calls.get(0x45d1e0, 0) != 1 + bone_count:
            raise ValueError(f'case {number}: 45d1e0 call count differs')
        if calls.get(0x45c910, 0) != 1 + (mesh_count if not case['has_character'] else bone_count):
            raise ValueError(f'case {number}: 45c910 call count differs')
        expected_transforms = 0
        if case['has_character'] and bone_count != 1:
            expected_transforms = sum(1 for parent in case['parents'] if parent != -1)
        if calls.get(0x45cda0, 0) != expected_transforms:
            raise ValueError(f'case {number}: 45cda0 call count differs')
        if len(state['calls']) != len(draws):
            raise ValueError(f'case {number}: draw count differs original {len(state["calls"])} '
                             f'native {len(draws)}')
        if len(state['weapon_types']) != (1 if native_weapon_type else 0):
            raise ValueError(f'case {number}: weapon lookup count differs')
        if state['weapon_types'] and state['weapon_types'][0] != native_weapon_type:
            raise ValueError(f'case {number}: weapon type argument differs '
                             f'original {state["weapon_types"][0]:#x} native {native_weapon_type:#x}')
        if weapon_type and native_weapon_type != weapon_type:
            raise ValueError(f'case {number}: native weapon type differs from the fixture rule')
        # Actor matrix table at 0xc22b80 after the camera multiply.
        table = []
        for index in range(mesh_count):
            table.extend(struct.unpack('<12i', u.mem_read(0xc22b80 + index * 64, 48)))
        native_table = [value for entry in tables[0] for value in entry]
        if table != native_table:
            if os.environ.get('CHAR_DEBUG'):
                for index, (got, want) in enumerate(zip(table, native_table)):
                    if got != want:
                        mesh = index // 12
                        print(f'  matrix entry {index} (mesh {mesh}): original {table[mesh * 12:mesh * 12 + 12]} '
                              f'native {native_table[mesh * 12:mesh * 12 + 12]}', file=sys.stderr)
                        print(f'  camera {case["camera"]} translation {case["translation"]} '
                              f'angles {case["angles"]}', file=sys.stderr)
                        print(f'  delta {case["deltas"][mesh]} parents {case["parents"][:4]} '
                              f'pose {case["pose"][:3]}', file=sys.stderr)
                        break
            raise ValueError(f'case {number}: actor matrix table differs')
        # Ordered 0xad3cf4 writes and the 0xc23800 counter.
        if state['frame_writes'] != writes:
            raise ValueError(f'case {number}: frame writes {state["frame_writes"]} differ from {writes}')
        if state['counter_writes']:
            expected_counter = alternate if alternate >= 0 else case['counter']
            if state['counter_writes'][-1] != expected_counter:
                raise ValueError(f'case {number}: counter write differs')
        elif alternate >= 0:
            raise ValueError(f'case {number}: native advanced the counter without an original write')
        # Every draw argument, matrix slot, and frame.
        for index, (drawn, expected) in enumerate(zip(state['calls'], draws)):
            if drawn['context'] != case['draw_context']:
                raise ValueError(f'case {number}: draw context differs')
            if drawn['mesh_index'] != expected['matrix_index']:
                raise ValueError(f'case {number}: matrix index differs')
            if drawn['frame'] != expected['frame']:
                raise ValueError(f'case {number}: draw frame differs')
            if drawn['matrix'] != tables[expected['table']][expected['matrix_index']]:
                if os.environ.get('CHAR_DEBUG'):
                    print(f'  draw {index}: source {expected["source"]} mesh {hex(drawn["mesh"])} '
                          f'table {hex(drawn["table"])} original {drawn["matrix"]} native '
                          f'{tables[expected["table"]][expected["matrix_index"]]}', file=sys.stderr)
                raise ValueError(f'case {number}: drawn matrix differs')
            if expected['source'] == 0:
                if drawn['mesh'] != 0x3001000 + expected['mesh'] * MESH_STRIDE:
                    raise ValueError(f'case {number}: actor mesh pointer differs')
            elif expected['source'] == 1:
                if drawn['mesh'] != weapon_arrays[case['lod_index']]:
                    raise ValueError(f'case {number}: weapon mesh pointer differs')
            elif expected['source'] == 2:
                if drawn['mesh'] != secondary_arrays[case['lod_index']] + expected['mesh'] * MESH_STRIDE:
                    raise ValueError(f'case {number}: secondary mesh pointer differs')
        total_draws += len(draws)
        total_writes += len(writes)
        print(f'case {number + 1}/{count}: {len(draws)} draws, {len(writes)} frame writes match', flush=True)
    if total_draws == 0 or total_writes == 0:
        raise ValueError('strict case coverage failed')
    return dict(source_sha256=digest, cases=count, compared_cases=count, draws=total_draws,
                frame_writes=total_writes, sine_table_entries=2561, mismatches=0,
                addresses=['481330', '481130', '45d1e0', '45c910', '45cda0', '46b1d0', '4828c0'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable')
    parser.add_argument('--probe', required=True)
    parser.add_argument('--model', default='.local/models/PLAYER01.3DI')
    parser.add_argument('--character', default='.local/PLAYER01.CHR')
    parser.add_argument('--cases', type=int, default=10)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.cases < 1:
        parser.error('--cases must be positive')
    try:
        result = check(args.executable, args.probe, args.model, args.character, args.cases)
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(f'character check failed: {error}', file=sys.stderr)
        return 1
    output = json.dumps(result, indent=2)
    print(output)
    if args.report:
        args.report.write_text(output + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
