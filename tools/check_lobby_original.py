#!/usr/bin/env python3
"""Compare the multiplayer settings port with original x86; stub all file I/O."""

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
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP,
    UC_X86_REG_EIP, UC_X86_REG_EFLAGS,
)

SOURCE_HASH = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
DEFAULTS = 0x407120
WRITER = 0x407360
MEMSET = 0x49a620
LOAD_CONFIG = 0x4568e0
LOAD_KEYS = 0x407040
FOPEN = 0x49a41c
FPRINTF = 0x49bac0
FCLOSE = 0x49a840
RECORD_BASE = 0x4ced20
RECORD_SIZE = 0x5c4
STACK, STOP = 0x2000000, 0x3000000

# Original field offsets inside the 0x4ced20 record.
FIELDS = {
    'login_password': (0x074, 32), 'game_password': (0x094, 32),
    'squad_password': (0x0b4, 16), 'red_password': (0x0c4, 16),
    'blue_password': (0x0d4, 16), 'game_name': (0x0e4, 32),
    'country': (0x104, 4), 'phone_number': (0x108, 32),
    'internet_address': (0x128, 32),
    'mp_service': (0x148, 4), 'mp_gametype': (0x14c, 4), 'maxplayers': (0x150, 4),
    'dedicated': (0x154, 4), 'com_port': (0x158, 4), 'baud_rate': (0x15c, 4),
    'max_lives': (0x160, 4), 'max_kills': (0x164, 4), 'team_req': (0x168, 4),
    'attributes': (0x16c, 4), 'replay': (0x174, 4), 'time_limit': (0x178, 4),
    'koth_limit': (0x17c, 4), 'ammo_respawn': (0x180, 4), 'timeout': (0x184, 4),
    'gpsicon': (0x188, 4), 'showtags': (0x18c, 4), 'contest': (0x190, 4),
    'flag_5b4': (0x5b4, 4), 'whichbrowser': (0x5b8, 4), 'turbo': (0x5bc, 4),
    'mp_verbose': (0x5c0, 4),
}
COVERED = (range(0x74, 0x194), range(0x5b4, 0x5c4))


def covered_hex(record):
    parts = []
    for block in COVERED:
        parts.append(record[block.start:block.stop].hex())
    return ''.join(parts)


def custom_record():
    record = bytearray(RECORD_SIZE)
    def put_text(name, text):
        at, size = FIELDS[name]
        raw = text.encode() + b'\0'
        record[at:at + len(raw)] = raw[:size]
    def put_int(name, value):
        at, size = FIELDS[name]
        record[at:at + size] = struct.pack('<i', value)
    put_text('login_password', 'alpha')
    put_text('game_password', 'beta%5')
    put_text('squad_password', 'sq')
    put_text('red_password', 'red')
    put_text('blue_password', 'blue')
    put_text('game_name', 'Delta "One" 100%')
    put_text('country', 'US')
    put_text('phone_number', '555-01')
    put_text('internet_address', '127.0.0.1')
    put_int('mp_service', 2)
    put_int('mp_gametype', 5)
    put_int('maxplayers', 8)
    put_int('dedicated', 1)
    put_int('com_port', 15827)
    put_int('baud_rate', 28800)
    put_int('max_lives', 0)
    put_int('max_kills', 255)
    put_int('team_req', 7)
    put_int('attributes', 2306)
    put_int('replay', 1)
    put_int('time_limit', 45)
    put_int('koth_limit', 15)
    put_int('ammo_respawn', 0)
    put_int('timeout', 99)
    put_int('gpsicon', 2)
    put_int('showtags', 1)
    put_int('contest', 1)
    put_int('flag_5b4', 0)
    put_int('whichbrowser', 3)
    put_int('turbo', 4)
    put_int('mp_verbose', 0)
    record[0x170:0x174] = struct.pack('<i', 0x12345678)
    return bytes(record)


def random_record(rng):
    record = bytearray(RECORD_SIZE)
    alphabet = ' ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"_%\\'
    for name, (at, size) in FIELDS.items():
        if size == 4 and name in ('country',):
            text = ''.join(rng.choice(alphabet) for _ in range(rng.randrange(0, 3)))
            record[at:at + len(text) + 1] = text.encode() + b'\0'
        elif size != 4:
            text = ''.join(rng.choice(alphabet) for _ in range(rng.randrange(0, 12)))
            record[at:at + len(text) + 1] = text.encode() + b'\0'
        else:
            record[at:at + 4] = struct.pack('<i', rng.randrange(-1000, 100000))
    return bytes(record)


def read_cstring(machine, address, limit=160):
    raw = machine.mem_read(address, limit)
    end = raw.find(b'\0')
    return bytes(raw[:end if end >= 0 else limit])


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
        machine.mem_map(STOP, 0x1000)
        self.calls = []
        self.lines = []
        self.opened = None
        machine.hook_add(UC_HOOK_CODE, self.memset_stub, begin=MEMSET, end=MEMSET)
        machine.hook_add(UC_HOOK_CODE, self.load_config_stub, begin=LOAD_CONFIG, end=LOAD_CONFIG)
        machine.hook_add(UC_HOOK_CODE, self.load_keys_stub, begin=LOAD_KEYS, end=LOAD_KEYS)
        machine.hook_add(UC_HOOK_CODE, self.fopen_stub, begin=FOPEN, end=FOPEN)
        machine.hook_add(UC_HOOK_CODE, self.fprintf_stub, begin=FPRINTF, end=FPRINTF)
        machine.hook_add(UC_HOOK_CODE, self.fclose_stub, begin=FCLOSE, end=FCLOSE)

    def finish_call(self, value):
        # Original call sites are cdecl: the caller adjusts ESP after the call.
        machine = self.machine
        sp = machine.reg_read(UC_X86_REG_ESP)
        address = struct.unpack('<I', machine.mem_read(sp, 4))[0]
        machine.reg_write(UC_X86_REG_ESP, sp + 4)
        machine.reg_write(UC_X86_REG_EAX, value)
        machine.reg_write(UC_X86_REG_EIP, address)

    def memset_stub(self, machine, address, size, user):
        sp = machine.reg_read(UC_X86_REG_ESP)
        target, value, length = struct.unpack('<3I', machine.mem_read(sp + 4, 12))
        self.calls.append(('memset', target, value, length))
        machine.mem_write(target, bytes([value & 0xff]) * length)
        self.finish_call(target)

    def load_config_stub(self, machine, address, size, user):
        sp = machine.reg_read(UC_X86_REG_ESP)
        path, callback = struct.unpack('<2I', machine.mem_read(sp + 4, 8))
        self.calls.append(('load_config', path, callback))
        self.finish_call(0)

    def load_keys_stub(self, machine, address, size, user):
        self.calls.append(('load_keys',))
        self.finish_call(0)

    def fopen_stub(self, machine, address, size, user):
        sp = machine.reg_read(UC_X86_REG_ESP)
        path, mode = struct.unpack('<2I', machine.mem_read(sp + 4, 8))
        self.calls.append(('fopen', read_cstring(machine, path).decode('ascii', 'replace'),
                           read_cstring(machine, mode).decode('ascii', 'replace')))
        self.opened = 0x1234
        self.finish_call(self.opened)

    def fprintf_stub(self, machine, address, size, user):
        sp = machine.reg_read(UC_X86_REG_ESP)
        handle, fmt_pointer = struct.unpack('<2I', machine.mem_read(sp + 4, 8))
        fmt = read_cstring(machine, fmt_pointer).decode('ascii', 'replace')
        consumed = 0
        values = []
        at = 0
        while at < len(fmt):
            if fmt[at] != '%':
                at += 1
                continue
            at += 1
            while at < len(fmt) and fmt[at] in '-+ #0123456789.':
                at += 1
            if at >= len(fmt):
                break
            conversion = fmt[at]
            at += 1
            if conversion == '%':
                continue
            argument = struct.unpack('<I', machine.mem_read(sp + 12 + 4 * consumed, 4))[0]
            consumed += 1
            if conversion == 's':
                values.append(read_cstring(machine, argument).decode('ascii', 'replace'))
            elif conversion in 'diu':
                values.append(struct.unpack('<i', struct.pack('<I', argument))[0])
            else:
                raise RuntimeError(f'Unexpected fprintf conversion %{conversion}')
        if handle != self.opened:
            raise RuntimeError('fprintf used an unexpected handle')
        self.lines.append((fmt, values))
        self.finish_call(1)

    def fclose_stub(self, machine, address, size, user):
        sp = machine.reg_read(UC_X86_REG_ESP)
        handle = struct.unpack('<I', machine.mem_read(sp + 4, 4))[0]
        self.calls.append(('fclose', handle))
        if handle != self.opened:
            raise RuntimeError('fclose used an unexpected handle')
        self.finish_call(0)

    def call(self, procedure):
        machine = self.machine
        sp = STACK + 0x8000
        machine.mem_write(sp, struct.pack('<I', STOP))
        for reg in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
            machine.reg_write(reg, 0)
        machine.reg_write(UC_X86_REG_ESP, sp)
        machine.reg_write(UC_X86_REG_EFLAGS, 2)
        machine.emu_start(procedure, STOP, timeout=1000000, count=1000000)
        if machine.reg_read(UC_X86_REG_EIP) != STOP:
            raise RuntimeError(f'0x{procedure:x} did not return')

    def record(self):
        return bytes(self.machine.mem_read(RECORD_BASE, RECORD_SIZE))

    def run_defaults(self):
        self.calls.clear()
        self.call(DEFAULTS)
        return self.record()

    def write(self, record):
        self.machine.mem_write(RECORD_BASE, record)
        self.lines.clear()
        self.calls.clear()
        self.call(WRITER)
        return list(self.lines), list(self.calls)


def selected_lines(lines):
    """Pick the 24 // MULTIPLAYER lines and the 4 shared // SYSTEM settings."""
    rendered = [(fmt, values) for fmt, values in lines]
    texts = [(fmt % tuple(values) if values else fmt) for fmt, values in rendered]
    try:
        start = texts.index('// MULTIPLAYER\n')
        system = texts.index('// SYSTEM\n')
    except ValueError as error:
        raise RuntimeError('Writer did not emit the multiplayer sections') from error
    multiplayer = rendered[start + 1:system]
    blank = [index for index, (fmt, _) in enumerate(multiplayer) if fmt == '\n']
    if not blank:
        raise RuntimeError('Unexpected multiplayer section shape')
    multiplayer = multiplayer[:blank[0]]
    system_lines = rendered[system + 1:]
    blank = [index for index, (fmt, _) in enumerate(system_lines) if fmt == '\n']
    if not blank:
        raise RuntimeError('Unexpected system section shape')
    system_lines = system_lines[:blank[0]]
    if len(multiplayer) != 24 or len(system_lines) != 6:
        raise RuntimeError('Unexpected multiplayer or system line counts')
    result = []
    for fmt, values in multiplayer + system_lines[-4:]:
        line = fmt % tuple(values) if values else fmt
        if not line.endswith('\n') or line.endswith('\n\n'):
            raise RuntimeError('Unexpected writer line ending')
        result.append(line.rstrip('\n'))
    return result


def probe_lines(probe, command, expect_marker=True):
    result = subprocess.run([str(probe)], input=command + '\n', capture_output=True, text=True,
                            check=True)
    lines = result.stdout.splitlines()
    if expect_marker:
        if not lines or lines[-1] != '.':
            raise RuntimeError('Probe did not finish a render command')
        return lines[:-1]
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path, help='original Df.exe')
    parser.add_argument('probe', type=Path, help='built lobby_probe')
    args = parser.parse_args()
    try:
        data = args.executable.read_bytes()
        if hashlib.sha256(data).hexdigest() != SOURCE_HASH:
            raise ValueError('Expected the documented Df.exe SHA-256')
        probe = args.probe.resolve()
        original = Original(data)

        mismatches = []
        cases = 0

        # Case A: the defaults pass at 0x407120.
        record = original.run_defaults()
        expected = probe_lines(probe, 'defaults', expect_marker=False)
        if len(expected) != 1:
            raise RuntimeError('Probe printed an unexpected record length')
        if covered_hex(record) != covered_hex(bytes.fromhex(expected[0])):
            mismatches.append({'case': 'defaults'})
        if original.calls != [('memset', RECORD_BASE, 0, RECORD_SIZE),
                              ('load_config', 0x4c869c, 0x406640), ('load_keys',)]:
            mismatches.append({'case': 'defaults-calls', 'calls': original.calls})
        cases += 1
        defaults_record = record

        # Case B: the writer pass at 0x407360 with seeded records.
        seeds = [custom_record()]
        rng = random.Random(0xdf10bb)
        seeds.extend(random_record(rng) for _ in range(24))
        for index, seed in enumerate(seeds):
            captured, calls = original.write(seed)
            cases += 1
            if not calls or calls[0] != ('fopen', 'df.cfg', 'w') or ('fclose', 0x1234) not in calls:
                mismatches.append({'case': f'writer-files-{index}', 'calls': calls[:2] + calls[-1:]})
                continue
            captured_lines = selected_lines(captured)
            expected = probe_lines(probe, 'render-hex ' + seed.hex())
            if captured_lines != expected:
                mismatches.append({'case': f'writer-{index}'})

        # Case C: original defaults followed by the original writer.
        captured, _ = original.write(defaults_record)
        cases += 1
        captured_lines = selected_lines(captured)
        expected = probe_lines(probe, 'defaults-render')
        if captured_lines != expected:
            mismatches.append({'case': 'defaults-render'})

        print(json.dumps({
            'source_sha256': SOURCE_HASH,
            'procedures': {'defaults': hex(DEFAULTS), 'writer': hex(WRITER)},
            'unicorn_version': unicorn.__version__,
            'cases': cases,
            'mismatch_count': len(mismatches),
            'mismatches': mismatches[:10],
            'scope': 'Multiplayer settings record and df.cfg lines; memset, config load, key load, '
                     'fopen, fprintf, and fclose stubbed; no file or network access',
        }, indent=2))
        return int(bool(mismatches))
    except (OSError, ValueError, RuntimeError, pefile.PEFormatError, unicorn.UcError,
            subprocess.CalledProcessError) as error:
        print(f'check_lobby: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
