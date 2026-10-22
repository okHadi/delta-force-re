#!/usr/bin/env python3
"""Compare the animation controller port with the original DF1 x86 code.

The controller functions 0x43a0e0/0x43a150/0x43a7a0 (with 0x43a690, 0x43a2c7,
0x43a4dc, 0x43aa30, 0x43ac10, 0x43ac40) are executed in unicorn against a
relocated CHR image and an instance built like 0x439e48; the same script runs
through tools/animation_probe and every state field must match.
"""
import argparse
import hashlib
import json
import struct
import subprocess
import sys
from pathlib import Path

import pefile
import unicorn
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_EFLAGS, UC_X86_REG_EIP, UC_X86_REG_ESI,
    UC_X86_REG_ESP,
)

EXE_SHA256 = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
IMAGE = 0x400000
CHR_BASE = 0x20000000
INSTANCE = 0x21000000
BLOCK = 0x22000000
STACK = 0x23000000
OBJECT = 0x24000000
RETURN = 0x30000000
SHA256 = 'sha256'


def check(executable, probe, chr_path, anm_path, report):
    source = Path(executable).read_bytes()
    if hashlib.sha256(source).hexdigest() != EXE_SHA256:
        raise ValueError('unsupported original executable hash')
    machine = Original(executable)
    chr_bytes = Path(chr_path).read_bytes()
    machine.load_character(chr_bytes)
    machine.reset_instance()
    table_result = machine.check_trig_tables()
    script = build_script(chr_bytes)+['set-condition 4294967295','status','set-condition 0','status']
    expected = machine.run_script(script)
    native = run_probe(probe, chr_path, script)
    compare(script, native, expected)

    direct = 0
    for opcode in range(0x41):
        for carry in (0, 1):
            for variant in range(3):
                fields = condition_fields(opcode, variant, machine)
                want = machine.condition(opcode, carry, fields)
                got = run_condition(probe, chr_path, opcode, carry, fields)
                if want != got:
                    raise ValueError(f'condition opcode {opcode:#x} carry {carry} '
                                     f'variant {variant}: original {want}, port {got}')
                direct += 1
    random_result = 0
    for heading, accumulator, random_a, random_b in (
            (0, 0, 0, 0), (0x123456, 0x654321, 1, 2), (-1, -1, 3, 7),
            (0x1ffffff, 0x1000000, 4, 5), (-0x4000000, 0x2000000, 7, 1)):
        want = machine.random(heading, accumulator, random_a, random_b)
        got = run_random(probe, chr_path, heading, accumulator, random_a, random_b)
        if want != got:
            raise ValueError(f'random selector mismatch at {heading:#x}/{accumulator:#x}: '
                             f'original {want}, port {got}')
        random_result += 1

    anm = machine.check_anm(probe, anm_path) if anm_path else None
    result = dict(
        source_sha256=EXE_SHA256,
        executable_function_sha256={
            'controller': hashlib.sha256(machine.bytes_at(0x43a0e0, 0x43b2b0 - 0x43a0e0)).hexdigest(),
            'animation_def_bridge': hashlib.sha256(machine.bytes_at(0x42ce40, 0x42d139 - 0x42ce40)).hexdigest(),
        },
        trace_operations=len(script), trace_fields_compared=expected['fields'],
        direct_condition_cases=direct, random_cases=random_result,
        sin_cos_tables=table_result,
        anm_entries=anm['entries'] if anm else None,
        anm_anchors=anm['anchors'] if anm else None,
        chr_path=str(chr_path), anm_path=str(anm_path) if anm_path else None,
        scope=('controller init, action selection, timed and single-tick deltas, pose and root '
               'interpolation, heading servo, frame events, frame-end conditions, loop and end '
               'behavior, status probe, full condition opcode switch, random selector, sine/cosine '
               'constant tables'),
        mocked_functions=[],
        mismatches=0,
    )
    if report:
        Path(report).write_text(json.dumps(result, indent=2) + '\n')
    return result


class Original:
    def __init__(self, path):
        pe = pefile.PE(data=Path(path).read_bytes())
        self.image = pe.get_memory_mapped_image()
        self.machine = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        for base, size in ((IMAGE, 0x1000000), (CHR_BASE, 0x400000), (INSTANCE, 0x10000),
                           (BLOCK, 0x10000), (STACK, 0x10000), (OBJECT, 0x1000), (RETURN, 0x1000)):
            self.machine.mem_map(base, size)
        self.machine.mem_write(IMAGE, self.image)
        self.bones = 0
        self.map_count = 0
        self.anims = {}
        self.observed_carry = 0
        # 0x42ced2 is the instruction after `call 0x43a7a0` in the wrapper; CF
        # there is the frame-end "action changed" result before `and eax,0xffff`.
        def carry_hook(_machine, address, _size, _data):
            self.observed_carry = _machine.reg_read(UC_X86_REG_EFLAGS) & 1
        self.machine.hook_add(unicorn.UC_HOOK_CODE, carry_hook, begin=0x42ced2, end=0x42ced2)

    # ---- memory helpers
    def r8(self, address):
        return self.machine.mem_read(address, 1)[0]
    def r16(self, address):
        return struct.unpack('<H', self.machine.mem_read(address, 2))[0]
    def r32(self, address):
        return struct.unpack('<i', self.machine.mem_read(address, 4))[0]
    def u32(self, address):
        return struct.unpack('<I', self.machine.mem_read(address, 4))[0]
    def w8(self, address, value):
        self.machine.mem_write(address, struct.pack('<B', value & 0xff))
    def w16(self, address, value):
        self.machine.mem_write(address, struct.pack('<H', value & 0xffff))
    def w32(self, address, value):
        self.machine.mem_write(address, struct.pack('<I', value & 0xffffffff))
    def bytes_at(self, address, size):
        return self.machine.mem_read(address, size)

    def call(self, address, registers=None, stack_args=()):
        machine = self.machine
        esp = STACK + 0x8000
        for argument in reversed(tuple(stack_args)):
            esp -= 4
            self.w32(esp, argument)
        esp -= 4
        self.w32(esp, RETURN)
        self.entry_esp = esp
        machine.reg_write(UC_X86_REG_ESP, esp)
        machine.reg_write(UC_X86_REG_EBP, 0)
        for register, value in (registers or {}).items():
            machine.reg_write(register, value)
        machine.emu_start(address, RETURN, count=10000000)
        if machine.reg_read(UC_X86_REG_EIP) != RETURN:
            raise ValueError(f'emulation at {address:#x} did not return')
        return machine

    # ---- character image (relocation 0x439d20)
    def load_character(self, source):
        data = bytearray(source)
        base = CHR_BASE
        def word(offset):
            return struct.unpack_from('<i', source, offset)[0]
        def put(offset, value):
            struct.pack_into('<I', data, offset, value & 0xffffffff)
        self.bones = word(8)
        self.map_count = word(16)
        self.anim_table = word(20) + base
        put(4, word(4) + base)
        put(12, word(12) + base)
        bone = word(4)
        for _ in range(self.bones):
            flags = word(bone + 24)
            stride = 92 if flags & 1 else 60
            for field in (0, 4, 8):
                put(bone + field, word(bone + field) + base)
            track_b = word(bone + 4)
            count = word(bone + 16)
            entry = track_b
            for _ in range(count):
                for field in (0, 4, 8, 44, 48, 52):
                    put(entry + field, word(entry + field) + base)
                struct.pack_into('<H', data, entry + 36, 0x1402)
                entry += 88
            bone += stride
        put(20, self.anim_table)
        for slot in range(255):
            pointer = struct.unpack_from('<I', source, word(20) + slot * 4)[0]
            if not pointer:
                continue
            animation = pointer + base
            struct.pack_into('<I', data, word(20) + slot * 4, animation)
            count = word(pointer)
            frame = pointer + 20
            for _ in range(count):
                stride = word(frame)
                for field in (28, 32, 40):
                    if word(frame + field):
                        put(frame + field, word(frame + field) + base)
                self.anims.setdefault(slot, []).append(frame + base)
                frame += stride
        for field in (28, 36, 40, 52, 68, 72, 76):
            if word(field):
                put(field, word(field) + base)
        self.machine.mem_write(CHR_BASE, bytes(data))
        self.chr = bytes(data)

    def reset_instance(self):
        machine = self.machine
        machine.mem_write(INSTANCE, bytes(0x9c))
        machine.mem_write(BLOCK, bytes(0x10000))
        self.w32(INSTANCE + 0x00, CHR_BASE)
        self.w32(INSTANCE + 0x14, self.map_count)
        self.w32(INSTANCE + 0x58, 0x34)
        pose = BLOCK + self.map_count * 4
        delta = pose + self.map_count * 0x18
        third = delta + self.map_count * 0x18
        roots = third + self.map_count * 0x3c
        for field, value in ((0x7c, BLOCK), (0x80, pose), (0x84, delta), (0x88, third),
                             (0x8c, roots), (0x90, roots + 0x60), (0x94, roots + 0xc0),
                             (0x98, roots + 0x2a0)):
            self.w32(INSTANCE + field, value)
        self.pose = pose
        self.delta = delta
        self.roots = roots
        self.root_delta = roots + 0x60
        # 0x439c70 instance setup: pose from slot 0 frame 0, then select 0/0.
        self.call(0x43a0e0, {UC_X86_REG_ESI: INSTANCE, UC_X86_REG_EAX: 0, UC_X86_REG_EBX: 0})
        self.call(0x43a150, {UC_X86_REG_ESI: INSTANCE, UC_X86_REG_EAX: 0, UC_X86_REG_EBX: 0})
        self.move_value = 0

    def set_object(self, heading, angles):
        self.w32(OBJECT + 0x18, INSTANCE)
        self.w32(OBJECT + 0x24, angles[0])
        self.w32(OBJECT + 0x28, angles[1])
        self.w32(OBJECT + 0x2c, angles[2])
        self.w32(OBJECT + 0x30, heading)
        return OBJECT

    def select(self, slot, frame):
        if not 0 <= slot <= 255:
            raise ValueError('original 0x43a150 only receives table slots below 255')
        before = self.r32(INSTANCE + 0x0c)
        machine = self.machine
        machine.reg_write(UC_X86_REG_ESI, INSTANCE)
        machine.reg_write(UC_X86_REG_EAX, slot)
        machine.reg_write(UC_X86_REG_EBX, frame)
        self.call(0x43a150, {UC_X86_REG_ESI: INSTANCE, UC_X86_REG_EAX: slot, UC_X86_REG_EBX: frame})
        carry = machine.reg_read(UC_X86_REG_EFLAGS) & 1
        return carry == 0 or self.r32(INSTANCE + 0x0c) != before

    def tick(self, fields):
        instance = INSTANCE
        self.w32(instance + 0x60, fields['state_flags'])
        self.w32(instance + 0x48, fields['anim_flags'])
        self.w8(instance + 0x64, fields['stance'])
        self.w8(instance + 0x65, fields['pitch'])
        self.w8(instance + 0x66, fields['switch_flag'])
        self.w8(instance + 0x78, fields['byte78'])
        self.w8(instance + 0x79, fields['byte79'])
        self.w8(instance + 0x7a, fields['aim_a'])
        self.w8(instance + 0x7b, fields['aim_b'])
        self.w32(instance + 0x4c, fields['extra_4c'])
        if fields['write_condition']:
            self.w32(instance + 0x38, fields['condition_state'])
        if fields['write_random']:
            self.w32(instance + 0x28, fields['random_a'])
            self.w32(instance + 0x2c, fields['random_b'])
        # 0x4873f5 call site: [instance+0x34] = [object+0x30] >> 7.
        self.w32(instance + 0x34, self.u32(OBJECT + 0x30) >> 7)
        self.observed_carry = 0
        self.call(0x42ce40, {}, (OBJECT,))
        events = self.machine.reg_read(UC_X86_REG_EAX) & 0xffff
        # 0x42ce40 frame: ebp = entry_esp - 16, move vector at ebp - 0x20.
        move = self.r32(self.entry_esp - 0x30 + 4)
        if move:
            self.move_value = move
        return events

    def state(self):
        machine = self.machine
        animation = self.r32(INSTANCE + 0x0c)
        frame = self.r32(INSTANCE + 0x10)
        pose = []
        for bone in range(self.map_count):
            pose.append([self.r32(self.pose + bone * 0x18 + 4 * part) for part in range(6)])
        delta = []
        for bone in range(self.map_count):
            delta.append([self.r32(self.delta + bone * 0x18 + 4 * part) for part in range(6)])
        roots = [[self.r32(self.roots + bone * 12 + 4 * part) for part in range(3)] for bone in range(8)]
        root_delta = [[self.r32(self.root_delta + bone * 12 + 4 * part) for part in range(3)]
                      for bone in range(8)]
        slot, frame_index = 1000, 100
        if animation:
            for index, (table_slot, frames) in enumerate(sorted(self.anims.items())):
                if frames and self.r32(self.anim_table + table_slot * 4) == animation:
                    slot, frame_index = table_slot, 0
                    for position, address in enumerate(frames):
                        if address == frame:
                            frame_index = position
                            break
                    break
        duration = self.r16(frame + 8) if frame else 0
        event_bits = self.r32(INSTANCE + 0x50)
        return {
            'ok': 1, 'changed': 0,
            'slot': slot, 'frame': frame_index,
            'elapsed': duration - self.r32(INSTANCE + 0x18) if frame else 0,
            'ticks': self.u32(INSTANCE + 0x18),
            'frame_mask': self.u32(frame+4) if frame else 0,
            'heading': self.r32(0x4399d0), 'accum': self.u32(INSTANCE + 0x34),
            'cond': self.u32(INSTANCE + 0x38), 'latch': self.u32(INSTANCE + 0x40),
            'event_bits': event_bits, 'last_events': self.r32(0x4399a8) & 0xffff,
            'correction': self.r32(0x4399c4), 'move': self.move_value,
            'ra': self.u32(INSTANCE + 0x28), 'rb': self.u32(INSTANCE + 0x2c),
            'motion': [self.r32(INSTANCE + 0x1c + 4 * part) for part in range(3)],
            'pose': pose, 'delta': delta, 'root': roots, 'root_delta': root_delta,
        }

    def status(self):
        animation = self.r32(INSTANCE + 0x0c)
        frame = self.r32(INSTANCE + 0x10)
        if not animation or not self.u32(INSTANCE + 0x18):
            return {'slot': 0, 'frame': 0, 'elapsed': 0}
        slot = 1000
        for table_slot in sorted(self.anims):
            if self.anims[table_slot] and self.r32(self.anim_table + table_slot * 4) == animation:
                slot = table_slot
                break
        frame_index = 0
        walk = animation + 20
        while walk != frame and frame_index < 100:
            frame_index += 1
            walk += self.u32(walk)
        return {'slot': slot, 'frame': frame_index,
                'elapsed': self.r16(frame + 8) - self.r32(INSTANCE + 0x18)}

    # ---- direct helpers
    def condition(self, opcode, carry, fields):
        machine = self.machine
        self.w32(INSTANCE + 0x60, fields['state_flags'])
        self.w32(INSTANCE + 0x48, fields['anim_flags'])
        self.w8(INSTANCE + 0x64, fields['stance'])
        self.w8(INSTANCE + 0x65, fields['pitch'])
        self.w8(INSTANCE + 0x66, fields['switch_flag'])
        self.w8(INSTANCE + 0x78, fields['byte78'])
        self.w8(INSTANCE + 0x79, fields['byte79'])
        self.w8(INSTANCE + 0x7a, fields['aim_a'])
        self.w8(INSTANCE + 0x7b, fields['aim_b'])
        self.w32(INSTANCE + 0x4c, fields['extra_4c'])
        self.w32(INSTANCE + 0x20, fields['motion_y'])
        self.w32(0x4399d0, fields['heading'])
        self.w32(INSTANCE + 0x34, fields['accumulator'])
        machine.reg_write(UC_X86_REG_EFLAGS, carry & 1)
        self.call(0x43ac40, {UC_X86_REG_ESI: INSTANCE, UC_X86_REG_EAX: opcode,
                             UC_X86_REG_EBX: fields['heading'], UC_X86_REG_ECX: 0})
        return (machine.reg_read(UC_X86_REG_EFLAGS) & 1) == 0

    def random(self, heading, accumulator, random_a, random_b):
        machine = self.machine
        self.w32(INSTANCE + 0x34, accumulator)
        self.w32(INSTANCE + 0x28, random_a)
        self.w32(INSTANCE + 0x2c, random_b)
        self.call(0x43ac10, {UC_X86_REG_ESI: INSTANCE, UC_X86_REG_EAX: heading})
        return machine.reg_read(UC_X86_REG_EAX) & 0xff

    def check_trig_tables(self):
        import math
        expected_sin = [max(-2147483647, min(2147483647,
            int(math.sin(i * 2 * math.pi / 512) * 2 ** 31))) for i in range(512)]
        expected_cos = [max(-2147483647, min(2147483647,
            int(math.cos(i * 2 * math.pi / 512) * 2 ** 31))) for i in range(512)]
        actual_sin = [self.r32(0x42f1d0 + i * 4) for i in range(512)]
        actual_cos = [self.r32(0x42f3d0 + i * 4) for i in range(512)]
        if actual_sin != expected_sin or actual_cos != expected_cos:
            raise ValueError('sine/cosine constant tables differ from the executable')
        return dict(sin_sha256=hashlib.sha256(self.bytes_at(0x42f1d0, 2048)).hexdigest(),
                    cos_sha256=hashlib.sha256(self.bytes_at(0x42f3d0, 2048)).hexdigest(),
                    entries=512, matches_port_formula=True)

    def run_script(self, script):
        results = []
        fields = 0
        for index, operation in enumerate(script):
            try:
                results.append(self.run_operation(operation))
            except Exception as error:
                raise ValueError(f'original emulation failed at operation {index} '
                                 f'({operation}): {error}') from error
            fields += len(results[-1])
        return {'results': results, 'fields': fields}

    def run_operation(self, operation):
        parts = operation.split()
        result = None
        if parts[0] == 'init':
                self.reset_instance()
                result = self.state()
        elif parts[0] == 'select':
                ok = self.select(int(parts[1]), int(parts[2]))
                state = self.state()
                state['ok'] = 1 if ok else 0
                result = state
        elif parts[0] == 'tick':
                values = [int(value, 0) for value in parts[1:]]
                payload = dict(zip(
                    ('heading', 'a24', 'a28', 'a2c', 'state_flags', 'anim_flags', 'stance', 'pitch',
                     'aim_a', 'aim_b', 'byte78', 'byte79', 'switch_flag', 'extra_4c',
                     'write_condition', 'condition_state', 'write_random', 'random_a', 'random_b',
                     'carry'), values))
                self.set_object(payload['heading'], (payload['a24'], payload['a28'], payload['a2c']))
                events = self.tick(payload)
                state = self.state()
                state['events'] = events
                state['changed'] = self.observed_carry
                result = state
        elif parts[0]=='set-condition':
                self.w32(INSTANCE+0x38,int(parts[1],0))
                result=self.state()
        elif parts[0] == 'status':
                state = self.state()
                state.update(self.status())
                result = state
        else:
                raise ValueError(f'unknown script operation {parts[0]}')
        return result

    def check_anm(self, probe, anm_path):
        text = Path(anm_path).read_bytes().decode('latin1')
        expected = []
        for raw in text.split('\r\n'):
            line = raw.strip()
            if not line or line.startswith('//'):
                continue
            tokens = line.split()
            if not tokens or tokens[0].startswith('/') or len(tokens) < 2:
                continue
            name = tokens[0]
            number = int(tokens[1])
            velocity = 0.0
            if len(tokens) >= 3 and not tokens[2].startswith('/'):
                velocity = float(tokens[2])
            override = len(tokens) >= 4 and not tokens[3].startswith('/') and tokens[3] == 'override'
            expected.append(dict(name=name, number=number, velocity=velocity,
                                 velocity_per_tick=int(velocity * 65536) // 30,
                                 override=1 if override else 0))
        output = subprocess.check_output([str(Path(probe).resolve()), '--anm-json',
                                          str(Path(anm_path).resolve())])
        actual = json.loads(output.decode())
        if len(actual) != len(expected):
            raise ValueError(f'ANM entry count differs: original {len(expected)}, port {len(actual)}')
        for index, (want, got) in enumerate(zip(expected, actual)):
            if (want['name'] != got['name'] or want['number'] != got['number']
                    or abs(want['velocity'] - got['velocity']) > 1e-9
                    or want['override'] != got['override']):
                raise ValueError(f'ANM entry {index} differs: {want} vs {got}')
        anchors = {name: next((entry for entry in actual if entry['name'] == name), None)
                   for name in ('Standing_1HW', 'Walking0_1HW', 'Prone_1HW', 'F1', 'Death_Fire')}
        return dict(entries=len(actual), anchors=anchors)


def condition_fields(opcode, variant, machine):
    patterns = {
        'state_flags': (0x00, 0xff, 0x55),
        'anim_flags': (0x00, 0x78, 0x28),
        'stance': (0, 7, 3),
        'pitch': (0x00, 0xf0, 0x1f),
        'byte78': (0, 5, 200),
        'byte79': (0, 1, 0x81),
        'aim_a': (0, 0, 3),
        'aim_b': (0, 0, 9),
        'switch_flag': (0, 1, 0),
        'extra_4c': (0, -0x2000, 0x2000),
        'motion_y': (0, -0x2000, 1000),
        'heading': (0, 0x123456, -0x200000),
        'accumulator': (0, 0x2468, -0x13579),
    }
    fields = {}
    for key, values in patterns.items():
        fields[key] = values[variant % len(values)]
    return fields


def build_script(chr_bytes):
    action = []
    action.append('init')
    action.append('status')
    action.append('select 2 0')  # Standing_1HW: two one-tick frames.
    for _ in range(4):
        action.append(tick_line(0, 0, 0, 0))
    action.append('select 79 0')  # Walking0_1HW: duration 5 first frame.
    action.append(tick_line(0x80, 0, 0, 0))
    action.append(tick_line(0x123400, 0, 0, 0))
    action.append(tick_line(-0x200000, 0, 0, 0))
    action.append(tick_line(0x80, 1, 0x3, 7))
    action.append(tick_line(0x80, 1, 0x11223344, 0x55667788))
    for _ in range(6):
        action.append(tick_line(0x80, 0, 0, 0))
    action.append('select 123 2')  # Running0_1HW frame 2.
    for _ in range(20):
        action.append(tick_line(0, 0, 0, 0))
    action.append('select 3 0')  # Standing_2HW: end condition selects slot 255.
    for _ in range(5):
        action.append(tick_line(0, 0, 0, 0))
    action.append('select 20 0')  # Death_B_P01DL: loops on its last frame.
    for _ in range(112):
        action.append(tick_line(0, 0, 0, 0))
    action.append('select 176 0')  # Standing_SLW.
    for _ in range(4):
        action.append(tick_line(0, 0, 0, 0))
    action.append('select 255 0')  # Empty slot: original leaves the instance untouched.
    action.append('status')
    action.append('init')
    action.append('select 158 1')  # Standing_SCW with a nonzero root index.
    for _ in range(4):
        action.append(tick_line(0x4000, 0, 0, 0))
    events = find_event_slot(chr_bytes)
    if events is not None:
        slot, frame = events
        action.append(f'select {slot} {frame}')
        for _ in range(6):
            action.append(tick_line(0, 0, 0, 0))
    return action


def tick_line(heading, write_condition, condition_state, random_a, random_b=0,
              state_flags=0, anim_flags=0, stance=0, pitch=0, aim_a=0, aim_b=0, byte78=0,
              byte79=0, switch_flag=0, extra_4c=0, write_random=1, carry=0, angles=(0, 0, 0)):
    # Field order matches tools/animation_probe --trace-json.
    return ('tick %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d'
            % (heading, angles[0], angles[1], angles[2], state_flags, anim_flags, stance, pitch,
               aim_a, aim_b, byte78, byte79, switch_flag, extra_4c, write_condition, condition_state,
               write_random, random_a, random_b, carry))


def find_event_slot(chr_bytes):
    """Locate one frame with a gated event byte (flags 0x11 in PLAYER01.CHR)."""
    data = chr_bytes
    bone = struct.unpack_from('<I', data, 4)[0]
    animation_table = struct.unpack_from('<I', data, 20)[0]
    for slot in range(255):
        pointer = struct.unpack_from('<I', data, animation_table + slot * 4)[0]
        if not pointer:
            continue
        count, mapped = struct.unpack_from('<2I', data, pointer)
        frame = pointer + 20
        for index in range(count):
            flags = struct.unpack_from('<I', data, frame + 4)[0]
            if flags in (0x10, 0x11, 0x20, 0x21, 0x30, 0x31) and data[frame + 0x0a]:
                return slot, index
            frame += struct.unpack_from('<I', data, frame)[0]
    return None


def run_probe(probe, chr_path, script):
    output = subprocess.check_output([str(Path(probe).resolve()), '--trace-json',
                                      str(Path(chr_path).resolve())],
                                     input=('\n'.join(script) + '\n').encode())
    return [json.loads(line) for line in output.decode().splitlines()]


def run_condition(probe, chr_path, opcode, carry, fields):
    command = ('condition %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d'
               % (opcode, carry, fields['heading'], fields['accumulator'], fields['motion_y'],
                  fields['extra_4c'], fields['state_flags'], fields['anim_flags'], fields['stance'],
                  fields['pitch'], fields['byte78'], fields['byte79'], fields['aim_a'],
                  fields['aim_b'], fields['switch_flag']))
    output = subprocess.check_output([str(Path(probe).resolve()), '--trace-json',
                                      str(Path(chr_path).resolve())],
                                     input=(command + '\n').encode())
    return json.loads(output.decode().splitlines()[0])['condition'] == 1


def run_random(probe, chr_path, heading, accumulator, random_a, random_b):
    command = 'random %d %d %d %d' % (heading, accumulator, random_a, random_b)
    output = subprocess.check_output([str(Path(probe).resolve()), '--trace-json',
                                      str(Path(chr_path).resolve())],
                                     input=(command + '\n').encode())
    return json.loads(output.decode().splitlines()[0])['random']


def compare(script, native, expected):
    if len(native) != len(expected['results']):
        raise ValueError('trace length differs')
    for index, (operation, want, got) in enumerate(zip(script, expected['results'], native)):
        for key, value in want.items():
            if key not in got:
                raise ValueError(f'operation {index} ({operation}) missing field {key}')
            if got[key] != value:
                raise ValueError(f'operation {index} ({operation}) field {key}: '
                                 f'original {value}, port {got[key]}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable')
    parser.add_argument('--probe', required=True)
    parser.add_argument('--chr', required=True)
    parser.add_argument('--anm', required=True)
    parser.add_argument('--report', type=Path)
    arguments = parser.parse_args()
    outcome = check(arguments.executable, arguments.probe, arguments.chr, arguments.anm,
                    arguments.report)
    print(json.dumps(outcome, indent=2))
