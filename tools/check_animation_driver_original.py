#!/usr/bin/env python3
"""Compare the animation driver with the original DF1 x86 code.

Strict replay of:
- 0x42CF40/0x42D100/0x42CF60 action item packing (0x1C-byte slots, item index
  from the compiled table at 0x4EF7C8),
- 0x4872C0 frame event dispatch (low byte only),
- the 0x487370 animation blocks: 0x4877F4 pending action, 0x4873D3-0x487419
  instance writes, tick and dispatch.

The same scripted updates run through tools/animation_driver_probe and every
field must match.
"""
import argparse
import hashlib
import importlib.util
import json
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

import unicorn
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_EFLAGS, UC_X86_REG_EIP, UC_X86_REG_ESI,
    UC_X86_REG_ESP,
)

BASE_DIR = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("check_animation_original", BASE_DIR / "check_animation_original.py")
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)

EXE_SHA256 = base.EXE_SHA256
IMAGE = base.IMAGE
CHR_BASE = base.CHR_BASE
INSTANCE = base.INSTANCE
STACK = base.STACK
OBJECT = base.OBJECT
TEXT_BASE = 0x32000000
CLASS_DEF = 0x33000000
ITEM_TABLE = 0x6BBD48
COMPILED_TABLE = 0x4EF7C8

PENDING_BLOCK = 0x4877F4
PENDING_UNTIL = 0x4873B4
TICK_BLOCK = 0x4873DC
TICK_UNTIL = 0x487419
EVENT_DISPATCH = 0x4872C0
TICK_CALL_AFTER = 0x487408
EFFECT_43F5A0 = 0x43F5A0
EFFECT_45E0F0 = 0x45E0F0
ACTION_TAG_EFFECT = 0x460D20


def trunc_i32(value):
    return int(value) if value >= 0 else -int(-value)


def signed32(value):
    value &= 0xffffffff
    return value - 0x100000000 if value >= 0x80000000 else value


def parse_anm(text):
    actions = []
    for raw in text.split('\r\n'):
        line = raw.strip()
        if not line or line.startswith('//'):
            continue
        tokens = line.split()
        if not tokens or tokens[0].startswith('/') or len(tokens) < 2:
            continue
        number = 0
        token = tokens[1]
        digits = ''
        for index, character in enumerate(token):
            if character in '+-' and index == 0:
                digits += character
            elif character.isdigit():
                digits += character
            else:
                break
        if digits not in ('', '+', '-'):
            number = int(digits)
        velocity = 0.0
        if len(tokens) >= 3 and not tokens[2].startswith('/'):
            velocity = float(tokens[2])
        override = len(tokens) >= 4 and not tokens[3].startswith('/') and tokens[3] == 'override'
        actions.append(dict(name=tokens[0], number=number, velocity=velocity, override=override))
    return actions


def check(executable, probe, chr_path, anm_path, tags_path, report):
    source = Path(executable).read_bytes()
    if hashlib.sha256(source).hexdigest() != EXE_SHA256:
        raise ValueError('unsupported original executable hash')
    machine = DriverOriginal(executable)
    chr_bytes = Path(chr_path).read_bytes()
    machine.load_character(chr_bytes)
    machine.reset_instance()
    anm_text = Path(anm_path).read_bytes().decode('latin1')

    # 1. Strict item packing replay (0x42CF40, 0x4565F0, 0x42CF60, 0x42D159).
    slot = machine.replay_item_table(anm_text)
    rows = machine.compiled_rows()
    actions = parse_anm(anm_text)
    by_name = {action['name']: action for action in actions}
    item_fields = 0
    for row in rows:
        action = by_name.get(row['name'])
        number = action['number'] if action else 0
        velocity = action['velocity'] if action else 0.0
        override = 1 if (action and action['override']) else 0
        expected = [
            (row['B'] << 29) & 0xffffffff,
            number & 0xffffffff,
            trunc_i32(velocity * 65536.0) // 30 if velocity >= 0 else -((-trunc_i32(velocity * 65536.0)) // 30),
            override,
            trunc_i32(row['C'] * 65536.0) & 0xffffffff,
            trunc_i32(row['D'] * 65536.0) & 0xffffffff,
            row['E'] & 0xffffffff,
        ]
        actual = struct.unpack_from('<7I', machine.bytes_at(slot + row['A'] * 0x1c, 0x1c))
        if list(actual) != expected:
            raise ValueError(f'item packing differs for {row["name"]} at index {row["A"]}: '
                             f'original {[hex(v) for v in actual]}, port metadata {[hex(v) for v in expected]}')
        item_fields += 7
    if len(rows) != len(actions):
        raise ValueError(f'compiled row count {len(rows)} differs from ANM actions {len(actions)}')

    # 2. Driver metadata must match the replayed item slots.
    Path(tags_path).write_text(''.join(
        f'{row["name"]} {row["A"]} {row["B"]} {row["E"] & 0xff} {trunc_i32(row["C"]*65536)} {trunc_i32(row["D"]*65536)}\n' for row in rows))
    probe_actions = json.loads(subprocess.check_output(
        [str(Path(probe).resolve()), '--actions-json', str(Path(chr_path).resolve()),
         str(Path(anm_path).resolve()), str(Path(tags_path).resolve())]).decode())
    if len(probe_actions) != len(rows):
        raise ValueError('driver action count differs from compiled rows')
    for row, action in zip(rows, probe_actions):
        item = machine.r32(slot + row['A'] * 0x1c)
        if (action['number'] != machine.r32(slot + row['A'] * 0x1c + 4)
                or action['velocity_per_tick'] != machine.r32(slot + row['A'] * 0x1c + 8)
                or action['override'] != (machine.r32(slot + row['A'] * 0x1c + 12) & 1)
                or action['direction'] != ((item >> 29) & 7)
                or action['tag'] != (machine.r32(slot + row['A'] * 0x1c + 0x18) & 0xff)
                or action['weight_10'] != machine.u32(slot + row['A'] * 0x1c + 0x10)
                or action['aim_14'] != machine.u32(slot + row['A'] * 0x1c + 0x14)):
            raise ValueError(f'driver metadata differs for {row["name"]}')

    # Compare all 256 item slots, using the native compiled table from source.
    lines=[line for line in anm_text.split('\r\n') if line.strip() and not line.lstrip().startswith('/')]
    first=rows[1]['name']
    variants={
        'full':anm_text,
        'empty':'',
        'subset':'\r\n'.join(lines[::13])+'\r\n',
        'duplicate':first+' 5 1.5 override\r\n'+first+' 7\r\n',
        'duplicate_velocity':first+' 5 1.5 override\r\n'+first+' 9 -2 nope\r\n',
        'missing_velocity':first+' 5\r\n',
    }
    full_table_fields=0
    with tempfile.TemporaryDirectory(prefix='dfre-animation-') as folder:
        path=Path(folder)/'case.anm'
        for name,text in variants.items():
            native_slot=machine.replay_item_table(text)
            expected_slots=[list(struct.unpack('<7I',machine.bytes_at(native_slot+i*28,28))) for i in range(256)]
            path.write_bytes(text.encode('latin1'))
            output=subprocess.run([str(Path(probe).resolve()),'--table-json',str(Path(chr_path).resolve()),str(path),'-'],capture_output=True,text=True,check=True)
            actual_slots=json.loads(output.stdout)
            if len(actual_slots)!=256:
                raise ValueError(f'{name}: native item table length differs')
            for index,(want,got) in enumerate(zip(expected_slots,actual_slots)):
                if want!=got:raise ValueError(f'{name}: slot {index} original {want}, native {got}')
                full_table_fields+=7
        unknown='Unknown_Name_For_Original_Check 1 2\r\n'
        original_fault=None
        try:
            unknown_machine=DriverOriginal(executable)
            unknown_machine.replay_item_table(unknown)
        except unicorn.UcError as error:original_fault=str(error)
        if original_fault is None:raise ValueError('Unknown-name original fault was not observed')
        path.write_bytes(unknown.encode('ascii'))
        rejected=subprocess.run([str(Path(probe).resolve()),'--table-json',str(Path(chr_path).resolve()),str(path),'-'],capture_output=True,text=True)
        if rejected.returncode==0 or 'no compiled item slot' not in rejected.stderr:
            raise ValueError('Native unknown-name guard did not reject the input')
    # Restore the full item table before replaying the game-side blocks.
    slot=machine.replay_item_table(anm_text)

    # 3. Event dispatch replay (0x4872C0): low byte only.
    dispatch_calls = machine.replay_dispatch()
    for event in list(range(256)) + [0x1240, 0x4000, 0xff41, 0x4312]:
        expected = machine.expected_dispatch(event & 0xff)
        actual = dispatch_calls[event]
        if actual != expected:
            raise ValueError(f'event dispatch differs for {event:#x}: original {actual}, '
                             f'port {expected}')

    # 4. Block replay: pending action, instance writes, tick, dispatch.
    script = build_script(machine, rows, chr_bytes)
    expected = machine.run_blocks(script, slot)
    native = run_probe(probe, chr_path, anm_path, tags_path, script)
    if len(native) != len(expected['results']):
        raise ValueError('driver trace length differs')
    fields = 0
    for index, (operation, want, got) in enumerate(zip(script, expected['results'], native)):
        for key, value in want.items():
            if key not in got:
                raise ValueError(f'operation {index} ({operation}) missing field {key}')
            if got[key] != value:
                raise ValueError(f'operation {index} ({operation}) field {key}: '
                                 f'original {value}, port {got[key]}')
        fields += len(want)

    result = dict(
        source_sha256=EXE_SHA256,
        executable_function_sha256={
            'event_dispatch': hashlib.sha256(machine.bytes_at(EVENT_DISPATCH, 0x487370 - EVENT_DISPATCH)).hexdigest(),
            'object_update_head': hashlib.sha256(machine.bytes_at(0x487370, 0x487A60 - 0x487370)).hexdigest(),
        },
        item_pack_fields=item_fields,
        complete_item_table_variants=len(variants),
        complete_item_table_fields=full_table_fields,
        unknown_name_original_fault=original_fault,
        unknown_name_native_guard=True,
        compiled_rows=len(rows),
        dispatch_cases=len(dispatch_calls),
        block_operations=len(script),
        block_fields_compared=fields,
        dispatch_calls_recorded=expected['dispatch_calls'],
        tag_effect_calls=expected['tag_effects'],
        tick_events=expected['events'],
        scope=('compiled action item packing (index, direction<<29, atoi number, velocity, '
               'override, default velocities, tag), driver metadata mapping, low-byte event '
               'dispatch 0x12-0x16/0x40-0x43 with high-byte masking, 0x4877F4 pending action '
               'with 0x42CEF0/0x43A150, instance writes 0x28/0x2C/0x60/0x34, tick 0x42CE40, '
               'event calls 0x43F5A0/0x45E0F0, item tag effect 0x460D20'),
        mismatches=0,
    )
    if report:
        Path(report).write_text(json.dumps(result, indent=2) + '\n')
    return result


class DriverOriginal(base.Original):
    def __init__(self, executable):
        super().__init__(executable)
        for address, size in ((TEXT_BASE, 0x40000), (CLASS_DEF, 0x1000)):
            self.machine.mem_map(address, size)
        self.calls = []
        # Caller-cleans stubs for the dispatch targets; hooks record their args.
        for address in (EFFECT_43F5A0, EFFECT_45E0F0, ACTION_TAG_EFFECT):
            self.machine.mem_write(address, b'\xc3')
        def record(_machine, address, _size, _data):
            if address == EFFECT_43F5A0:
                self.calls.append(('43f5a0', _machine.reg_read(UC_X86_REG_ESP)))
            elif address == EFFECT_45E0F0:
                self.calls.append(('45e0f0', _machine.reg_read(UC_X86_REG_ESP)))
            else:
                self.calls.append(('tag', _machine.reg_read(UC_X86_REG_ESP)))
        self.machine.hook_add(unicorn.UC_HOOK_CODE, record,
                              begin=EFFECT_43F5A0, end=EFFECT_43F5A0 + 4)
        self.machine.hook_add(unicorn.UC_HOOK_CODE, record,
                              begin=EFFECT_45E0F0, end=EFFECT_45E0F0 + 4)
        self.machine.hook_add(unicorn.UC_HOOK_CODE, record,
                              begin=ACTION_TAG_EFFECT, end=ACTION_TAG_EFFECT + 4)
        self.events = None
        def capture_events(_machine, address, _size, _data):
            self.events = _machine.reg_read(UC_X86_REG_EAX) & 0xffff
        self.machine.hook_add(unicorn.UC_HOOK_CODE, capture_events,
                              begin=TICK_CALL_AFTER, end=TICK_CALL_AFTER)

    def write_text(self, address, text):
        data = text.encode('latin1') + b'\0'
        self.machine.mem_write(address, data)

    def compiled_rows(self):
        rows = []
        for index in range(256):
            data = self.bytes_at(COMPILED_TABLE + index * 0x34, 0x34)
            name = data[:32].split(b'\0')[0].decode('latin1')
            if not name:
                continue
            A, B = struct.unpack_from('<ii', data, 0x20)
            C, D = struct.unpack_from('<ff', data, 0x28)
            E = struct.unpack_from('<i', data, 0x30)[0]
            rows.append(dict(name=name, slot=index, A=A, B=B, C=C, D=D, E=E))
        return rows

    def replay_item_table(self, text):
        """Strict packing chain: 0x42CF40 init, 0x4565F0 tokenizer per line,
        0x42CF60 line handler, then the 0x42D159 postprocess loop. Only the
        0x456720 file reader is bypassed (its import stubs are unresolved in
        emulation); the tokenizer and handler run as shipped."""
        self.machine.mem_write(ITEM_TABLE, bytes(0x1c104))
        self.call(0x42CF40)
        self.w32(0x6BBD44, ITEM_TABLE)
        line_address, record = TEXT_BASE + 0x10000, TEXT_BASE + 0x20000
        for raw in text.split('\r\n'):
            self.machine.mem_write(line_address, raw.encode('latin1') + b'\0')
            self.machine.mem_write(record, bytes(0x80))
            self.call(0x4565F0, {UC_X86_REG_ESI: record}, (line_address,))
            count = self.r32(record)
            if count == 0:
                continue
            if self.r8(self.u32(record + 4)) == 0x2F:
                continue
            self.call(0x42CF60, {}, (record,))
        self.machine.emu_start(0x42D159, 0x42D190, count=10000000)
        if self.machine.reg_read(UC_X86_REG_EIP) != 0x42D190:
            raise ValueError('item postprocess loop did not finish')
        return ITEM_TABLE

    def call_status(self, address, registers=None, stack_args=()):
        self.call(address, registers, stack_args)
        return self.machine.reg_read(UC_X86_REG_EAX)

    def expected_dispatch(self, event):
        table = {
            0x12: ('43f5a0', 7, 0), 0x13: ('43f5a0', 8, 0), 0x14: ('43f5a0', 9, 0),
            0x15: ('43f5a0', 10, 0), 0x16: ('43f5a0', 11, 0),
            0x40: ('45e0f0', 1, 1), 0x41: ('45e0f0', 1, 0),
            0x42: ('45e0f0', 0, 1), 0x43: ('45e0f0', 0, 0),
        }
        return table.get(event, ('none', 0, 0))

    def replay_dispatch(self):
        calls = {}
        for event in list(range(256)) + [0x1240, 0x4000, 0xff41, 0x4312]:
            self.calls = []
            self.call(EVENT_DISPATCH, {}, (event, OBJECT))
            record = ('none', 0, 0)
            if self.calls:
                which, esp = self.calls[0]
                if which == '43f5a0':
                    record = ('43f5a0', self.r32(esp + 8), 0)
                elif which == '45e0f0':
                    record = ('45e0f0', self.r32(esp + 4), self.r32(esp + 8))
            calls[event] = record
        return calls

    def set_object(self, pending, current, heading):
        self.w32(OBJECT + 4, CLASS_DEF)
        self.w32(CLASS_DEF + 0x78, ITEM_TABLE)
        self.w32(CLASS_DEF + 0xD4, 0)
        self.w32(OBJECT + 0x18, INSTANCE)
        self.w32(OBJECT + 0x7C, pending)
        self.w32(OBJECT + 0x80, current)
        self.w32(OBJECT + 0x30, heading)
        self.w32(OBJECT + 0x24, 0)
        self.w32(OBJECT + 0x28, 0)
        self.w32(OBJECT + 0x2C, 0)

    def run_pending_block(self):
        # 0x4877F4 expects EAX = [obj+0x7C] (loaded at 0x4873A6 before the compare).
        pending = self.r32(OBJECT + 0x7C)
        self.calls = []
        self.run_block(PENDING_BLOCK, PENDING_UNTIL, {UC_X86_REG_EBX: OBJECT,
                                                      UC_X86_REG_ESI: ITEM_TABLE,
                                                      UC_X86_REG_EAX: pending,
                                                      UC_X86_REG_ESP: STACK + 0x7000})
        tag = any(which == 'tag' for which, _ in self.calls)
        return tag

    def run_tick_block(self, heading):
        del heading
        self.calls = []
        self.events = None
        esp = STACK + 0x7000
        self.w32(esp + 0x3C, INSTANCE)
        self.run_block(TICK_BLOCK, TICK_UNTIL, {UC_X86_REG_EBX: OBJECT,
                                                UC_X86_REG_ESP: esp})
        # 0x42ce40 frame: entry_esp = esp-4 (push ebx) -4 (call); ebp = entry-16,
        # move vector at ebp-0x20; only a nonzero write is remembered.
        move = self.r32(esp - 8 - 0x30 + 4)
        if move:
            self.move_value = move
        return self.events

    def run_block(self, start, until, registers):
        machine = self.machine
        for register, value in registers.items():
            machine.reg_write(register, value)
        machine.emu_start(start, until, count=10000000)
        if machine.reg_read(UC_X86_REG_EIP) != until:
            raise ValueError(f'block {start:#x} did not reach {until:#x}')

    def event_slots(self, chr_bytes):
        """Distinct dispatched event bytes (frame end low byte) with the earliest
        slot and frame index. The low byte is byte A at frame end (0x4873D3
        frame-end write) and the gated bytes mid-frame."""
        data = chr_bytes
        table = struct.unpack_from('<I', data, 20)[0]
        found = {}
        for slot in range(255):
            pointer = struct.unpack_from('<I', data, table + slot * 4)[0]
            if not pointer:
                continue
            count = struct.unpack_from('<I', data, pointer)[0]
            frame = pointer + 20
            for index in range(count):
                byte = data[frame + 0x0A]  # frame end: byte A | byte B << 8 (low byte = A)
                if byte and byte not in found:
                    found[byte] = (slot, index)
                frame += struct.unpack_from('<I', data, frame)[0]
        return sorted(((slot, byte, frame) for byte, (slot, frame) in found.items()),
                      key=lambda item: item[2])

    def run_blocks(self, script, slot):
        results = []
        dispatch_calls = 0
        tag_effects = 0
        events = 0
        fields = 0
        for operation in script:
            fields += self.run_block_operation(operation, results, slot)
        for entry in results:
            dispatch_calls += 1 if entry.get('dispatch_target', 'none') != 'none' else 0
            tag_effects += 1 if entry.get('tag_effect') else 0
            events += 1 if entry.get('events') else 0
        return {'results': results, 'fields': fields, 'dispatch_calls': dispatch_calls,
                'tag_effects': tag_effects, 'events': events}

    def run_block_operation(self, operation, results, slot):
        parts = operation.split()
        if parts[0] == 'init':
            self.reset_instance()
            self.set_object(0, 0, 0)
            state = self.state()
            state['action_index'] = 0
            state.pop('changed', None)  # the driver reports action_changed separately
            results.append(state)
            return len(state)
        values = [int(value, 0) for value in parts[1:]]
        (flags, pending, heading, a24, a28, a2c, anim_flags, stance, pitch, aim_a, aim_b, byte78,
         byte79, switch_flag, extra_4c) = values
        self.w32(INSTANCE + 0x48, anim_flags)
        self.w8(INSTANCE + 0x64, stance)
        self.w8(INSTANCE + 0x65, pitch)
        self.w8(INSTANCE + 0x78, byte78)
        self.w8(INSTANCE + 0x79, byte79)
        self.w8(INSTANCE + 0x7A, aim_a)
        self.w8(INSTANCE + 0x7B, aim_b)
        self.w8(INSTANCE + 0x66, switch_flag)
        self.w32(INSTANCE + 0x4C, extra_4c)
        previous_action = self.r32(OBJECT + 0x80)
        self.set_object(pending, previous_action, heading)
        tag_effect = False
        action_started = pending != previous_action
        if action_started:
            tag_effect = self.run_pending_block()
        current = self.r32(OBJECT + 0x80)
        item = self.r32(slot + current * 0x1c)
        result = {
            'action_started': 1 if action_started else 0,
            'action_index': current,
            'animation_slot': self.r32(slot + current * 0x1c + 4),
            'direction': item & 0xffffffff,  # item+0x00 packing, compared literally
            'move_heading': signed32(heading + item),
            'override': 1 if self.r32(slot + current * 0x1c + 12) else 0,
            'tag_effect': 1 if tag_effect else 0,
            'ticked': 1 if (flags & 1) else 0,
            'events': 0,
            'dispatch_target': 'none', 'dispatch_value': 0, 'dispatch_value2': 0,
            'action_changed': 0,
        }
        if flags & 1:
            self.observed_carry = 0
            events = self.run_tick_block(heading)
            result['events'] = events or 0
            result['action_changed'] = self.observed_carry
            record = ('none', 0, 0)
            for which, esp in self.calls:
                if which == '43f5a0':
                    record = ('43f5a0', self.r32(esp + 8), 0)
                elif which == '45e0f0':
                    record = ('45e0f0', self.r32(esp + 4), self.r32(esp + 8))
            result['dispatch_target'], result['dispatch_value'], result['dispatch_value2'] = record
        result.update(self.state())
        result.pop('changed', None)  # the driver reports action_changed separately
        results.append(result)
        return len(result)


def build_script(machine, rows, chr_bytes):
    by_number = {}
    for row in rows:
        by_number.setdefault(machine.r32(ITEM_TABLE + row['A'] * 0x1c + 4), []).append(row['A'])
    script = ['init']
    script.append(update_line(1, 1, 0))
    script.append(update_line(1, 1, 0))
    script.append(update_line(2, 1, 0))           # bit 0 clear: no tick
    script.append(update_line(1, 10, 0x80))       # tag 2 item: no side effect
    script.append(update_line(1, 250, 0x123400))  # tag 1 item: 0x460D20
    script.append(update_line(1, 250, -0x200000))
    for _ in range(4):
        script.append(update_line(1, 250, 0x40))
    script.append(update_line(1, 255, 0))         # death by fire (tag 1)
    for _ in range(6):
        script.append(update_line(1, 255, 0))
    script.append(update_line(1, 8, 0))           # reserved index: plays slot 0
    script.append(update_line(1, 12, 0))          # item order differs from file order
    for _ in range(3):
        script.append(update_line(1, 12, 0))
    # Conditions with varied actor/weapon fields.
    for flags in (1, 3):
        script.append(update_line(flags, 79, 0x2000, anim_flags=0x28, stance=3, pitch=0x20,
                                  aim_a=1, byte79=1, extra_4c=-0x4000))
        script.append(update_line(flags, 123, 0x4000, anim_flags=0x60, stance=7, pitch=10,
                                  aim_b=1))
    # Every observed event byte through a real action: the frame end delivers
    # byte A, so tick frame+2 times to pass the frame and its end.
    for slot, byte, frame in machine.event_slots(chr_bytes):
        del byte
        candidates = by_number.get(slot, [])
        if not candidates:
            continue
        start = candidates[0]
        script.append(update_line(1, start, 0))
        for _ in range(frame + 2):
            script.append(update_line(1, start, 0))
    return script


def update_line(flags, pending, heading, anim_flags=0, stance=0, pitch=0, aim_a=0, aim_b=0,
                byte78=0, byte79=0, switch_flag=0, extra_4c=0):
    return ('update %d %d %d 0 0 0 %d %d %d %d %d %d %d %d %d'
            % (flags, pending, heading, anim_flags, stance, pitch, aim_a, aim_b, byte78, byte79,
               switch_flag, extra_4c))


def run_probe(probe, chr_path, anm_path, tags_path, script):
    output = subprocess.check_output(
        [str(Path(probe).resolve()), '--trace-json', str(Path(chr_path).resolve()),
         str(Path(anm_path).resolve()), str(Path(tags_path).resolve())],
        input=('\n'.join(script) + '\n').encode())
    return [json.loads(line) for line in output.decode().splitlines()]


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable')
    parser.add_argument('--probe', required=True)
    parser.add_argument('--chr', required=True)
    parser.add_argument('--anm', required=True)
    parser.add_argument('--tags', default='.local/scratch/driver/playanim_tags.txt')
    parser.add_argument('--report', type=Path)
    arguments = parser.parse_args()
    outcome = check(arguments.executable, arguments.probe, arguments.chr, arguments.anm,
                    arguments.tags, arguments.report)
    print(json.dumps(outcome, indent=2))
