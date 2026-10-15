#!/usr/bin/env python3
"""Check the rebuilt input config parser and action map against the original DF1 x86 code.

Replays the original df.cfg defaults/loader path (0x407120, 0x4568e0, 0x4565f0,
0x406640) and the original DFDK.KEY loader (0x407040, 0x49b230, 0x49b580) under
Unicorn with file I/O stubs, then compares every value with the C++ port through
the probe. The static hotkey table at 0x4fb2fc is read back from the same image.
Original game files are only read."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile

import pefile
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_READ_UNMAPPED, UC_HOOK_MEM_WRITE_UNMAPPED
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_ESP,
)

SOURCE_HASH = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
IMAGE = 0x400000
IMAGE_SIZE = 0xa00000
STACK = 0x2000000
STACK_TOP = 0x2008000
RETURN = 0x3fffff0
SCRATCH = 0x3000000
THREAD_DATA = 0x3030000
CONFIG_BLOCK = 0x4ced20
CONFIG_SIZE = 0x5c4
TOKENIZE = 0x4565f0
TOKEN_COUNT_ADDRESS = 0x8e442c
DISPATCH = 0x406640
DEFAULTS = 0x407120
CFG_READ = 0x4568e0
KEY_LOADER = 0x407040
KEY_OPEN = 0x49b010
KEY_CLOSE = 0x49b1a0
KEY_CHECK = 0x499040
KEY_SIZE = 0x499500
KEY_ALLOC = 0x4983b0
SCANF = 0x49c89c
GET_THREAD_DATA = 0x4ba06c
ACTION_TABLE = 0x4e7ddc
ACTION_COUNT = 46
HOTKEY_TABLE = 0x4fb2fc
HOTKEY_STRIDE = 0x2c
HOTKEY_COUNT = 121
CPU_FAMILY_ADDRESS = 0x4f8704

INT_FIELDS = {
    'cd_play': 0x5c, 'menu_track': 0x60, 'game_track': 0x64, 'cd_volume': 0x68,
    'fx_volume': 0x6c, 'dialog_volume': 0x70, 'renderer': 0x04, 'windowed': 0x0c,
    'window_size': 0x18, 'game_res': 0x08, 'max_res': 0x14, 'gamma': 0x10,
    'turbo': 0x5bc, 'flipmouse': 0x1c, 'crosshairs': 0x20, 'mousescale': 0x24,
    'showgun': 0x28, 'picinpic': 0x2c, 'mp_service': 0x148, 'mp_gametype': 0x14c,
    'maxplayers': 0x150, 'dedicated': 0x154, 'com_port': 0x158, 'baud_rate': 0x15c,
    'max_lives': 0x160, 'max_kills': 0x164, 'team_req': 0x168, 'attributes': 0x16c,
    'replay': 0x174, 'time_limit': 0x178, 'koth_limit': 0x17c, 'ammo_respawn': 0x180,
    'timeout': 0x184, 'gpsicon': 0x188, 'showtags': 0x18c, 'contest': 0x190,
    'map_infrared': 0x4c, 'map_iff': 0x50, 'map_AWAC': 0x54, 'preempt_pff': 0x30,
    'whichbrowser': 0x5b8, 'mp_verbose': 0x5c0,
}
STRING_FIELDS = {
    'login_password': (0x74, 32), 'game_password': (0x94, 32), 'squad_password': (0xb4, 48),
    'game_name': (0xe4, 36), 'phone_number': (0x108, 32), 'internet_address': (0x128, 32),
    'country': (0x104, 4), 'red_password': (0xc4, 16), 'blue_password': (0xd4, 16),
}
CONFIG_KEYS = list(INT_FIELDS) + list(STRING_FIELDS)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


class Original:
    def __init__(self, source):
        self.pe = pefile.PE(data=source)
        self._key_bytes = b''
        self.u = Uc(UC_ARCH_X86, UC_MODE_32)
        self.u.mem_map(IMAGE, IMAGE_SIZE)
        self.u.mem_write(IMAGE, self.pe.get_memory_mapped_image())
        self.u.mem_map(STACK, 0x10000)
        self.u.mem_map(SCRATCH, 0x100000)
        self.u.mem_map(RETURN & ~0xfff, 0x1000)
        self.stubbed = []
        self._install_cfg_stubs()
        self._install_key_stubs()

    # ---- shared plumbing ----
    def _return_from(self, address, value):
        def hook(uc, addr, size, user):
            esp = uc.reg_read(UC_X86_REG_ESP)
            ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
            uc.reg_write(UC_X86_REG_ESP, esp + 4)
            uc.reg_write(UC_X86_REG_EAX, value)
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

    def _read_block(self, address, size):
        return bytes(self.u.mem_read(address, size))

    # ---- df.cfg path ----
    def _install_cfg_stubs(self):
        # FUN_00407120 also calls the df.cfg reader and the key loader; both open
        # files, so return early and replay the text line by line instead.
        self._return_from(CFG_READ, 0)
        self.stub_key_loader = True

        def key_loader_stub(uc, addr, size, user):
            if not self.stub_key_loader:
                return
            esp = uc.reg_read(UC_X86_REG_ESP)
            ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
            uc.reg_write(UC_X86_REG_ESP, esp + 4)
            uc.reg_write(UC_X86_REG_EAX, 0)
            uc.reg_write(UC_X86_REG_EIP, ret)

        self.u.hook_add(UC_HOOK_CODE, key_loader_stub, begin=KEY_LOADER, end=KEY_LOADER)

    def load_config(self, text, cpu_family=6):
        # A fresh process starts with a zeroed static tokenizer buffer and record.
        self.u.mem_write(0x8e4040, b'\0' * 0x400)
        self.u.mem_write(TOKEN_COUNT_ADDRESS, b'\0' * 0x80)
        self.u.mem_write(CPU_FAMILY_ADDRESS, struct.pack('<I', cpu_family & 0xffffffff))
        self._call(DEFAULTS, [])
        lines = text.split('\n')
        if lines and lines[-1] == '':
            lines.pop()
        record = SCRATCH + 0x10000
        matched = 0
        names = {name.lower() for name in CONFIG_KEYS}
        for line in lines:
            raw = line.encode('latin1')[:999]
            self.u.mem_write(SCRATCH, raw + b'\0')
            self.u.reg_write(UC_X86_REG_ESI, record)
            self._call(TOKENIZE, [SCRATCH])
            count = struct.unpack('<I', self._read_block(record, 4))[0]
            if count == 0:
                continue
            first = struct.unpack('<I', self._read_block(record + 4, 4))[0]
            key = self._c_string(first)
            if key.startswith(b'/'):
                continue
            if key.decode('latin1').lower() in names:
                matched += 1
            self._call(DISPATCH, [record, 1])
        return matched

    def read_config(self):
        block = self._read_block(CONFIG_BLOCK, CONFIG_SIZE)
        out = {}
        for name, offset in INT_FIELDS.items():
            out[name] = struct.unpack_from('<i', block, offset)[0]
        for name, (offset, size) in STRING_FIELDS.items():
            out[name] = block[offset:offset + size].split(b'\0')[0].decode('latin1')
        return out

    def _c_string(self, address):
        out = b''
        while True:
            byte = self.u.mem_read(address, 1)[0]
            if byte == 0:
                return out
            out += bytes([byte])
            address += 1

    # ---- DFDK.KEY path ----
    def _install_key_stubs(self):
        # FUN_0049b010 opens the file through the game file API; stub its three
        # helpers and let the real CRLF/tab/uppercase preprocessing loop run.
        self._return_from(KEY_CHECK, 1)
        self._return_from(KEY_CLOSE, 1)
        self._return_from(KEY_ALLOC, SCRATCH + 0x80000)
        self._return_from(GET_THREAD_DATA, THREAD_DATA)
        self.u.hook_add(UC_HOOK_CODE, self._read_hook, begin=KEY_SIZE, end=KEY_SIZE)
        self.u.hook_add(UC_HOOK_CODE, self._scanf_hook, begin=SCANF, end=SCANF)
        self.u.hook_add(UC_HOOK_MEM_READ_UNMAPPED, self._fault)
        self.u.hook_add(UC_HOOK_MEM_WRITE_UNMAPPED, self._fault)

    def _read_hook(self, uc, addr, size, user):
        # FUN_00499500(path, 0) returns the size; FUN_00499500(path, buffer)
        # copies the file. The caller pops both arguments itself, so the stub
        # only removes the return address.
        esp = uc.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
        target = struct.unpack('<I', uc.mem_read(esp + 8, 4))[0]
        if target != 0:
            self.u.mem_write(target, self._key_bytes)
        uc.reg_write(UC_X86_REG_ESP, esp + 4)
        uc.reg_write(UC_X86_REG_EAX, len(self._key_bytes) & 0xffffffff)
        uc.reg_write(UC_X86_REG_EIP, ret)

    @staticmethod
    def _fault(uc, access, address, size, value, user):
        raise RuntimeError('Original code touched unmapped memory at %x' % address)

    def _scanf_hook(self, uc, addr, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        src, fmt, out1, out2 = struct.unpack('<4I', uc.mem_read(esp + 4, 16))
        count = self._sscanf(src, fmt, out1, out2)
        ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
        uc.reg_write(UC_X86_REG_ESP, esp + 4)
        uc.reg_write(UC_X86_REG_EAX, count)
        uc.reg_write(UC_X86_REG_EIP, ret)

    def _sscanf(self, src, fmt, out1, out2):
        text = self._c_string(src)
        form = self._c_string(fmt)
        if form == b'[%s':
            if not text.startswith(b'['):
                return 0
            i = 1
            while i < len(text) and text[i] in b' \t\n\r\v\f':
                i += 1
            j = i
            while j < len(text) and text[j] not in b' \t\n\r\v\f':
                j += 1
            if j == i:
                return 0
            self.u.mem_write(out1, text[i:j] + b'\0')
            return 1
        if form == b'[%[ABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789]':
            if not text.startswith(b'['):
                return 0
            chars = b'ABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789'
            i = 1
            j = i
            while j < len(text) and text[j] in chars:
                j += 1
            if j == i:
                return 0
            self.u.mem_write(out1, text[i:j] + b'\0')
            return 1
        if form == b'%[^\n\r=]=%[^\n\r;]':
            j = 0
            while j < len(text) and text[j] not in b'\n\r=':
                j += 1
            if j == 0:
                return 0
            self.u.mem_write(out1, text[:j] + b'\0')
            if j >= len(text) or text[j] != ord('='):
                return 1
            k = j + 1
            while k < len(text) and text[k] not in b'\n\r;':
                k += 1
            if k == j + 1:
                return 1
            self.u.mem_write(out2, text[j + 1:k] + b'\0')
            return 2
        raise RuntimeError('Unexpected format string %r' % form)

    def load_keys(self, data):
        self._key_bytes = bytes(data)
        self.u.mem_write(SCRATCH + 0x80000, b'\0' * (len(data) + 0x100))
        self.u.mem_write(ACTION_TABLE, b'\0' * (ACTION_COUNT * 12))
        self.stub_key_loader = False
        try:
            self._call(KEY_LOADER, [])
        finally:
            self.stub_key_loader = True
        raw = self._read_block(ACTION_TABLE, ACTION_COUNT * 12)
        return [list(struct.unpack_from('<3I', raw, i * 12)) for i in range(ACTION_COUNT)]

    # ---- hotkey table ----
    def hotkeys(self):
        image = self.pe.get_memory_mapped_image()
        rows = []
        for i in range(HOTKEY_COUNT):
            at = HOTKEY_TABLE - IMAGE + i * HOTKEY_STRIDE
            row = image[at:at + HOTKEY_STRIDE]
            identifier = struct.unpack_from('<I', row, 0)[0]
            name = row[4:0x22].split(b'\0')[0].decode('latin1')
            key, mouse, joystick, flags = struct.unpack_from('<4H', row, 0x24)
            rows.append({'id': identifier, 'name': name, 'key': key, 'mouse': mouse,
                         'joystick': joystick, 'flags': flags})
        return rows


def run_probe(probe, game, *args):
    command = [str(probe), str(game)] + list(args)
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError('Probe failed: %s' % result.stderr.strip())
    return json.loads(result.stdout)


def expect(condition, message, failures):
    if not condition:
        failures.append(message)


def compare_config(probe_config, original_config, tag, failures, notes):
    for name in INT_FIELDS:
        if probe_config[name] != original_config[name]:
            failures.append('%s: %s port=%r original=%r' % (tag, name, probe_config[name], original_config[name]))
    # The original copies strings without a length check, so a value longer
    # than its field spills into the next field. The port keeps full strings;
    # compare exactly while values fit and record overflow cases separately.
    overflow = False
    for name, (offset, size) in sorted(STRING_FIELDS.items(), key=lambda item: item[1][0]):
        port = probe_config[name]
        original_field = original_config[name]
        if overflow:
            continue
        if len(port) >= size:
            if not original_field.startswith(port[:size - 1]):
                failures.append('%s: %s overflow port=%r original=%r' % (tag, name, port, original_field))
            overflow = True
            notes.append('%s: %s overflows its %d-byte field' % (tag, name, size))
            continue
        if port != original_field:
            failures.append('%s: %s port=%r original=%r' % (tag, name, port, original_field))


def random_config_text(rng, count):
    lines = ['cd_play = 0\r\n', '// mixed case\n', 'GAMMA = 1\n']
    interesting = ['0', '1', '-1', '255', '256', '2147483648', '4294967296', '99999999999',
                   '"quoted value"', '"two words"', '', '0x20', '12abc', '+7']
    keys = list(CONFIG_KEYS)
    for _ in range(count):
        style = rng.randrange(8)
        if style == 0:
            lines.append('\n')
        elif style == 1:
            lines.append('// comment %d\n' % rng.randrange(1000))
        elif style == 2:
            lines.append('unknown_%d = %d\n' % (rng.randrange(1000), rng.randrange(100)))
        elif style == 3:
            key = 'unknown_%d' % rng.randrange(1000)
            lines.append('%s %d\n' % (key, rng.randrange(100)))
        elif style == 4:
            key = rng.choice(keys)
            value = rng.choice(interesting)
            lines.append('%s\t=\t%s\r\n' % (key, value))
        elif style == 5:
            key = rng.choice(keys)
            lines.append('%s = %d, %d\n' % (key, rng.randrange(-100, 200), rng.randrange(100)))
        elif style == 6:
            key = rng.choice(keys).upper()
            lines.append('%s = %s\n' % (key, rng.choice(interesting)))
        else:
            key = rng.choice(keys)
            lines.append('  %s = %d\r\n' % (key, rng.randrange(-50, 300)))
    return ''.join(lines)


def random_key_text(rng, count):
    lines = ['; random key file\r\n', '[KEYS]\r\n']
    for _ in range(count):
        style = rng.randrange(6)
        if style == 0:
            lines.append('; comment\r\n')
        elif style == 1:
            lines.append('KEY = %d, %d, %d, %d\r\n' % (rng.randrange(ACTION_COUNT), rng.randrange(256),
                                                       rng.randrange(4), rng.randrange(3)))
        elif style == 2:
            lines.append('key=%d,%d,%d,%d\r\n' % (rng.randrange(ACTION_COUNT), rng.randrange(256),
                                                  rng.randrange(4), rng.randrange(4)))
        elif style == 3:
            lines.append('KEY=%d, %d, %d, %d ; trailing comment\r\n' % (rng.randrange(ACTION_COUNT),
                                                                        rng.randrange(256), rng.randrange(4),
                                                                        rng.randrange(4)))
        elif style == 4:
            lines.append('OTHER = 1,2,3,4\r\n')
        else:
            lines.append('KEY\t=\t%d\t,\t%d\t,\t%d\t,\t%d\r\n' % (rng.randrange(ACTION_COUNT), rng.randrange(256),
                                                                  rng.randrange(4), rng.randrange(4)))
    return ''.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--cases', type=int, default=40)
    parser.add_argument('--cpu-family', type=int, default=6)
    parser.add_argument('--seed', type=int, default=1998)
    args = parser.parse_args()

    source = (args.game / 'Df.exe').read_bytes()
    if hashlib.sha256(source).hexdigest() != SOURCE_HASH:
        raise SystemExit('Unsupported original executable hash')
    original = Original(source)
    failures = []
    notes = []
    details = {}

    scratch_root = Path(__file__).parents[1] / '.local' / 'input_config' / 'check-cases'
    try:
        scratch_root.mkdir(parents=True, exist_ok=True)
        scratch = scratch_root
    except OSError:
        scratch = Path(tempfile.mkdtemp(prefix='dfre-input-config-'))

    # 1. Defaults only.
    empty = scratch / 'empty.cfg'
    empty.write_text('')
    result = run_probe(args.probe, args.game, '--config', str(empty), '--cpu-family', str(args.cpu_family))
    original.load_config('', args.cpu_family)
    compare_config(result['config'], original.read_config(), 'defaults', failures, notes)
    expect(result['config_matched'] == 0, 'defaults: matched count is not zero', failures)

    # 2. Installed df.cfg (read-only).
    installed = args.game / 'df.cfg'
    installed_text = installed.read_bytes().decode('latin1') if installed.exists() else ''
    result = run_probe(args.probe, args.game, '--cpu-family', str(args.cpu_family))
    matched = original.load_config(installed_text, args.cpu_family)
    compare_config(result['config'], original.read_config(), 'df.cfg', failures, notes)
    expect(result['config_matched'] == matched,
           'df.cfg: matched %d, original %d' % (result['config_matched'], matched), failures)
    details['df_cfg_keys'] = matched
    details['df_cfg_string_values'] = {name: original.read_config()[name] for name in STRING_FIELDS}

    # 3. Random df.cfg cases.
    rng = random.Random(args.seed)
    cases = [random_config_text(rng, 24) for _ in range(args.cases)]
    cases.append('gamma = 5\ngamma 9\n')
    cases.append('cd_play = 3\ncd_play = 4\r\n\r\n')
    cases.append('login_password   = ""\r\ngame_name = ""\n')
    for index, text in enumerate(cases):
        path = scratch / ('case%03d.cfg' % index)
        path.write_bytes(text.encode('latin1'))
        result = run_probe(args.probe, args.game, '--config', str(path), '--cpu-family', str(args.cpu_family))
        matched = original.load_config(text, args.cpu_family)
        compare_config(result['config'], original.read_config(), 'case%03d' % index, failures, notes)
        expect(result['config_matched'] == matched,
               'case%03d: matched %d, original %d' % (index, result['config_matched'], matched), failures)
    details['df_cfg_cases'] = len(cases)

    # 4. Shipped DFDK.KEY from the archives (AssetStore reads Dfbase.pff first).
    spec = importlib.util.spec_from_file_location('pff', Path(__file__).parent / 'pff.py')
    pff = importlib.util.module_from_spec(spec)
    sys.modules['pff'] = pff
    spec.loader.exec_module(pff)
    key_bytes = None
    for archive_name in ('Dfbase.pff', 'Df.pff'):
        archive = pff.Archive(args.game / archive_name)
        match = [entry for entry in archive.entries if entry.name.lower() == 'dfdk.key' and not entry.deleted]
        if match:
            with open(archive.path, 'rb') as source_file:
                source_file.seek(match[0].offset)
                key_bytes = source_file.read(match[0].size)
            break
    if key_bytes is None:
        raise SystemExit('DFDK.KEY not found in the archives')
    original.load_keys(key_bytes)
    result = run_probe(args.probe, args.game)
    expected_actions = original.load_keys(key_bytes)
    port_actions = [[row['key'], row['mouse'], row['joystick']] for row in result['actions']]
    expect(port_actions == expected_actions, 'shipped DFDK.KEY action table mismatch', failures)
    expect(result['action_load']['applied'] == ACTION_COUNT and result['action_load']['rejected'] == 0,
           'shipped DFDK.KEY applied/rejected count', failures)
    details['shipped_actions'] = expected_actions

    # 6. Random key files (CRLF) and line-ending edge cases.
    key_cases = [random_key_text(rng, 20) for _ in range(args.cases)]
    key_cases.append('; before\r\n[KEYS]\r\nKEY=0, 1, 2, 3\r\n[NEXT]\r\nKEY=1,9,9,9\r\n')
    key_cases.append('\r\n[KEYS]\r\nKEY = 45 , 36 , 0 , 0\r\n')
    key_cases.append('[KEYS]\r\nKEY=1,2,3,4\nKEY=5,6,7,8\n')          # only the first line is readable
    key_cases.append('[KEYS]\nKEY=1,2,3,4\n')                          # no CRLF pair: nothing loads
    key_cases.append('KEY=1,2,3,4\r\n[KEYS]\r\nKEY=2,3,4,5\r\n')       # first section wins
    # A KEY line with fewer than four fields makes the original call atoi(NULL)
    # and crash, so it is not replayed; the port rejects those lines and the
    # native test covers the counting.
    key_cases.append('[KEYS]\r\nKEY=3,4,5,6\r\nKEY=4,5,6,7\r\n')
    for index, text in enumerate(key_cases):
        path = scratch / ('keys%03d.key' % index)
        path.write_bytes(text.encode('latin1'))
        result = run_probe(args.probe, args.game, '--keys', str(path))
        expected = original.load_keys(text.encode('latin1'))
        port = [[row['key'], row['mouse'], row['joystick']] for row in result['actions']]
        expect(port == expected, 'keys case%03d mismatch' % index, failures)
    details['key_cases'] = len(key_cases)

    # 7. Static hotkey table from the executable image.
    result = run_probe(args.probe, args.game)
    expect(result['hotkeys'] == original.hotkeys(), 'hotkey command table mismatch', failures)
    details['hotkeys'] = len(result['hotkeys'])

    output = {
        'source_sha256': SOURCE_HASH,
        'probe': str(args.probe),
        'cpu_family': args.cpu_family,
        'passed': not failures,
        'failures': failures,
        'overflow_notes': notes[:20],
        'details': details,
    }
    text = json.dumps(output, indent=2)
    print(text)
    if args.report:
        args.report.write_text(text + '\n')
    return 1 if failures else 0


if __name__ == '__main__':
    raise SystemExit(main())
