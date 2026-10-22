#!/usr/bin/env python3
"""Check the rebuilt input stage against the original DF1 x86 input pipeline.

Replays 0x41ce20 profile bindings, the 0x47a300 polled command list, the
0x4795a0 held keys, 0x479630 mouse axes and buttons, 0x479380 key events,
0x47a150 mouse pulses, the 0x4790a0 menu/chat filter and the 0x479750+0x479450
frame under Unicorn, then compares every result with the C++ port through
tools/input_state_probe. Original game files are only read."""

import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess
import sys

import pefile
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP

SOURCE_HASH = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
IMAGE = 0x400000
IMAGE_SIZE = 0xa00000
STACK = 0x2000000
STACK_TOP = 0x2008000
RETURN = 0x3fffff0
SCRATCH = 0x3000000
QUEUE = 0xbd4850
QUEUE_COUNT = 0xbd484c
LIST = 0xbd2870
LIST_COUNT = 0xbd4848
KEY_STATE = 0xd093b0
MOUSE_BUTTONS = 0xd09348
JOY_BUTTONS = 0xd09944
AXIS_X = 0xd0934c
AXIS_Y = 0xd09350
SHIFT_STATE = 0xd093c0
CTRL_STATE = 0xd093c1
RUN_FLAG = 0xbd482c
INPUT_MODE = 0xbd4f78
FOCUS_FLAG = 0xc23a10
TYPE_KEY_A = 0xbd4820
TYPE_KEY_B = 0xbd4824
MOUSE_FLIP = 0x67f4fc
MOUSE_SCALE = 0x67f500
TYPE_FLAG = 0x67db6c
DIGIT_FLAG = 0x67db90
PAGE_FLAG = 0x67db7c
MENU_FLAG = 0x67dbc0
HUD_MENU = 0x67f520
HUD_MENU2 = 0x67f53c
ENTER_FLAG = 0x67dbbc
ACTOR_PTR = 0xc23820
PROFILE_PTR = 0x4cf2e4
WINDOWED = 0x503130
WINDOW_ACTIVE = 0xcc0884
TABLE = 0x4fb2fc
TABLE_ROWS = 121
STRIDE = 0x2c
SET_BINDING = 0x41cd70
APPLY_BINDINGS = 0x41ce20
INPUT_INIT = 0x47a300
HELD_KEYS = 0x4795a0
MOUSE_FN = 0x479630
EVENTS = 0x479380
PULSE = 0x47a150
FILTER = 0x4790a0
FRAME_PUSH = 0x479750
FRAME_CONSUME = 0x479450
DISPATCH = 0x477d40
DIGIT_HANDLER = 0x4717e0
FLIGHT_PAGE = 0x42ad50
MENU_PAGE = 0x429700
STRING_GET = 0x404cf0
RESET_MOUSE = 0x479e90
READ_AXES = 0x497d20
STEADY = 0x484740
KEY_PRESS = 0x498ae0
RING_RESET = 0x498ba0
EVENT_POP = 0x47a000
EVENT_DUE = 0x47a070
EFFECTS = {
    'none': 0, 'dispatch_exit': 1, 'clear_typing': 2, 'digit': 3,
    'flight_page': 4, 'menu_page': 5, 'enter_flag': 6, 'dispatch_space': 7,
}


class Original:
    def __init__(self, source):
        self.pe = pefile.PE(data=source)
        self.u = Uc(UC_ARCH_X86, UC_MODE_32)
        self.u.mem_map(IMAGE, IMAGE_SIZE)
        self.u.mem_write(IMAGE, self.pe.get_memory_mapped_image())
        self.u.mem_map(STACK, 0x10000)
        self.u.mem_map(SCRATCH, 0x100000)
        self.u.mem_map(RETURN & ~0xfff, 0x1000)
        self.calls = []
        self.steady_value = 0
        self.dispatch_log = []
        self.name_index = {}
        image = self.pe.get_memory_mapped_image()
        for i in range(TABLE_ROWS):
            at = TABLE - IMAGE + i * STRIDE + 4
            name = image[at:at + 0x22].split(b'\0')[0].decode('latin1')
            self.name_index[name.lower()] = i
        self.shipped_table = self._read(TABLE, TABLE_ROWS * STRIDE)
        self._install_stubs()
        self._zero_state()

    # ---- plumbing ----
    def _return_from(self, address, value):
        def hook(uc, addr, size, user):
            esp = uc.reg_read(UC_X86_REG_ESP)
            ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
            uc.reg_write(UC_X86_REG_ESP, esp + 4)
            uc.reg_write(UC_X86_REG_EAX, value() if callable(value) else value)
            uc.reg_write(UC_X86_REG_EIP, ret)
        self.u.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)

    def _log_from(self, address, name, args=0):
        def hook(uc, addr, size, user):
            esp = uc.reg_read(UC_X86_REG_ESP)
            values = list(struct.unpack('<%dI' % args, uc.mem_read(esp + 4, 4 * args))) if args else []
            ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
            self.calls.append((name, values))
            uc.reg_write(UC_X86_REG_ESP, esp + 4)
            uc.reg_write(UC_X86_REG_EAX, 0)
            uc.reg_write(UC_X86_REG_EIP, ret)
        self.u.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)

    def _call(self, entry, args):
        esp = STACK_TOP
        for arg in reversed(args):
            esp -= 4
            self.u.mem_write(esp, struct.pack('<I', arg))
        esp -= 4
        self.u.mem_write(esp, struct.pack('<I', RETURN))
        self.u.reg_write(UC_X86_REG_ESP, esp)
        self.u.emu_start(entry, RETURN, count=100000000)
        if self.u.reg_read(UC_X86_REG_EIP) != RETURN:
            raise RuntimeError('Original code did not return: %x' % self.u.reg_read(UC_X86_REG_EIP))
        return self.u.reg_read(UC_X86_REG_EAX)

    def _read(self, address, size):
        return bytes(self.u.mem_read(address, size))

    def _write_u32(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def _read_u32(self, address):
        return struct.unpack('<I', self.u.mem_read(address, 4))[0]

    def _install_stubs(self):
        self._return_from(STRING_GET, SCRATCH + 0xf000)
        self._return_from(RESET_MOUSE, 0)
        self._return_from(READ_AXES, 0)
        self._return_from(STEADY, lambda: self.steady_value)
        self._log_from(DIGIT_HANDLER, 'digit', 1)
        self._log_from(FLIGHT_PAGE, 'flight_page', 1)
        self._log_from(MENU_PAGE, 'menu_page', 1)

        def dispatch_hook(uc, addr, size, user):
            esp = uc.reg_read(UC_X86_REG_ESP)
            values = list(struct.unpack('<3I', uc.mem_read(esp + 4, 12)))
            ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
            self.dispatch_log.append((values[0], values[1], values[2]))
            uc.reg_write(UC_X86_REG_ESP, esp + 4)
            uc.reg_write(UC_X86_REG_EAX, 0)
            uc.reg_write(UC_X86_REG_EIP, ret)
        self.u.hook_add(UC_HOOK_CODE, dispatch_hook, begin=DISPATCH, end=DISPATCH)

    def _zero_state(self):
        self.u.mem_write(KEY_STATE, b'\0' * 0x100)
        self._write_u32(MOUSE_BUTTONS, 0)
        self._write_u32(JOY_BUTTONS, 0)
        self._write_u32(AXIS_X, 0)
        self._write_u32(AXIS_Y, 0)
        self._write_u32(QUEUE_COUNT, 0)
        self.u.mem_write(QUEUE, b'\0' * (32 * 16))
        self._call(RING_RESET, [])
        self._write_u32(RUN_FLAG, 0)
        self._write_u32(INPUT_MODE, 0)
        self._write_u32(FOCUS_FLAG, 0)
        self._write_u32(TYPE_FLAG, 0)
        self._write_u32(DIGIT_FLAG, 0)
        self._write_u32(PAGE_FLAG, 0)
        self._write_u32(MENU_FLAG, 0)
        self._write_u32(HUD_MENU, 0)
        self._write_u32(HUD_MENU2, 0)
        self._write_u32(ENTER_FLAG, 0)
        self._write_u32(WINDOWED, 0)
        self._write_u32(WINDOW_ACTIVE, 0)
        self.calls = []
        self.dispatch_log = []

    # ---- queue helpers ----
    def drain(self):
        records = []
        while self._read_u32(QUEUE_COUNT) > 0:
            outs = SCRATCH + 0x20000
            self._call(EVENT_POP, [outs, outs + 4, outs + 8, outs + 0xc])
            records.append(tuple(struct.unpack('<4I', self._read(outs, 16))))
        return records

    # ---- checks ----
    def table_rows(self):
        rows = []
        raw = self._read(TABLE, TABLE_ROWS * STRIDE)
        for i in range(TABLE_ROWS):
            row = raw[i * STRIDE:(i + 1) * STRIDE]
            identifier = struct.unpack_from('<I', row, 0)[0]
            key, mouse, joystick, flags = struct.unpack_from('<4H', row, 0x24)
            rows.append((identifier, key, mouse, joystick, flags))
        return rows

    def set_binding(self, index, key, mouse):
        row = SCRATCH + 0x30000
        self._write_u32(row, key)
        self._write_u32(row + 4, mouse)
        self._write_u32(row + 8, 0)
        if 0 <= index < TABLE_ROWS:
            self._call(SET_BINDING, [index, row])
        entry = self._read(TABLE + index * STRIDE, STRIDE)
        self.restore_table()
        return struct.unpack_from('<2H', entry, 0x24)

    def restore_table(self):
        self.u.mem_write(TABLE, self.shipped_table)

    def apply_bindings(self, bindings):
        base = SCRATCH + 0x40000
        for i, (key, mouse, joy) in enumerate(bindings):
            self._write_u32(base + 0x6a0 + i * 12, key)
            self._write_u32(base + 0x6a0 + i * 12 + 4, mouse)
            self._write_u32(base + 0x6a0 + i * 12 + 8, joy)
        self._write_u32(PROFILE_PTR, base)
        self._call(APPLY_BINDINGS, [])
        out = []
        for action, name in PROFILE_PAIRS:
            found = self.find_row(name)
            if found < 0:
                out.append((-1, 0, 0))
                continue
            entry = self._read(TABLE + found * STRIDE, STRIDE)
            key, mouse = struct.unpack_from('<2H', entry, 0x24)
            out.append((found, key, mouse))
        self.restore_table()
        return out

    def find_row(self, name):
        return self.name_index.get(name.lower(), -1)

    def active(self):
        self.restore_table()
        self._write_u32(WINDOWED, 1)
        self._call(INPUT_INIT, [])
        self._write_u32(WINDOWED, 0)
        count = self._read_u32(LIST_COUNT)
        raw = self._read(LIST, count * 4)
        return list(struct.unpack('<%dI' % count, raw))

    def held(self, tick, priority, keys_down):
        self._zero_state()
        self.restore_table()
        self._write_u32(0x4fcbde, (priority & 0xffff) << 16)
        for key in keys_down:
            self.u.mem_write(KEY_STATE + (key & 0xff), b'\x01')
        self._call(HELD_KEYS, [tick])
        focus = self._read_u32(FOCUS_FLAG)
        return focus, self.drain()

    def mouse(self, x, y, scale, flip, steady, buttons, tick):
        self._zero_state()
        self.restore_table()
        self._write_u32(WINDOWED, 0)
        self._write_u32(WINDOW_ACTIVE, 1)
        self._write_u32(AXIS_X, x)
        self._write_u32(AXIS_Y, y)
        self._write_u32(MOUSE_FLIP, flip)
        self._write_u32(MOUSE_SCALE, scale)
        self._write_u32(MOUSE_BUTTONS, buttons)
        self.steady_value = steady
        self._call(MOUSE_FN, [tick])
        return self.drain()

    def event(self, key, state, flag, modifiers, tick):
        self._zero_state()
        self.restore_table()
        self.u.mem_write(SHIFT_STATE, b'\x01' if (modifiers & 1) else b'\x00')
        self.u.mem_write(CTRL_STATE, b'\x01' if (modifiers & 4) else b'\x00')
        lparam = 0x01000000 if flag else 0
        self._call(KEY_PRESS, [key, lparam])
        self.u.mem_write(SHIFT_STATE, b'\x00')
        self.u.mem_write(CTRL_STATE, b'\x00')
        self._call(EVENTS, [tick])
        return self.drain()

    def pulse(self, event, menu_flag, actor_flags):
        self._zero_state()
        self.restore_table()
        self._write_u32(MENU_FLAG, menu_flag)
        actor = SCRATCH + 0x50000
        self._write_u32(ACTOR_PTR, actor)
        self._write_u32(actor + 0x6c, actor_flags)
        self._call(PULSE, [event])
        return self.drain()

    def filter_key(self, key, flags):
        self._zero_state()
        self.restore_table()
        type_flag, digit_flag, page_flag, menu_flag, hud_menu, hud_menu2, key_a, key_b, actor_flags = flags
        self._write_u32(TYPE_FLAG, type_flag)
        self._write_u32(DIGIT_FLAG, digit_flag)
        self._write_u32(PAGE_FLAG, page_flag)
        self._write_u32(MENU_FLAG, menu_flag)
        self._write_u32(HUD_MENU, hud_menu)
        self._write_u32(HUD_MENU2, hud_menu2)
        self._write_u32(TYPE_KEY_A, key_a)
        self._write_u32(TYPE_KEY_B, key_b)
        actor = SCRATCH + 0x50000
        self._write_u32(ACTOR_PTR, actor)
        self._write_u32(actor + 0x6c, actor_flags)
        handled = self._call(FILTER, [key])
        type_cleared = self._read_u32(TYPE_FLAG) == 0
        digit_cleared = self._read_u32(DIGIT_FLAG) == 0
        enter_set = self._read_u32(ENTER_FLAG) != 0
        records = self.drain()
        effect = EFFECTS['none']
        argument = 0
        pushed = -1
        dispatched = -1
        if self.dispatch_log:
            dispatched = self.dispatch_log[0][0]
            effect = EFFECTS['dispatch_exit']
        elif any(name == 'digit' for name, _ in self.calls):
            effect = EFFECTS['digit']
            argument = next(v[0] for name, v in self.calls if name == 'digit')
        elif any(name == 'flight_page' for name, _ in self.calls):
            effect = EFFECTS['flight_page']
            argument = next(v[0] for name, v in self.calls if name == 'flight_page')
        elif any(name == 'menu_page' for name, _ in self.calls):
            effect = EFFECTS['menu_page']
            argument = next(v[0] for name, v in self.calls if name == 'menu_page')
        elif records:
            effect = EFFECTS['dispatch_space']
            pushed = records[0][0]
        elif type_flag and type_cleared:
            effect = EFFECTS['clear_typing']
        elif enter_set:
            effect = EFFECTS['enter_flag']
        return handled, effect, argument, pushed, dispatched

    def frame(self, case):
        self._zero_state()
        self.restore_table()
        tick, mode, live, mouse_enabled, priority = case['head'][:5]
        mouse_buttons, x, y, scale, flip, steady = case['head'][5:]
        self._write_u32(INPUT_MODE, mode)
        self._write_u32(RUN_FLAG, 0 if live else 1)
        self._write_u32(WINDOWED, 0 if mouse_enabled else 1)
        self._write_u32(WINDOW_ACTIVE, 1)
        self._write_u32(MOUSE_BUTTONS, mouse_buttons)
        self._write_u32(AXIS_X, x)
        self._write_u32(AXIS_Y, y)
        self._write_u32(MOUSE_FLIP, flip)
        self._write_u32(MOUSE_SCALE, scale)
        self.steady_value = steady
        flags = case['flags']
        self._write_u32(TYPE_FLAG, flags[0])
        self._write_u32(DIGIT_FLAG, flags[1])
        self._write_u32(PAGE_FLAG, flags[2])
        self._write_u32(MENU_FLAG, flags[3])
        self._write_u32(HUD_MENU, flags[4])
        self._write_u32(HUD_MENU2, flags[5])
        self._write_u32(TYPE_KEY_A, flags[6])
        self._write_u32(TYPE_KEY_B, flags[7])
        actor = SCRATCH + 0x50000
        self._write_u32(ACTOR_PTR, actor)
        self._write_u32(actor + 0x6c, flags[8])
        for key in case['keys']:
            self.u.mem_write(KEY_STATE + (key & 0xff), b'\x01')
        for event in case['events']:
            key, state, flag, modifiers = event
            # 0xd093c0 and 0xd093c1 are the key-state bytes for shift and control,
            # so the event snapshot keeps whatever the platform state is.
            self.u.mem_write(SHIFT_STATE, b'\x01' if (modifiers & 1) else b'\x00')
            self.u.mem_write(CTRL_STATE, b'\x01' if (modifiers & 4) else b'\x00')
            lparam = 0x01000000 if flag else 0
            self._call(KEY_PRESS, [key, lparam])
        for event, menu, actor_bits in case['pulses']:
            self._write_u32(MENU_FLAG, menu)
            self._write_u32(actor + 0x6c, actor_bits)
            self._call(PULSE, [event])
        self._write_u32(MENU_FLAG, flags[3])
        self._write_u32(actor + 0x6c, flags[8])
        self._call(FRAME_PUSH, [tick])
        self.calls = []
        self._call(FRAME_CONSUME, [tick])
        return self.dispatch_log


def row_equals_name(pe, index, name):
    image = pe.get_memory_mapped_image()
    at = TABLE - IMAGE + index * STRIDE + 4
    raw = image[at:at + 0x22].split(b'\0')[0].decode('latin1')
    return raw.lower() == name.lower()


PROFILE_PAIRS = [
    (0, 'slide_left'), (1, 'slide_right'), (2, 'move_forward'), (3, 'move_back'),
    (4, 'Prone'), (5, 'Crouch'), (6, 'Stand'), (7, 'move_jump'),
    (8, 'attack_1'), (8, 'attack_2'), (9, 'scope'), (10, 'NoRun'), (11, 'cycle'), (12, 'Binoculars'),
    (13, 'weapon1'), (14, 'weapon2'), (15, 'weapon3'), (16, 'weapon4'), (17, 'weapon5'), (18, 'weapon6'),
    (19, 'weapon7'), (20, 'weapon8'), (21, 'weapon9'), (22, 'weapon0'),
    (23, 'zoomin'), (24, 'zoomout'), (25, 'talk'), (26, 'stalk'), (27, 'NextWaypoint'),
    (28, 'AudioMacros'), (29, 'resolution'), (30, 'Drop'), (31, 'magazine'),
    (32, 'look_up'), (33, 'look_down'), (34, 'turn_right'), (35, 'turn_left'),
    (36, 'tab'), (37, 'briefing'), (38, 'OldMessages'), (39, 'ShowFriendly'),
    (40, 'MissionGoals'), (41, 'BigMap'), (42, 'Turbo'), (43, 'NextFlag'),
    (44, 'PrevWaypoint'), (45, 'server'),
]


def run_probe(probe, mode, lines):
    result = subprocess.run([str(probe), mode], input='\n'.join(lines) + '\n',
                            capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError('probe %s failed: %s' % (mode, result.stderr.strip()))
    return result.stdout.strip().split('\n') if result.stdout.strip() else []


def format_records(records):
    return ';'.join('%d:%d:%d:%d' % record for record in records)


def expect(condition, message, failures):
    if not condition:
        failures.append(message)


def random_keys(rng):
    pool = [0x10, 0x11, 0x12, 0x20, 0x22, 0x26, 0x28, 0x31, 0x32, 0x53, 0x50, 0x55, 0x77, 0x78, 0x1B, 0x0D]
    return [key for key in pool if rng.random() < 0.5]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--cases', type=int, default=40)
    parser.add_argument('--seed', type=int, default=1998)
    args = parser.parse_args()

    source = (args.game / 'Df.exe').read_bytes()
    if hashlib.sha256(source).hexdigest() != SOURCE_HASH:
        raise SystemExit('Unsupported original executable hash')
    original = Original(source)
    failures = []
    details = {}
    rng = random.Random(args.seed)

    # 1. Shipped table and set_binding.
    table_lines = run_probe(args.probe, 'table', [])
    port_rows = [tuple(int(part) for part in line.split(':')) for line in table_lines]
    expect(port_rows == original.table_rows(), 'shipped command table mismatch', failures)
    details['table_rows'] = len(port_rows)

    set_cases = [(3, 200, 0), (3, 0, 3), (3, 40, 2), (0, 0, 1), (10, 65, 0), (120, 300, 9)]
    set_cases += [(rng.randrange(TABLE_ROWS), rng.randrange(600), rng.randrange(5)) for _ in range(args.cases)]
    lines = ['%d %d %d' % case for case in set_cases]
    outputs = run_probe(args.probe, 'set', lines)
    for case, line in zip(set_cases, outputs):
        key, mouse = (int(v) for v in line.split())
        original_key, original_mouse = original.set_binding(*case)
        expect((key & 0xffff, mouse) == (original_key, original_mouse),
               'set_binding %r port=%r original=%r' % (case, (key, mouse), (original_key, original_mouse)), failures)
    details['set_bindings'] = len(set_cases)

    # 2. Profile bindings.
    bind_cases = []
    bind_cases.append([(37, 0, 0), (39, 0, 0), (38, 0, 0), (40, 0, 0), (34, 3, 0), (35, 0, 0), (46, 0, 0), (32, 0, 0),
                       (17, 1, 0), (83, 2, 0), (16, 0, 0), (8, 0, 0), (66, 0, 0), (49, 0, 0), (50, 0, 0), (51, 0, 0),
                       (52, 0, 0), (53, 0, 0), (54, 0, 0), (55, 0, 0), (56, 0, 0), (57, 0, 0), (48, 0, 0), (187, 0, 0),
                       (189, 0, 0), (84, 0, 0), (89, 0, 0), (87, 0, 0), (65, 0, 0), (86, 0, 0), (68, 0, 0), (77, 0, 0),
                       (104, 0, 0), (98, 0, 0), (102, 0, 0), (100, 0, 0), (9, 0, 0), (79, 0, 0), (82, 0, 0), (70, 0, 0),
                       (71, 0, 0), (67, 0, 0), (72, 0, 0), (81, 0, 0), (69, 0, 0), (36, 0, 0)])
    for _ in range(args.cases):
        case = []
        for _ in range(46):
            style = rng.randrange(4)
            if style == 0:
                case.append((0, 0, 0))
            elif style == 1:
                case.append((rng.randrange(256), 0, 0))
            elif style == 2:
                case.append((0, rng.randrange(4), 0))
            else:
                case.append((rng.randrange(0x10000), rng.randrange(4), rng.randrange(4)))
        bind_cases.append(case)
    lines = [' '.join('%d %d %d' % row for row in case) for case in bind_cases]
    outputs = run_probe(args.probe, 'bind', lines)
    for number, (case, line) in enumerate(zip(bind_cases, outputs)):
        port = [tuple(int(v) for v in entry.split(':')) for entry in line.split()]
        expected = original.apply_bindings(case)
        expect(port == expected, 'bind case %d mismatch port=%r original=%r' % (number, port[:3], expected[:3]), failures)
    details['bind_cases'] = len(bind_cases)

    # 3. Active list.
    port_active = [int(v) for v in run_probe(args.probe, 'active', [])[0].split()]
    expect(port_active == original.active(), 'active list mismatch', failures)
    details['active_commands'] = len(port_active)

    # 4. Held keys.
    held_cases = []
    for _ in range(args.cases):
        held_cases.append((rng.randrange(1, 5000), rng.choice([0, 0, 0x11]), random_keys(rng)))
    held_cases += [(100, 0, [0x26, 0x11]), (101, 0, [0x26]), (102, 0, [0x12])]
    lines = ['%d %d %d %s' % (tick, priority, len(keys), ' '.join(str(k) for k in keys)) for tick, priority, keys in held_cases]
    outputs = run_probe(args.probe, 'held', lines)
    for number, ((tick, priority, keys), line) in enumerate(zip(held_cases, outputs)):
        focus_text, records_text = line.split('|', 1)
        port = [tuple(int(v) for v in record.split(':')) for record in records_text.split(';') if record]
        expected_focus, expected = original.held(tick, priority, keys)
        expect(int(focus_text) == (1 if expected_focus else 0), 'held %d focus mismatch' % number, failures)
        expect(port == expected, 'held %d mismatch port=%r original=%r' % (number, port[:3], expected[:3]), failures)
    details['held_cases'] = len(held_cases)

    # 5. Mouse axes and buttons.
    mouse_cases = [(1, 0, 1, 1, 0, 0, 30), (-2, -3, 0x80, 0, 0, 0, 31), (0x40, 0x20, 0x80, 1, 1, 0, 32),
                   (0, 0, 0x80, 1, 0, 3, 33), (5, -7, 0x40, 0, 1, 1, 34)]
    for flip in [0, 1]:
        for steady in [0, 1]:
            mouse_cases.append((-2147483648, -2147483648, 1, flip, steady, 0, 35))
    for _ in range(args.cases):
        mouse_cases.append((rng.randrange(-0x20000, 0x20000), rng.randrange(-0x20000, 0x20000),
                            rng.randrange(0x4000), rng.randrange(2), rng.randrange(2), rng.randrange(8),
                            rng.randrange(1, 5000)))
    lines = [' '.join(str(v) for v in case) for case in mouse_cases]
    outputs = run_probe(args.probe, 'mouse', lines)
    for number, (case, line) in enumerate(zip(mouse_cases, outputs)):
        port = [tuple(int(v) for v in record.split(':')) for record in line.split(';') if record]
        expected = original.mouse(*case)
        expect(port == expected, 'mouse %d mismatch port=%r original=%r' % (number, port, expected), failures)
    details['mouse_cases'] = len(mouse_cases)

    # 6. Key events.
    event_cases = [(34, 0, 0, 0, 50), (34, 0, 0, 1, 51), (17, 0, 0, 4, 52), (38, 0, 0, 0, 53),
                   (80, 0, 0, 0, 60), (80, 0, 0, 1, 61), (0xFE, 0, 0, 0, 62)]
    for _ in range(args.cases):
        # A key of 0 with no other bytes reads back as an empty ring slot in the
        # original, so the event mode keeps keys 1..255.
        event_cases.append((rng.choice([0x10, 0x11, 0x12, 0x20, 0x22, 0x26, 0x28, 0x31, 0x53, 0x50, 0xFE]),
                            rng.randrange(2), rng.randrange(2), rng.choice([0, 1, 4, 5]), rng.randrange(1, 5000)))
    lines = [' '.join(str(v) for v in case) for case in event_cases]
    outputs = run_probe(args.probe, 'event', lines)
    for number, (case, line) in enumerate(zip(event_cases, outputs)):
        port = [tuple(int(v) for v in record.split(':')) for record in line.split(';') if record]
        expected = original.event(*case)
        expect(port == expected, 'event %d mismatch port=%r original=%r' % (number, port[:4], expected[:4]), failures)
    details['event_cases'] = len(event_cases)

    # 7. Mouse pulses.
    pulse_cases = [(2, 0, 0), (4, 0, 0), (8, 0, 0), (8, 1, 0), (8, 0, 4), (0x20, 0, 0), (0x100, 0, 0), (0x200, 0, 0),
                   (1, 0, 0), (0x40, 0, 0)]
    for _ in range(args.cases):
        pulse_cases.append((rng.choice([1, 2, 4, 8, 0x10, 0x20, 0x40, 0x100, 0x200]), rng.randrange(2), rng.randrange(8)))
    lines = [' '.join(str(v) for v in case) for case in pulse_cases]
    outputs = run_probe(args.probe, 'pulse', lines)
    for number, (case, line) in enumerate(zip(pulse_cases, outputs)):
        port = [tuple(int(v) for v in record.split(':')) for record in line.split(';') if record]
        expected = original.pulse(*case)
        expect(port == expected, 'pulse %d mismatch port=%r original=%r' % (number, port, expected), failures)
    details['pulse_cases'] = len(pulse_cases)

    # 8. Menu/chat filter.
    filter_cases = [(0x59, (1, 0, 0, 0, 0, 0, 0x59, 0x4E, 0)), (0x4E, (1, 0, 0, 0, 0, 0, 0x59, 0x4E, 0)),
                    (0x41, (1, 0, 0, 0, 0, 0, 0x59, 0x4E, 0)), (0x37, (0, 1, 0, 0, 0, 0, 0x59, 0x4E, 0)),
                    (0x30, (0, 1, 0, 0, 0, 0, 0x59, 0x4E, 0)), (0x41, (0, 1, 0, 0, 0, 0, 0x59, 0x4E, 0)),
                    (0x21, (0, 0, 1, 0, 0, 0, 0x59, 0x4E, 0)), (0x20, (0, 0, 1, 0, 0, 0, 0x59, 0x4E, 0)),
                    (0x20, (0, 0, 0, 1, 0, 0, 0x59, 0x4E, 0)), (0x0D, (0, 0, 0, 1, 0, 0, 0x59, 0x4E, 0)),
                    (0x21, (0, 0, 0, 1, 1, 1, 0x59, 0x4E, 0)), (0x22, (0, 0, 0, 1, 1, 1, 0x59, 0x4E, 0)),
                    (0x20, (0, 0, 0, 1, 1, 1, 0x59, 0x4E, 0)), (0x20, (0, 0, 0, 0, 1, 0, 0x59, 0x4E, 4)),
                    (0x20, (0, 0, 0, 0, 1, 0, 0x59, 0x4E, 0)), (0x41, (0, 0, 0, 0, 0, 0, 0x59, 0x4E, 0))]
    for _ in range(args.cases):
        flags = (rng.randrange(2), rng.randrange(2), rng.randrange(2), rng.randrange(2),
                 rng.randrange(2), rng.randrange(2), rng.choice([0, 0x59, 0x41]), rng.choice([0, 0x4E, 0x42]),
                 rng.randrange(8))
        filter_cases.append((rng.choice([0x20, 0x0D, 0x21, 0x22, 0x30, 0x37, 0x41, 0x59, 0x4E, 0x10]), flags))
    lines = ['%d %s' % (key, ' '.join(str(v) for v in flags)) for key, flags in filter_cases]
    outputs = run_probe(args.probe, 'filter', lines)
    for number, ((key, flags), line) in enumerate(zip(filter_cases, outputs)):
        handled, effect, argument, pushed, dispatched = (int(v) for v in line.split(':'))
        pushed = -1 if pushed == 0xffffffff else pushed
        dispatched = -1 if dispatched == 0xffffffff else dispatched
        expected = original.filter_key(key, flags)
        expect((handled, effect, argument, pushed, dispatched) == expected,
               'filter %d port=%r original=%r' % (number, (handled, effect, argument, pushed, dispatched), expected),
               failures)
    details['filter_cases'] = len(filter_cases)

    # 9. Full frames.
    frame_cases = []
    for index in range(args.cases):
        case = {
            # The replay only drives the live path ([0xbd482c] == 0); the demo
            # queue-A path of 0x479450 is outside this stage.
            'head': (100 + index, rng.choice([0, 0, 0, 1]), 1, rng.randrange(2), 0,
                     rng.choice([0, 1, 2, 3]),
                     rng.randrange(-0x4000, 0x4000), rng.randrange(-0x4000, 0x4000),
                     rng.choice([0x80, 0x100, 0x40]), rng.randrange(2), rng.randrange(2)),
            'flags': (0, 0, 0, 0, 0, 0, 0x59, 0x4E, 0),
            'keys': random_keys(rng),
            'events': [(rng.choice([0x10, 0x11, 0x12, 0x20, 0x22, 0x26, 0x28, 0x31, 0x53, 0x50]),
                        rng.randrange(2), rng.randrange(2), 0) for _ in range(rng.randrange(3))],
            'pulses': [(rng.choice([2, 4, 8, 0x20, 0x100, 0x200]), rng.randrange(2), rng.randrange(8))
                       for _ in range(rng.randrange(2))],
        }
        frame_cases.append(case)
    frame_cases.append({'head': (200, 0, 1, 1, 0, 1, 1, -1, 1, 1, 0),
                        'flags': (0, 0, 0, 0, 0, 0, 0x59, 0x4E, 0), 'keys': [0x26, 0x11],
                        'events': [(17, 0, 0, 0)], 'pulses': []})
    frame_cases.append({'head': (201, 0, 1, 1, 0, 0, 1, 1, 0x80, 1, 0),
                        'flags': (0, 0, 0, 0, 0, 0, 0x59, 0x4E, 0), 'keys': [0x26],
                        'events': [(0, 0, 0, 0), (13, 0, 0, 0)], 'pulses': []})
    # Filter states inside a frame: typing confirm clears the flag, menu space
    # queues action 0xd, digits and pages leave their side effects.
    frame_cases.append({'head': (202, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0),
                        'flags': (1, 0, 0, 0, 0, 0, 0x59, 0x4E, 0), 'keys': [],
                        'events': [(0x59, 0, 0, 0)], 'pulses': []})
    frame_cases.append({'head': (203, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0),
                        'flags': (0, 0, 0, 1, 0, 0, 0x59, 0x4E, 0), 'keys': [],
                        'events': [(0x20, 0, 0, 0)], 'pulses': []})
    frame_cases.append({'head': (204, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0),
                        'flags': (0, 1, 0, 0, 0, 0, 0x59, 0x4E, 0), 'keys': [],
                        'events': [(0x37, 0, 0, 0)], 'pulses': []})
    frame_cases.append({'head': (205, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0),
                        'flags': (0, 0, 1, 0, 0, 0, 0x59, 0x4E, 0), 'keys': [],
                        'events': [(0x21, 0, 0, 0)], 'pulses': []})
    frame_cases.append({'head': (206, 1, 1, 1, 0, 1, 1, -1, 1, 1, 0),
                        'flags': (0, 0, 0, 0, 0, 0, 0x59, 0x4E, 0), 'keys': [0x26],
                        'events': [(0x41, 0, 0, 0)], 'pulses': []})
    lines = []
    for case in frame_cases:
        # 0x498ae0 sets key_state[vk] before queueing the event, so the same key
        # is seen by the polled held-key loop in the same frame. The event
        # modifiers are the shift/control key-state bytes at enqueue time.
        merged = list(case['keys'])
        for event in case['events']:
            if event[0] not in merged:
                merged.append(event[0])
        case['keys'] = merged
        modifiers = (1 if 0x10 in merged else 0) | (4 if 0x11 in merged else 0)
        events = []
        for event in case['events']:
            events.append((event[0], event[1], event[2], modifiers))
            if event[0] == 0x10:
                modifiers |= 1
            if event[0] == 0x11:
                modifiers |= 4
        case['events'] = events
        head = ' '.join(str(v) for v in case['head'])
        flags = ' '.join(str(v) for v in case['flags'])
        keys = '%d %s' % (len(case['keys']), ' '.join(str(k) for k in case['keys']))
        events = '%d %s' % (len(case['events']),
                            ' '.join(' '.join(str(v) for v in event) for event in case['events']))
        pulses = '%d %s' % (len(case['pulses']),
                            ' '.join(' '.join(str(v) for v in pulse) for pulse in case['pulses']))
        lines.append(' '.join([head, flags, keys, events, pulses]))
    outputs = run_probe(args.probe, 'frame', lines)
    for number, (case, line) in enumerate(zip(frame_cases, outputs)):
        # Queue records carry (id, tick, value, extra); 477d40 only gets id, value, extra.
        port = [(parts[0], parts[2], parts[3]) for parts in
                (tuple(int(v) for v in record.split(':')) for record in line.split(';') if record)]
        expected = original.frame(case)
        expect(port == expected, 'frame %d mismatch port=%r original=%r' % (number, port[:5], expected[:5]), failures)
    details['frame_cases'] = len(frame_cases)

    output = {
        'source_sha256': SOURCE_HASH,
        'probe': str(args.probe),
        'passed': not failures,
        'failures': failures,
        'details': details,
    }
    text = __import__('json').dumps(output, indent=2)
    print(text)
    if args.report:
        args.report.write_text(text + '\n')
    return 1 if failures else 0


if __name__ == '__main__':
    raise SystemExit(main())
