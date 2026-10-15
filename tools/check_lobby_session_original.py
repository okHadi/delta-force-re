#!/usr/bin/env python3
"""Replay original lobby session/host callbacks and compare with the rebuilt port.

Every case seeds the original globals, runs one original callback with stubbed
menu APIs, converts the captured stub calls and memory back into the probe line
format, and requires an exact match. Any extra or missing stub call is a
mismatch. External network helpers are stubs; nothing touches files or sockets.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

import pefile
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP,
    UC_X86_REG_EIP, UC_X86_REG_EFLAGS,
)

SOURCE_HASH = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
STACK, SCRATCH, STOP = 0x2000000, 0x3000000, 0x4000000
RECORD = 0x4ced20

G_PRECONNECT = 0x67f520
G_MAP_NAME = 0x67f438
G_LIST_HEAD = 0x501328
G_LIST_COUNT = 0x501330
G_SELECTED = 0x4d6b68
G_STATUS = 0x4d6b58
G_DOTS = 0x4d6b64
G_LOCAL_ID = 0x50362c
G_MAP_COUNTS = 0x4e65ac
G_MAP_BASES = 0x4e65c0
G_MAP_OFFSET = 0x4e65d4
G_MAP_NAMES = 0x4d6ba8
G_UI020 = 0x503234
G_SLIDER_MAX = 0x64
G_SLIDER_POS = 0x68
G_TEXT = 0x8
G_USER_ID = 0x84

MENUS = {
    0x4e6608: 'Host Menu', 0x4e6878: 'Pre Menu', 0x4e671c: 'Host2 Menu',
    0x4d34d0: 'ho3 Menu', 0x4d0cd4: 'Campaign Menu',
}

F = {
    'game_password': 0x94, 'game_name': 0xe4, 'country': 0x104,
    'red_password': 0xc4, 'blue_password': 0xd4,
    'mp_service': 0x148, 'mp_gametype': 0x14c, 'maxplayers': 0x150, 'dedicated': 0x154,
    'max_kills': 0x164, 'attributes': 0x16c, 'replay': 0x174, 'time_limit': 0x178,
    'koth_limit': 0x17c, 'ammo_respawn': 0x180, 'timeout': 0x184,
}
TEXT_FIELDS = {'game_name': 32, 'game_password': 32, 'country': 4, 'red_password': 16,
               'blue_password': 16}
SETTING_ORDER = ['mp_service', 'mp_gametype', 'maxplayers', 'dedicated', 'replay', 'max_kills',
                 'time_limit', 'koth_limit', 'ammo_respawn', 'timeout', 'attributes']

CALLBACKS = {
    'mp-host': 0x416f10, 'mp-join': 0x416f80, 'mp-search': 0x4173b0,
    'gamename': 0x418110, 'gamename-text': 0x418170,
    'sessionpassword': 0x418200, 'sessionpassword-text': 0x418260,
    'country': 0x4182f0, 'country-text': 0x418350,
    'servertype': 0x4183e0, 'gametype': 0x418460, 'maxplayers': 0x418680,
    'hostnext': 0x417ae0, 'hostaccept': 0x418ca0,
    'host2-accept': 0x41b490, 'host2-next': 0x41bc70, 'host2-replay': 0x41b660,
    'host2-maxplayers': 0x41b720, 'host2-score': 0x41b840, 'host2-time': 0x41b960,
    'host2-koth': 0x41ba80, 'host2-timeout': 0x41bcc0, 'host2-ammo': 0x41bbb0,
    'host2-breakx': 0x41bde0, 'host2-teamsel': 0x41be60, 'host2-tracers': 0x41bf60,
    'host2-password': 0x41bfe0, 'host2-password-text': 0x41c050,
    'select': 0x416670, 'row': 0x416670, 'status': 0x416500,
}

STUBS = [0x49a620, 0x4568e0, 0x40c630, 0x40e2a0, 0x40e340, 0x461340, 0x40b9a0, 0x417370,
         0x4172f0, 0x41e2d0, 0x417b60, 0x417530, 0x41ac00, 0x497580, 0x493a80, 0x49a092,
         0x410050, 0x44c190]


def hex_text(text):
    return text.encode('latin1').hex() if text else '-'


def read_cstring(machine, address, limit=200):
    raw = machine.mem_read(address, limit)
    end = raw.find(b'\0')
    return bytes(raw[:end if end >= 0 else limit]).decode('latin1')


def render_format(fmt, args):
    out = []
    at = 0
    index = 0
    while at < len(fmt):
        if fmt[at] != '%':
            out.append(fmt[at])
            at += 1
            continue
        at += 1
        if at < len(fmt) and fmt[at] == '%':
            out.append('%')
            at += 1
            continue
        flags = ''
        while at < len(fmt) and fmt[at] in '-+ #0':
            flags += fmt[at]
            at += 1
        width = ''
        while at < len(fmt) and fmt[at].isdigit():
            width += fmt[at]
            at += 1
        conversion = fmt[at]
        at += 1
        value = args[index]
        index += 1
        if conversion == 's':
            out.append(value)
        elif conversion in 'diu':
            number = value & 0xffffffff
            if number >= 0x80000000:
                number -= 0x100000000
            text = str(number)
            if '0' in flags and width:
                text = text.rjust(int(width), '0')
            out.append(text)
        else:
            raise RuntimeError(f'Unexpected conversion %{conversion}')
    return ''.join(out)


class Original:
    def __init__(self, data):
        pe = pefile.PE(data=data)
        self.machine = Uc(UC_ARCH_X86, UC_MODE_32)
        base = pe.OPTIONAL_HEADER.ImageBase
        size = (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095
        machine = self.machine
        machine.mem_map(base, size)
        machine.mem_write(base, pe.get_memory_mapped_image())
        pe.close()
        machine.mem_map(STACK, 0x10000)
        machine.mem_map(SCRATCH, 0x80000)
        machine.mem_map(STOP, 0x1000)
        self.case = 0
        self.buttons = {}
        self.slots = {}
        self.capture = []
        for address, name in MENUS.items():
            machine.mem_write(address, name.encode() + b'\0')
        for type_index in range(5):
            for index in range(64):
                machine.mem_write(G_MAP_NAMES + type_index * 0x3200 + index * 0x80,
                                  f'Map{index:02d}\0'.encode())
        for address in STUBS:
            machine.hook_add(UC_HOOK_CODE, self.make_stub(address), begin=address, end=address)

    def make_stub(self, address):
        def stub(machine, hook_address, size, user):
            getattr(self, 'stub_' + hex(address)[2:])(machine)
        return stub

    def finish(self, value=0):
        machine = self.machine
        sp = machine.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', machine.mem_read(sp, 4))[0]
        machine.reg_write(UC_X86_REG_ESP, sp + 4)
        machine.reg_write(UC_X86_REG_EAX, value)
        machine.reg_write(UC_X86_REG_EIP, ret)

    def args(self, count):
        machine = self.machine
        sp = machine.reg_read(UC_X86_REG_ESP)
        return list(struct.unpack(f'<{count}I', machine.mem_read(sp + 4, count * 4)))

    def button(self, name):
        if name not in self.buttons:
            slot = len(self.buttons)
            if slot >= 30:
                raise RuntimeError('Too many fake buttons in one case')
            self.slots[name] = slot
            self.buttons[name] = SCRATCH + 0x10000 + slot * 0x80
        return self.buttons[name]

    def button_name(self, pointer):
        return next((name for name, at in self.buttons.items() if at == pointer), '?')

    def set_button_text(self, name, text):
        pointer = self.button(name)
        buffer = SCRATCH + 0x20000 + self.slots[name] * 0x100
        self.machine.mem_write(buffer, text.encode() + b'\0')
        self.machine.mem_write(pointer + G_TEXT, struct.pack('<I', buffer))

    def set_button_user(self, name, user_id):
        self.machine.mem_write(self.button(name) + G_USER_ID, struct.pack('<i', user_id))

    def stub_40c630(self, machine):
        (name_pointer,) = self.args(1)
        name = read_cstring(machine, name_pointer)
        self.capture.append(('lookup', name))
        self.finish(self.button(name))

    def stub_40e2a0(self, machine):
        button, state = self.args(2)
        self.capture.append(('state', self.button_name(button), state))
        self.finish()

    def stub_40e340(self, machine):
        button, length = self.args(2)
        self.capture.append(('edit', self.button_name(button), length))
        self.finish()

    def stub_461340(self, machine):
        (menu_pointer,) = self.args(1)
        self.capture.append(('menu', read_cstring(machine, menu_pointer)))
        self.finish()

    def stub_40b9a0(self, machine):
        self.capture.append(('reset',))
        self.finish()

    def stub_417370(self, machine):
        self.capture.append(('stop_search',))
        self.finish()

    def stub_4172f0(self, machine):
        self.capture.append(('start_search',))
        self.finish()

    def stub_41e2d0(self, machine):
        (index,) = self.args(1)
        self.capture.append(('join', index))
        self.finish()

    def stub_417b60(self, machine):
        self.capture.append(('refresh_host',))
        self.finish()

    def stub_417530(self, machine):
        self.capture.append(('update_maps',))
        self.finish()

    def stub_41ac00(self, machine):
        self.capture.append(('refresh_host2',))
        self.finish()

    def stub_497580(self, machine):
        self.capture.append(('session_call30',))
        self.finish()

    def stub_49a620(self, machine):
        target, value, length = self.args(3)
        machine.mem_write(target, bytes([value & 0xff]) * length)
        self.capture.append(('memset',))
        self.finish(target)

    def stub_4568e0(self, machine):
        self.capture.append(('load_config',))
        self.finish()

    def stub_493a80(self, machine):
        dest, fmt_pointer = self.args(2)
        fmt = read_cstring(machine, fmt_pointer)
        ordered = []
        at = 0
        index = 0
        extra = self.args(14)[2:]
        while at < len(fmt):
            if fmt[at] != '%':
                at += 1
                continue
            at += 1
            if at < len(fmt) and fmt[at] == '%':
                at += 1
                continue
            while at < len(fmt) and fmt[at] in '-+ #0123456789.':
                at += 1
            conversion = fmt[at]
            at += 1
            ordered.append((conversion, extra[index]))
            index += 1
        rendered = render_format(fmt, [read_cstring(machine, value) if kind == 's' else value
                                       for kind, value in ordered])
        machine.mem_write(dest, rendered.encode('latin1') + b'\0')
        self.capture.append(('vsprintf', fmt, rendered))
        self.finish(len(rendered))

    def stub_49a092(self, machine):
        argv = self.args(6)
        key = read_cstring(machine, argv[2])
        arena = SCRATCH + 0x70000 + len(self.capture) * 0x100
        machine.mem_write(arena, f'<{key}>\0'.encode())
        obj = SCRATCH + 0x7f000
        machine.mem_write(obj, struct.pack('<I', arena))
        self.capture.append(('text', key))
        self.finish(obj)

    def stub_410050(self, machine):
        argv = self.args(7)
        self.capture.append(('draw_text', read_cstring(machine, argv[4])))
        self.finish()

    def stub_44c190(self, machine):
        argv = self.args(9)
        self.capture.append(('draw_rect_text', read_cstring(machine, argv[4])))
        self.finish()

    def reset_case(self):
        self.case += 1
        self.buttons.clear()
        self.capture.clear()
        machine = self.machine
        machine.mem_write(SCRATCH + 0x10000, bytes(30 * 0x80))
        machine.mem_write(RECORD, bytes(0x5c4))
        machine.mem_write(G_PRECONNECT, bytes(0x40))
        machine.mem_write(G_MAP_NAME, bytes(0x80))
        for address in (G_MAP_OFFSET, G_SELECTED, G_STATUS, G_DOTS, G_LIST_COUNT,
                        G_LIST_HEAD):
            machine.mem_write(address, bytes(4))
        for index in range(5):
            machine.mem_write(G_MAP_COUNTS + index * 4, bytes(4))
            machine.mem_write(G_MAP_BASES + index * 4, bytes(4))

    def set_int(self, address, value):
        self.machine.mem_write(address, struct.pack('<i', value))

    def get_int(self, address):
        return struct.unpack('<i', self.machine.mem_read(address, 4))[0]

    def get_text_field(self, name):
        return read_cstring(self.machine, RECORD + F[name], TEXT_FIELDS[name])

    def call(self, address, button, action):
        machine = self.machine
        sp = STACK + 0x8000
        machine.mem_write(sp, struct.pack('<3I', STOP, self.button(button), action))
        for reg in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
            machine.reg_write(reg, 0)
        machine.reg_write(UC_X86_REG_ESP, sp)
        machine.reg_write(UC_X86_REG_EFLAGS, 2)
        machine.emu_start(address, STOP, timeout=1000000, count=1000000)
        if machine.reg_read(UC_X86_REG_EIP) != STOP:
            raise RuntimeError(f'0x{address:x} did not return')

    def intents(self):
        out = []
        for entry in self.capture:
            kind = entry[0]
            if kind == 'state':
                out.append(f'INT SetButtonState {entry[1]} {entry[2]}')
            elif kind == 'edit':
                out.append(f'INT BeginTextEdit {entry[1]} {entry[2]}')
            elif kind == 'menu':
                out.append(f'INT SwitchMenu {entry[1]}')
            elif kind == 'reset':
                out.append('INT ResetMenuInput')
            elif kind == 'stop_search':
                out.append('INT StopSessionSearch')
            elif kind == 'start_search':
                out.append('INT StartSessionSearch')
            elif kind == 'join':
                out.append(f'INT JoinSession {entry[1]}')
            elif kind == 'refresh_host':
                out.append('INT RefreshHostMenu')
            elif kind == 'refresh_host2':
                out.append('INT RefreshHost2Menu')
            elif kind == 'update_maps':
                out.append('INT UpdateHostMapList')
            elif kind == 'session_call30':
                out.append('INT SessionObjectCall30')
        return out


def settings_ints(line, offset):
    return [int(value) for value in line.split()[offset:offset + 11]]


def make_settings(service=2, gametype=0, maxplayers=8, dedicated=0, replay=0, max_kills=0,
                  time_limit=0, koth_limit=0, ammo_respawn=2, timeout=0, attributes=0):
    return [service, gametype, maxplayers, dedicated, replay, max_kills, time_limit, koth_limit,
            ammo_respawn, timeout, attributes]


COUNTS_BASES = [6, 7, 3, 4, 5, 10, 20, 30, 40, 50, 0]


def make_cases():
    cases = []
    returns = [1, 2, 6, 7, 8]

    for selected, count, row in [(0, 3, 0), (0, 3, 2), (2, 5, 1), (4, 5, 4), (0, 5, 5),
                                 (2, 3, 3), (0, 0, 0)]:
        cases.append(f'select {selected} {count} {row}')

    for game_type in [0, 1, 2, 3, 4, 5, 9]:
        for flag in [0, 1]:
            for local in [0, 1]:
                version = 0x01050302 if local else 0x02010000
                cases.append(f'row Row{game_type}{flag}{local} 5 10 {game_type} {flag} '
                             f'{version} {0x01050302}')

    for service, status, dots, action in [(4, 0, 0, 3), (2, 0, 0, 3), (2, 1, 0, 3),
                                          (2, 1, 1, 3), (2, 1, 3, 3), (2, 2, 0, 3), (2, 0, 0, 2),
                                          (6, 1, 2, 3)]:
        cases.append(f'status {service} {status} {dots} {action}')

    for action in returns:
        cases.append(f'mp-host {action}')
    for action in returns:
        cases.append('mp-join {} {} 0'.format(' '.join(map(str, make_settings(service=4))), action))
        cases.append('mp-join {} {} 3'.format(' '.join(map(str, make_settings(service=2))), action))
    for status in [0, 1]:
        for action in returns:
            cases.append(f'mp-search {status} {action}')

    for action in returns:
        cases.append(f'gamename {action}')
        cases.append(f'sessionpassword {action}')
        cases.append(f'country {action}')
    cases.append('gamename-text 4 Alpha12')
    cases.append('gamename-text 4 ' + 'A' * 31)
    cases.append('gamename-text 4 ' + 'B' * 31)
    cases.append('gamename-text 5 Alpha')
    for action in [1, 2, 6, 7]:
        cases.append(f'gamename-text {action} Alpha')
    cases.append('sessionpassword-text 4 pass123')
    cases.append('sessionpassword-text 4 ' + 'C' * 16)
    for action in [5, 6, 7]:
        cases.append(f'sessionpassword-text {action} pass')
    cases.append('country-text 4 US')
    cases.append('country-text 4 USA')
    for action in [5, 6]:
        cases.append(f'country-text {action} US')

    for action in [1, 2, 6, 7]:
        cases.append(f'servertype {action} but_ho_sew')
    cases.append('servertype 2 but_ho_sel')
    cases.append('servertype 2 but_ho_ser')

    seed = ' '.join(map(str, COUNTS_BASES))
    for action in [1, 2, 6, 7]:
        for user_id in ([0, 1, 5, 6] if action == 1 else [0]):
            cases.append(f'gametype {seed} '
                         f'{" ".join(map(str, make_settings(maxplayers=16)))} {action} {user_id}')
    cases.append(f'gametype {seed} {" ".join(map(str, make_settings(maxplayers=9)))} 1 0')
    cases.append(f'gametype {seed} {" ".join(map(str, make_settings(maxplayers=8)))} 1 0')

    for action in [1, 2, 6, 7]:
        for minus in [0, 1]:
            for value in [2, 3, 31, 32, 33]:
                cases.append(f'maxplayers {" ".join(map(str, make_settings(maxplayers=value)))} '
                             f'{action} {minus}')

    for action in returns:
        cases.append(f'hostnext {action}')

    for action in [1, 2, 6, 7]:
        cases.append(f'hostaccept {seed} {" ".join(map(str, make_settings()))} {action}')
    for gametype in [0, 6]:
        cases.append(f'hostaccept {seed} '
                     f'{" ".join(map(str, make_settings(gametype=gametype, dedicated=1)))} 2')
    for gametype in [1, 2, 5]:
        cases.append(f'hostaccept {seed} '
                     f'{" ".join(map(str, make_settings(gametype=gametype, dedicated=0)))} 2')

    for action in [1, 2, 6, 7, 8]:
        cases.append(f'host2-accept {action}')
        cases.append(f'host2-next {action}')
    for action in [1, 2, 6, 7]:
        for minus in [0, 1]:
            for replay in [0, 1, 2]:
                cases.append(f'host2-replay '
                             f'{" ".join(map(str, make_settings(replay=replay)))} {action} {minus}')
    for action in [1, 2, 6, 7]:
        for minus in [0, 1]:
            for gametype, value in [(0, 2), (0, 8), (0, 9), (1, 2), (1, 16), (1, 17)]:
                cases.append(f'host2-maxplayers '
                             f'{" ".join(map(str, make_settings(gametype=gametype, maxplayers=value)))} '
                             f'{action} {minus}')
    for command, field, values in [('host2-score', 'max_kills', [-1, 0, 99, 100, 101]),
                                   ('host2-time', 'time_limit', [-1, 0, 99, 100, 101]),
                                   ('host2-koth', 'koth_limit', [0, 1, 100, 101]),
                                   ('host2-timeout', 'timeout', [-1, 0, 100, 101])]:
        for value in values:
            for minus in [0, 1]:
                cases.append(f'{command} '
                             f'{" ".join(map(str, make_settings(**{field: value})))} 2 {minus} 0')
        for slider in [-5, 0, 1, 50, 100, 500]:
            cases.append(f'{command} {" ".join(map(str, make_settings()))} 3 0 {slider}')
        for action in [1, 6, 7]:
            cases.append(f'{command} {" ".join(map(str, make_settings()))} {action} 0 0')
    for action in [1, 2, 6, 7]:
        for minus in [0, 1]:
            for value in [0, 1, 3]:
                cases.append(f'host2-ammo '
                             f'{" ".join(map(str, make_settings(ammo_respawn=value)))} {action} {minus}')
    for command, attr in [('host2-breakx', 1), ('host2-teamsel', 0x40), ('host2-tracers', 2)]:
        for action in [1, 2, 6, 7]:
            for value in [0, attr, 2306]:
                cases.append(f'{command} '
                             f'{" ".join(map(str, make_settings(attributes=value)))} {action}')
    for action in [1, 2, 6, 7]:
        for user_id in [0, 1]:
            cases.append(f'host2-password {action} {user_id}')
    for action in [2, 4, 5, 6, 7]:
        for user_id in [0, 1]:
            cases.append(f'host2-password-text {action} {user_id} SECRET')
    return cases


def seed_settings(original, values):
    for key, value in zip(SETTING_ORDER, values):
        original.set_int(RECORD + F[key], value)


def seed_host_lists(original, counts, bases, offset):
    for index, value in enumerate(counts):
        original.set_int(G_MAP_COUNTS + index * 4, value)
    for index, value in enumerate(bases):
        original.set_int(G_MAP_BASES + index * 4, value)
    original.set_int(G_MAP_OFFSET, offset)


KNOWN_CAPTURE_KINDS = {'lookup', 'state', 'edit', 'menu', 'reset', 'stop_search', 'start_search',
                       'join', 'refresh_host', 'refresh_host2', 'update_maps', 'session_call30',
                       'memset', 'load_config', 'vsprintf', 'text', 'draw_text', 'draw_rect_text'}


def expected_lines(original, line):
    command = line.split()[0]
    original.reset_case()
    machine = original.machine
    state = []

    def run(button_name, action, user_id=None):
        if user_id is not None:
            original.set_button_user(button_name, user_id)
        original.call(CALLBACKS[command], button_name, action)

    if command == 'select':
        selected, count, row = (int(x) for x in line.split()[1:4])
        nodes = [SCRATCH + 0x40000 + index * 0x40 for index in range(count)]
        for index, node in enumerate(nodes):
            following = nodes[index + 1] if index + 1 < count else 0
            machine.mem_write(node, struct.pack('<I', following))
        if nodes:
            machine.mem_write(G_LIST_HEAD, struct.pack('<I', nodes[0]))
        original.set_int(G_LIST_COUNT, count)
        original.set_int(G_SELECTED, selected)
        run(f'but_mp_s{row + 1}', 1, row)
        state.append(f'SET selected={original.get_int(G_SELECTED)}')
    elif command == 'row':
        name, players, max_players, game_type, flag, version, local_id = line.split()[1:8]
        node = SCRATCH + 0x40000
        name_buffer = SCRATCH + 0x41000
        machine.mem_write(name_buffer, name.encode() + b'\0')
        machine.mem_write(node, bytes(4))
        machine.mem_write(node + 0x0c, struct.pack('<I', name_buffer))
        machine.mem_write(node + 0x34, struct.pack('<i', int(players)))
        machine.mem_write(node + 0x38, struct.pack('<i', int(max_players)))
        machine.mem_write(node + 0x3c, struct.pack('<i', int(game_type)))
        machine.mem_write(node + 0x40, struct.pack('<I', int(flag)))
        machine.mem_write(node + 0x44, struct.pack('<I', int(version, 0) & 0xffffffff))
        machine.mem_write(G_LIST_HEAD, struct.pack('<I', node))
        original.set_int(G_LIST_COUNT, 1)
        machine.mem_write(G_LOCAL_ID, struct.pack('<I', int(local_id, 0) & 0xffffffff))
        machine.mem_write(G_UI020, struct.pack('<I', SCRATCH + 0x60000))
        run('but_mp_s1', 3)
        keys = [entry[1] for entry in original.capture if entry[0] == 'text']
        versions = [entry[2] for entry in original.capture
                    if entry[0] == 'vsprintf' and entry[1].startswith('V%')]
        rows = [entry[2] for entry in original.capture
                if entry[0] == 'vsprintf' and not entry[1].startswith('V%')]
        draws = [entry[1] for entry in original.capture if entry[0] == 'draw_text']
        if not rows or not versions:
            raise RuntimeError(f'row case produced no render: {line}')
        if draws != rows:
            raise RuntimeError(f'row draw text differs from render: {line}')
        if len(keys) != 2 or len(rows) != 1 or len(versions) != 1 or len(draws) != 1:
            raise RuntimeError(f'unexpected row capture shape: {line}')
        adjust = 0 if int(flag) == 0 else -1
        state.append(f'ROW {name} {int(players) + adjust} {int(max_players) + adjust} '
                     f'{[key for key in keys if key != "OF"][0]} {versions[0]} '
                     f'{1 if int(version, 0) == int(local_id, 0) else 0} {1 if int(flag) else 0}')
        state.append(f'TEXT {hex_text(rows[0])}')
        return state
    elif command == 'status':
        service, status, dots, action = (int(x) for x in line.split()[1:5])
        original.set_int(RECORD + F['mp_service'], service)
        original.set_int(G_STATUS, status)
        original.set_int(G_DOTS, dots)
        original.set_int(G_UI020, SCRATCH + 0x60000)
        button = original.button('but_mp_st')
        machine.mem_write(button + 0x54, struct.pack('<i', 100))
        machine.mem_write(button + 0x58, struct.pack('<i', 200))
        machine.mem_write(button + 0x5c, struct.pack('<I', SCRATCH + 0x61000))
        original.call(0x416500, 'but_mp_st', action)
        texts = [entry[1] for entry in original.capture if entry[0] == 'draw_rect_text']
        state.append(f'TEXT {hex_text(texts[0]) if texts else "-"}')
        return state
    elif command == 'mp-host':
        run('but_mp_hotxt', int(line.split()[1]))
    elif command == 'mp-join':
        seed_settings(original, settings_ints(line, 1))
        original.set_int(G_SELECTED, int(line.split()[13]))
        run('but_mp_jo', int(line.split()[12]))
        state.append('PRE {} {} {} {} {} {}'.format(
            original.get_int(G_PRECONNECT), original.get_int(G_PRECONNECT + 4),
            original.get_int(G_PRECONNECT + 8), original.get_int(G_PRECONNECT + 0xc),
            original.get_int(G_PRECONNECT + 0x10), original.get_int(G_PRECONNECT + 0x1c)))
    elif command == 'mp-search':
        status, action = (int(x) for x in line.split()[1:3])
        original.set_int(G_STATUS, status)
        run('but_mp_se', action)
    elif command in ('gamename', 'sessionpassword', 'country'):
        button = {'gamename': 'but_ho_gn', 'sessionpassword': 'but_ho_sp',
                  'country': 'but_ho_cty'}[command]
        run(button, int(line.split()[1]))
    elif command in ('gamename-text', 'sessionpassword-text', 'country-text'):
        field = {'gamename-text': 'game_name', 'sessionpassword-text': 'game_password',
                 'country-text': 'country'}[command]
        button = {'gamename-text': 'but_ho_gnw', 'sessionpassword-text': 'but_ho_spw',
                  'country-text': 'but_ho_ctyw'}[command]
        original.set_button_text(button, line.split()[2])
        run(button, int(line.split()[1]))
        state.append(f'SET {field}={hex_text(original.get_text_field(field))}')
    elif command == 'servertype':
        run(line.split()[2], int(line.split()[1]))
        state.append(f'SET dedicated={original.get_int(RECORD + F["dedicated"])}')
    elif command == 'gametype':
        seed_host_lists(original, [int(x) for x in line.split()[1:6]],
                        [int(x) for x in line.split()[6:11]], int(line.split()[11]))
        seed_settings(original, settings_ints(line, 12))
        action = int(line.split()[23])
        user_id = int(line.split()[24])
        run(f'but_ho_gt{user_id + 1}', action, user_id)
        slider = original.button('but_ho_selsl')
        state.append('SET gametype={} maxplayers={} slider_max={} slider_pos={} map_offset={}'.format(
            original.get_int(RECORD + F['mp_gametype']),
            original.get_int(RECORD + F['maxplayers']),
            original.get_int(slider + G_SLIDER_MAX), original.get_int(slider + G_SLIDER_POS),
            original.get_int(G_MAP_OFFSET)))
    elif command == 'maxplayers':
        seed_settings(original, settings_ints(line, 1))
        minus = int(line.split()[13])
        run('but_ho_mpl' if minus else 'but_ho_mpr', int(line.split()[12]))
        state.append(f'SET maxplayers={original.get_int(RECORD + F["maxplayers"])}')
    elif command == 'hostnext':
        run('but_ho_next', int(line.split()[1]))
    elif command == 'hostaccept':
        seed_host_lists(original, [int(x) for x in line.split()[1:6]],
                        [int(x) for x in line.split()[6:11]], int(line.split()[11]))
        seed_settings(original, settings_ints(line, 12))
        run('but_ho_actxt', int(line.split()[23]))
        state.append('PRE {} {} {} {} {} {}'.format(
            original.get_int(G_PRECONNECT), original.get_int(G_PRECONNECT + 4),
            original.get_int(G_PRECONNECT + 8), original.get_int(G_PRECONNECT + 0xc),
            original.get_int(G_PRECONNECT + 0x10), original.get_int(G_PRECONNECT + 0x1c)))
        map_name = read_cstring(machine, G_MAP_NAME, 0x80)
        state.append(f'MAP {map_name if map_name else "-"}')
    elif command in ('host2-accept', 'host2-next'):
        button = 'but_ho2_actxt' if command == 'host2-accept' else 'but_ho2_wtxt'
        run(button, int(line.split()[1]))
    elif command == 'host2-replay':
        seed_settings(original, settings_ints(line, 1))
        minus = int(line.split()[13])
        run('but_ho2_irl' if minus else 'but_ho2_irr', int(line.split()[12]))
        state.append(f'SET replay={original.get_int(RECORD + F["replay"])}')
    elif command == 'host2-maxplayers':
        seed_settings(original, settings_ints(line, 1))
        minus = int(line.split()[13])
        run('but_ho2_mpl' if minus else 'but_ho2_mpr', int(line.split()[12]))
        state.append(f'SET maxplayers={original.get_int(RECORD + F["maxplayers"])}')
    elif command in ('host2-score', 'host2-time', 'host2-koth', 'host2-timeout'):
        field = {'host2-score': 'max_kills', 'host2-time': 'time_limit',
                 'host2-koth': 'koth_limit', 'host2-timeout': 'timeout'}[command]
        minus_name = {'host2-score': 'but_ho2_sll', 'host2-time': 'but_ho2_tll',
                      'host2-koth': 'but_ho2_kohl', 'host2-timeout': 'but_ho2_tol'}[command]
        plus_name = {'host2-score': 'but_ho2_slr', 'host2-time': 'but_ho2_tlr',
                     'host2-koth': 'but_ho2_kohr', 'host2-timeout': 'but_ho2_tor'}[command]
        slider_name = {'host2-score': 'but_ho2_slsl', 'host2-time': 'but_ho2_tlsl',
                       'host2-koth': 'but_ho2_kohsl', 'host2-timeout': 'but_ho2_tosl'}[command]
        seed_settings(original, settings_ints(line, 1))
        action = int(line.split()[12])
        minus = int(line.split()[13])
        original.set_int(original.button(slider_name) + G_SLIDER_POS, int(line.split()[14]))
        run(minus_name if minus else plus_name, action)
        state.append('SET max_kills={} time_limit={} koth_limit={} timeout={} slider={}'.format(
            original.get_int(RECORD + F['max_kills']),
            original.get_int(RECORD + F['time_limit']),
            original.get_int(RECORD + F['koth_limit']),
            original.get_int(RECORD + F['timeout']),
            original.get_int(original.button(slider_name) + G_SLIDER_POS)))
    elif command == 'host2-ammo':
        seed_settings(original, settings_ints(line, 1))
        minus = int(line.split()[13])
        run('but_ho2_arl' if minus else 'but_ho2_arr', int(line.split()[12]))
        state.append(f'SET ammo_respawn={original.get_int(RECORD + F["ammo_respawn"])}')
    elif command in ('host2-breakx', 'host2-teamsel', 'host2-tracers'):
        button = {'host2-breakx': 'but_ho2_bxw', 'host2-teamsel': 'but_ho2_tsw',
                  'host2-tracers': 'but_ho2_trw'}[command]
        seed_settings(original, settings_ints(line, 1))
        run(button, int(line.split()[12]))
        state.append(f'SET attributes={original.get_int(RECORD + F["attributes"])}')
    elif command == 'host2-password':
        action, user_id = (int(x) for x in line.split()[1:3])
        run('but_ho2_rtd' if user_id == 0 else 'but_ho2_btd', action, user_id)
    elif command == 'host2-password-text':
        action, user_id = (int(x) for x in line.split()[1:3])
        button = 'but_ho2_rtw' if user_id == 0 else 'but_ho2_btw'
        original.set_button_text(button, line.split()[3])
        run(button, action, user_id)
        state.append('SET red={} blue={}'.format(
            hex_text(original.get_text_field('red_password')),
            hex_text(original.get_text_field('blue_password'))))
    else:
        raise RuntimeError(f'Unknown case command {command}')
    for entry in original.capture:
        if entry[0] not in KNOWN_CAPTURE_KINDS:
            raise RuntimeError(f'unknown stub capture {entry[0]} in {line}')
    lines = original.intents()
    lines.append('END')
    lines.extend(state)
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path, help='original Df.exe')
    parser.add_argument('probe', type=Path, help='built lobby_session_probe')
    args = parser.parse_args()
    try:
        data = args.executable.read_bytes()
        if hashlib.sha256(data).hexdigest() != SOURCE_HASH:
            raise ValueError('Expected the documented Df.exe SHA-256')
        probe = args.probe.resolve()
        original = Original(data)
        cases = make_cases()
        mismatches = []
        for line in cases:
            expected = expected_lines(original, line)
            result = subprocess.run([str(probe)], input=line + '\n', capture_output=True,
                                    text=True, check=True)
            actual = result.stdout.splitlines()
            if actual != expected:
                mismatches.append({'case': line, 'expected': expected[:12], 'actual': actual[:12]})
        print(json.dumps({
            'source_sha256': SOURCE_HASH,
            'unicorn_version': unicorn.__version__,
            'cases': len(cases),
            'mismatch_count': len(mismatches),
            'mismatches': mismatches[:5],
            'scope': 'Original lobby session and host callbacks replayed with stubbed menu APIs, '
                     'settings memory, and network helpers; no files or sockets',
        }, indent=2))
        return int(bool(mismatches))
    except (OSError, ValueError, RuntimeError, IndexError, pefile.PEFormatError, unicorn.UcError,
            subprocess.CalledProcessError) as error:
        print(f'check_lobby_session: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
