#!/usr/bin/env python3
"""Check the ITEMS.DEF to runtime item mapping against original Df.exe x86.

Builds the native test and probe with /usr/bin/clang++, codesigns them, then:

  1. runs tests/native_item_runtime.cpp,
  2. loads ITEMS.DEF from the shipped PFF archives and decodes it with the
     original 0x456EB0 transform under Unicorn; the bytes must match the
     Python port of src/scripts.cpp,
  3. tokenizes every line with the original 0x4565F0 and replays the count
     callback 0x481BB0 and the populate callback 0x481BE0 over all 306
     definitions, then compares every 0x13C record byte with the native probe,
  4. replays edge cases (attrib flags, types, name truncation, hp pairs,
     numeric conversions, ambient slots, duplicate fields) through the same
     original tokenizer and callbacks and compares them byte for byte,
  5. replays the axle_dist x87 block (FLD/FMUL/0x49BAE6/FISTP) from original
     bytes over stock, boundary, negative, subnormal, overflow and non-finite
     doubles and compares each result with the native mapping.

All instructions execute from the original image. Unicorn needs the x87
control word at its default 0x037F, and the 0x49CAAB overflow path writes the
CRT errno through the 0xD0A56C pointer, so the machine initializes both. No
CRT call is stubbed. The original executable is read-only; the hash is
verified first.

Run with the venv python that provides unicorn and pefile:

  .local/tools/python/bin/python3 tools/check_item_runtime_original.py \
      "/Users/khanh/Desktop/workspace/Delta Force" [--report FILE]
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
from unicorn import UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import (
    UC_X86_REG_EAX,
    UC_X86_REG_EIP,
    UC_X86_REG_ESP,
    UC_X86_REG_ESI,
    UC_X86_REG_FPCW,
)

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pff import Archive  # noqa: E402

SOURCE_SHA = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
IMAGE = 0x400000
IMAGE_SIZE = 0xA00000
STACK = 0x2000000
STOP = 0x2100000
SCRATCH = 0x3000000
SCRATCH_SIZE = 0x200000

DECODE = 0x456EB0          # (buffer, size, key) reverse + XOR from the runner 0x456720
TOKENIZE = 0x4565F0        # line -> count + 30 token pointers (output pointer in ESI)
COUNT_CB = 0x481BB0        # first pass callback: counts beginnings
POPULATE_CB = 0x481BE0     # second pass callback: fills records
FRNDINT = 0x49BAE6         # truncating FRNDINT helper used by the axle_dist branch
DECODE_KEY = 0x04960552    # DAT_00503228, set by the 0x4010D2 immediate
CRT_ERRNO_POINTER = 0xD0A56C  # CRT location pointer written by 49CAAB's overflow path
FPCW_DEFAULT = 0x037F      # MSVC startup control word: extended precision, round nearest

RECORD_SIZE = 0x13C
EXPECTED_ITEMS = 306
TOKENIZER_RECORD = SCRATCH + 0x40000
LINE_BUFFER = SCRATCH + 0x50000
TABLE = SCRATCH + 0x80000
DECODE_BUFFER = SCRATCH + 0x100000
ERRNO_SCRATCH = SCRATCH + 0x1000
NUMERIC_CODE = SCRATCH + 0x18000
NUMERIC_IN = SCRATCH + 0x18100
NUMERIC_FACTOR = SCRATCH + 0x18110
NUMERIC_OUT = SCRATCH + 0x18120

CONSTANTS_EXPECTED = {
    'record_size': 0x13C,
    'name_max': 30,
    'id_bias': 100000,
    'ambient_slots': 4,
    'ambient_size': 12,
    'offsets': {
        'name': 0x00, 'id': 0x20, 'flags': 0x24, 'type': 0x28, 'graphic': 0x2C, 'husk': 0x3C,
        'chr_file': 0x4C, 'anim_def': 0x5C, 'model': 0x6C, 'husk_model': 0x70, 'character': 0x74,
        'animation': 0x78, 'ai_function': 0x7C, 'ai_data': 0x84, 'move_function': 0x88,
        'move_data': 0x90, 'hp': 0x94, 'hp_secondary': 0x98, 'effect_count': 0x9C,
        'phrase_set': 0xD4, 'face_set': 0xD8, 'acceleration': 0xDC, 'turn_rate': 0xE0,
        'axle_dist': 0xE4, 'ambient': 0xE8, 'ambient_sound': 0x128, 'interrupt_sound_id': 0x138,
    },
    'flags': {'good': 1, 'evil': 2, 'aidata': 4, 'fo': 8, 'movecb': 0x10,
              'powerup': 0x20, 'flesh': 0x40, 'noak': 0x80},
    'types': {'vehicle': 1, 'decoration': 2, 'person': 3, 'marker': 4, 'building': 5,
              'object': 6, 'effect': 8},
}


def decode_python(raw):
    """src/scripts.cpp port of 0x456EB0 for cross-checking the emulated decode."""
    out = bytearray(raw[::-1])
    key = DECODE_KEY
    for i, byte in enumerate(out):
        key = (key + (((key << 11) | (key >> 21)) & 0xFFFFFFFF)) & 0xFFFFFFFF
        key = ((((key << 4) | (key >> 28)) & 0xFFFFFFFF) ^ 1) & 0xFFFFFFFF
        out[i] = byte ^ (key & 0xFF)
    return bytes(out)


def split_lines(data):
    """Line scan of runner 0x456720: CRLF ends a line; a tail without CRLF
    loses its last byte (0x45689D path)."""
    lines = []
    start = 0
    while start < len(data):
        end = data.find(b'\r\n', start)
        if end < 0:
            lines.append(data[start:len(data) - 1])
            break
        lines.append(data[start:end])
        start = end + 2
    return lines


def script(*lines):
    return b''.join(line.encode('latin1') + b'\r\n' for line in lines)


class Original:
    """Runs original Df.exe fragments (tokenizer, callbacks, x87) under Unicorn."""

    def __init__(self, game):
        source = (game / 'Df.exe').read_bytes()
        if hashlib.sha256(source).hexdigest() != SOURCE_SHA:
            raise ValueError('Unsupported original executable hash')
        self.game = game
        self.image = pefile.PE(data=source, fast_load=True).get_memory_mapped_image()
        self.archives = [Archive(game / name) for name in ('Df.pff', 'Dfbase.pff')]

    def read(self, name):
        for archive in self.archives:
            for entry in archive.entries:
                if not entry.deleted and entry.name.lower() == name.lower():
                    with archive.path.open('rb') as source:
                        source.seek(entry.offset)
                        return archive.path.name, source.read(entry.size)
        raise ValueError(f'missing archive member {name}')

    # -- one emulated machine with the original tokenizer and callbacks ------

    class Machine:
        def __init__(self, image):
            u = unicorn.Uc(UC_ARCH_X86, UC_MODE_32)
            u.mem_map(IMAGE, IMAGE_SIZE)
            u.mem_write(IMAGE, image)
            u.mem_map(STACK, 0x40000)
            u.mem_map(SCRATCH, SCRATCH_SIZE)
            u.mem_write(LINE_BUFFER, bytes(0x400))
            # 49CAAB's overflow path stores errno through the CRT location
            # pointer; the game startup sets it before any file load.
            u.mem_write(CRT_ERRNO_POINTER, struct.pack('<I', ERRNO_SCRATCH))
            self.u = u
            self._build_numeric_block()

        # -- small memory helpers --
        def r8(self, at):
            return self.u.mem_read(at, 1)[0]

        def r32(self, at):
            return struct.unpack('<I', self.u.mem_read(at, 4))[0]

        def w32(self, at, value):
            self.u.mem_write(at, struct.pack('<I', value & 0xFFFFFFFF))

        def cstr(self, at):
            out = bytearray()
            while True:
                byte = self.r8(at)
                if byte == 0:
                    return bytes(out)
                out.append(byte)
                at += 1

        def call(self, entry, args=(), esi=None):
            sp = STACK + 0x20000
            for arg in reversed(args):
                sp -= 4
                self.w32(sp, arg)
            sp -= 4
            self.w32(sp, STOP)
            self.u.reg_write(UC_X86_REG_ESP, sp)
            self.u.reg_write(UC_X86_REG_FPCW, FPCW_DEFAULT)
            if esi is not None:
                self.u.reg_write(UC_X86_REG_ESI, esi)
            try:
                self.u.emu_start(entry, STOP, count=4_000_000)
            except unicorn.UcError as error:
                eip = self.u.reg_read(UC_X86_REG_EIP)
                try:
                    opcode = bytes(self.u.mem_read(eip, 16)).hex()
                except unicorn.UcError:
                    opcode = 'unreadable'
                raise ValueError(f'original 0x{entry:x} faulted: {error} '
                                 f'at 0x{eip:x} opcode {opcode}') from error
            if self.u.reg_read(UC_X86_REG_EIP) != STOP:
                raise ValueError(f'original 0x{entry:x} did not return')
            return self.u.reg_read(UC_X86_REG_EAX)

        def decode(self, raw):
            self.u.mem_write(DECODE_BUFFER, raw)
            self.call(DECODE, args=(DECODE_BUFFER, len(raw), DECODE_KEY))
            return bytes(self.u.mem_read(DECODE_BUFFER, len(raw)))

        def tokenize(self, line):
            self.u.mem_write(LINE_BUFFER, line[:0x3E8] + b'\0')
            self.call(TOKENIZE, args=(LINE_BUFFER,), esi=TOKENIZER_RECORD)
            count = self.r32(TOKENIZER_RECORD)
            return count, [self.cstr(self.r32(TOKENIZER_RECORD + 4 + 4 * i)) for i in range(count)]

        def _lines(self, lines):
            """Runs the runner's dispatch: tokenize, skip empty and '/' lines."""
            for number, line in enumerate(lines, start=1):
                count, _ = self.tokenize(line)
                if count == 0:
                    continue
                if self.r8(self.r32(TOKENIZER_RECORD + 4)) == 0x2F:  # '/'
                    continue
                yield number

        def count_pass(self, lines):
            self.w32(0xC23818, 0)
            for number in self._lines(lines):
                self.call(COUNT_CB, args=(TOKENIZER_RECORD, number))
            self.call(COUNT_CB, args=(0, len(lines) + 1))
            return self.r32(0xC23818)

        def populate_pass(self, lines, count):
            self.u.mem_write(TABLE, bytes(count * RECORD_SIZE))
            self.w32(0xC23810, TABLE)
            self.w32(0xC23814, count)
            self.w32(0xC23818, 0)
            self.w32(0xC2381C, TABLE)
            for number in self._lines(lines):
                self.call(POPULATE_CB, args=(TOKENIZER_RECORD, number))
            self.call(POPULATE_CB, args=(0, len(lines) + 1))
            return self.r32(0xC23818), bytes(self.u.mem_read(TABLE, count * RECORD_SIZE))

        def run_text(self, raw):
            """decode-less replay: the tests feed decoded text directly."""
            lines = split_lines(raw)
            count = self.count_pass(lines)
            end_count, records = self.populate_pass(lines, count)
            return count, end_count, records

        # -- axle_dist x87 block: original FLD/FMUL/49BAE6/FISTP bytes ------
        def _build_numeric_block(self):
            program = bytes.fromhex('dd05') + struct.pack('<I', NUMERIC_IN)       # fld qword [in]
            program += bytes.fromhex('dc0d') + struct.pack('<I', NUMERIC_FACTOR)  # fmul qword [65536.0]
            relative = (FRNDINT - (NUMERIC_CODE + len(program) + 5)) & 0xFFFFFFFF
            program += b'\xe8' + struct.pack('<I', relative)                      # call 49BAE6
            program += bytes.fromhex('db1d') + struct.pack('<I', NUMERIC_OUT)     # fistp dword [out]
            program += b'\xc3'
            self.u.mem_write(NUMERIC_CODE, program)
            self.u.mem_write(NUMERIC_FACTOR, struct.pack('<d', 65536.0))

        def numeric_block(self, value_bits):
            self.u.mem_write(NUMERIC_OUT, b'\x00' * 4)
            self.u.mem_write(NUMERIC_IN, struct.pack('<Q', value_bits))
            self.call(NUMERIC_CODE)
            return struct.unpack('<i', self.u.mem_read(NUMERIC_OUT, 4))[0]

    def machine(self):
        return self.Machine(self.image)


def probe_items(probe, text):
    result = subprocess.run([str(probe), 'items'], input=text, stdout=subprocess.PIPE, check=True)
    return json.loads(result.stdout.decode())


def record_mismatches(scope, native_records, original_records, limit=10):
    """Returns (total mismatching records, sampled details)."""
    if len(native_records) != len(original_records):
        return 1, [{'scope': scope, 'key': 'record_count',
                    'native': len(native_records), 'original': len(original_records)}]
    details = []
    total = 0
    for index, (native_hex, original_bytes) in enumerate(zip(native_records, original_records)):
        native_bytes = bytes.fromhex(native_hex)
        if native_bytes == original_bytes:
            continue
        total += 1
        if len(details) < limit:
            for offset in range(RECORD_SIZE):
                if native_bytes[offset] != original_bytes[offset]:
                    details.append({'scope': scope, 'record': index, 'offset': offset,
                                    'native': native_bytes[offset], 'original': original_bytes[offset]})
                    break
    return total, details


def build(source_root, out_dir):
    clang = Path('/usr/bin/clang++')
    probe = out_dir / 'item_runtime_probe'
    test = out_dir / 'native_item_runtime'
    sources = [str(source_root / 'src' / 'item_runtime.cpp'),
               str(source_root / 'src' / 'items.cpp'),
               str(source_root / 'src' / 'scripts.cpp'),
               str(source_root / 'src' / 'assets.cpp')]
    subprocess.run([str(clang), '-std=c++17', '-O2', '-I', str(source_root / 'include'),
                    *sources, str(source_root / 'tools' / 'item_runtime_probe.cpp'),
                    '-o', str(probe)], check=True)
    subprocess.run([str(clang), '-std=c++17', '-O2', '-I', str(source_root / 'include'),
                    *sources, str(source_root / 'tests' / 'native_item_runtime.cpp'),
                    '-o', str(test)], check=True)
    for binary in (probe, test):
        subprocess.run(['codesign', '--force', '--sign', '-', str(binary)], check=True)
    return probe, test


def corpus_cases():
    """Everything the original callbacks must agree on, in script order."""
    cases = []
    cases.append(('minimal', script(
        'begin "null"', 'id 100000', 'type marker', 'end')))
    cases.append(('name-30', script(
        'begin "' + 'a' * 30 + '"', 'id 100001', 'type object', 'end')))
    cases.append(('name-31', script(
        'begin "' + 'b' * 31 + '"', 'id 100002', 'type object', 'end')))
    cases.append(('name-space-case', script(
        'begin "Peru palm1"', 'id 100003', 'type decoration', 'end')))
    cases.append(('id-wrap', script(
        'begin "low"', 'id 099999', 'type object', 'end')))
    cases.append(('id-biased', script(
        'begin "biased"', 'id 103034', 'type object', 'end')))
    cases.append(('types', script(
        'begin "t1"', 'id 100010', 'type person', 'end',
        'begin "t2"', 'id 100011', 'type vehicle', 'end',
        'begin "t3"', 'id 100012', 'type building', 'end',
        'begin "t4"', 'id 100013', 'type decoration', 'end',
        'begin "t5"', 'id 100014', 'type marker', 'end',
        'begin "t6"', 'id 100015', 'type object', 'end',
        'begin "t7"', 'id 100016', 'type effect', 'end',
        'begin "t8"', 'id 100017', 'type unknown', 'end',
        'begin "t9"', 'id 100018', 'type OBJECT', 'end')))
    cases.append(('attrib-all', script(
        'begin "flags"', 'id 100020', 'type marker',
        'attrib: good evil aidata fo movecb powerup flesh NoAK', 'end')))
    cases.append(('attrib-case', script(
        'begin "flags"', 'id 100021', 'type marker', 'attrib: AiDaTa noak', 'end')))
    cases.append(('attrib-unknown', script(
        'begin "flags"', 'id 100022', 'type marker', 'attrib: other thing', 'end')))
    cases.append(('attrib-lines', script(
        'begin "flags"', 'id 100023', 'type marker', 'attrib: good',
        'attrib: evil unknown aidata', 'end')))
    cases.append(('hp-forms', script(
        'begin "one"', 'id 100030', 'type person', 'hp 40', 'end',
        'begin "pair"', 'id 100031', 'type person', 'hp 40 2', 'end',
        'begin "keep"', 'id 100032', 'type person', 'hp 40 2', 'hp 7', 'end')))
    cases.append(('strings', script(
        'begin "names"', 'id 100040', 'type person',
        'graphic BanA2', 'husk Banax', 'chr_file A_boom', 'anim_def AniWalk',
        'ai_function TREE', 'move_function GnRc', 'end')))
    cases.append(('graphic-shrink', script(
        'begin "shrink"', 'id 100041', 'type object', 'graphic abcdefgh', 'graphic xy', 'end')))
    cases.append(('graphic-overflow', script(
        'begin "spill"', 'id 100042', 'type object',
        'graphic abcdefghijklmnopqrst', 'husk husk', 'end')))
    cases.append(('numerics', script(
        'begin "motion1"', 'id 100050', 'type vehicle',
        'acceleration 3', 'turn_rate 90', 'axle_dist 2.5', 'end',
        'begin "motion2"', 'id 100051', 'type vehicle',
        'axle_dist -0.5', 'turn_rate -90', 'axle_dist 0.25', 'end',
        'begin "motion3"', 'id 100052', 'type vehicle', 'axle_dist junk', 'end',
        # exact x87 boundary: 2147483647/65536 fits, 32768 overflows to the
        # integer indefinite value 0x80000000 (both signs stored alike)
        'begin "motion4"', 'id 100053', 'type vehicle',
        'axle_dist 32767.9999847412109375', 'end',
        'begin "motion5"', 'id 100054', 'type vehicle', 'axle_dist 32768', 'end',
        'begin "motion6"', 'id 100055', 'type vehicle', 'axle_dist -32768', 'end',
        'begin "motion7"', 'id 100056', 'type vehicle', 'axle_dist -32768.5', 'end')))
    cases.append(('axle-parse', script(
        # 0x49CAAB grammar: decimal only, no inf/nan/hex; overflow gives inf
        'begin "p1"', 'id 100057', 'type vehicle', 'axle_dist 1e999', 'end',
        'begin "p2"', 'id 100058', 'type vehicle', 'axle_dist -1e999', 'end',
        'begin "p3"', 'id 100059', 'type vehicle', 'axle_dist 1e-999', 'end',
        'begin "p4"', 'id 100060', 'type vehicle', 'axle_dist .5', 'end',
        'begin "p5"', 'id 100061', 'type vehicle', 'axle_dist 5.', 'end',
        'begin "p6"', 'id 100062', 'type vehicle', 'axle_dist 2.3e0', 'end',
        'begin "p7"', 'id 100063', 'type vehicle', 'axle_dist inf', 'end',
        'begin "p8"', 'id 100064', 'type vehicle', 'axle_dist nan', 'end',
        'begin "p9"', 'id 100065', 'type vehicle', 'axle_dist 0x10', 'end',
        'begin "p10"', 'id 100066', 'type vehicle', 'axle_dist 1e', 'end')))
    cases.append(('ambient', script(
        'begin "sounds"', 'id 100070', 'type object',
        'ambient tone1 10', 'ambient tone2 20', 'ambient tone3 30', 'ambient tone4 40',
        'ambient tone5 50', 'end')))
    cases.append(('interrupt', script(
        'begin "butt"', 'id 100071', 'type object', 'ambient tone1 10',
        'interrupt_sound_id 7', 'end')))
    cases.append(('comments-keys', script(
        '// comment', 'BEGIN "Case"', '  id 100072', '', '  type OBJECT', 'END')))
    cases.append(('phrase-face', script(
        'begin "sets"', 'id 100080', 'type person', 'phrase_set 3', 'face_set 9', 'end')))
    return cases


def numeric_cases(stock_values):
    """(double value, script text or None) for the axle x87 block.

    Finite values use the Python repr, which round-trips to the same double;
    the stock values keep their ITEMS.DEF spelling. +inf and -inf come from
    overflowing decimal exponents (the strings the original parser accepts);
    NaN has no ITEMS.DEF spelling, so it is checked on the original side only
    and listed as a gap.
    """
    cases = [(float(text), text) for text in stock_values]
    for value in (2.3, 0.25, -0.25, -0.5, -2.3, 1.0 / 65536.0,
                  0.0, -0.0, 5e-324, 2.2250738585072014e-308,
                  32767.0, 32767.5, 32767.9999847412109375, 32768.0, 32768.5, 65536.0,
                  -32767.9999847412109375, -32768.0, -32768.5, -65536.0,
                  1e300, -1e300):
        cases.append((value, repr(value)))
    for step in (1, 3, 7, 11, 100, 1234, 30000):
        cases.append((step + 0.5, repr(step + 0.5)))
        cases.append((-(step + 0.5), repr(-(step + 0.5))))
    cases.append((float('inf'), '1e999'))
    cases.append((float('-inf'), '-1e999'))
    cases.append((float('nan'), None))
    return cases


def stock_axle_values(decoded):
    """The axle_dist spellings present in ITEMS.DEF, deduplicated."""
    values = []
    for line in split_lines(decoded):
        parts = line.split()
        if len(parts) >= 2 and parts[0].lower() == b'axle_dist':
            text = parts[1].decode('latin1')
            if text not in values:
                values.append(text)
    return values


def axle_from_probe(probe, text):
    native = probe_items(probe, script('begin "axle"', 'id 100000', 'type vehicle',
                                       'axle_dist ' + text, 'end'))
    record = bytes.fromhex(native['records'][0])
    return struct.unpack_from('<i', record, 0xE4)[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()

    source_root = Path(__file__).resolve().parent.parent
    out_dir = source_root / '.local' / 'opencode' / 'item_runtime'
    out_dir.mkdir(parents=True, exist_ok=True)
    probe, test = build(source_root, out_dir)
    test_run = subprocess.run([str(test)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if test_run.returncode != 0 or b'item runtime checks passed' not in test_run.stdout:
        raise SystemExit('native item runtime test failed: ' + test_run.stderr.decode())

    original = Original(args.game)
    archive_name, raw = original.read('ITEMS.DEF')
    machine = original.machine()

    failures = []
    mismatches = []
    mismatch_total = 0

    emulated_decoded = machine.decode(raw)
    if emulated_decoded != decode_python(raw):
        failures.append('0x456EB0 decode differs from the src/scripts.cpp port')

    constants = json.loads(subprocess.run([str(probe), 'constants'], stdout=subprocess.PIPE,
                                          check=True).stdout.decode())
    if constants != CONSTANTS_EXPECTED:
        failures.append('native constants differ from the original layout evidence')

    # -- the shipped 306 definitions -------------------------------------
    count = end_count = 0
    records = []
    try:
        lines = split_lines(emulated_decoded)
        count = machine.count_pass(lines)
        end_count, original_records = machine.populate_pass(lines, count) if count else (0, b'')
        if count != EXPECTED_ITEMS:
            failures.append(f'count callback returned {count}, expected {EXPECTED_ITEMS}')
        if end_count != count:
            failures.append(f'populate pass advanced {end_count} records for {count} definitions')
        records = [original_records[i * RECORD_SIZE:(i + 1) * RECORD_SIZE] for i in range(count)]
        empty_names = sum(1 for record in records if record[0] == 0)
        if empty_names:
            failures.append(f'{empty_names} corpus records have an empty name')
    except ValueError as error:
        failures.append(str(error))

    native = probe_items(probe, emulated_decoded)
    if native['count'] != count:
        failures.append(f'native probe returned {native["count"]} records, expected {count}')
    total, details = record_mismatches('corpus', native['records'], records)
    mismatch_total += total
    mismatches.extend(details)

    # -- edge cases -------------------------------------------------------
    edge_report = []
    for name, text in corpus_cases():
        try:
            case_machine = original.machine()
            case_count, case_end, case_records = case_machine.run_text(text)
        except ValueError as error:
            failures.append(f'{name}: {error}')
            edge_report.append({'name': name, 'match': False, 'error': str(error)})
            continue
        case_native = probe_items(probe, text)
        case_list = [case_records[i * RECORD_SIZE:(i + 1) * RECORD_SIZE] for i in range(case_count)]
        case_failures = []
        if case_count != case_native['count']:
            case_failures.append(f'count {case_count} != native {case_native["count"]}')
        if case_end != case_count:
            case_failures.append(f'populate advanced {case_end} of {case_count}')
        case_total, case_details = record_mismatches(name, case_native['records'], case_list)
        mismatch_total += case_total
        edge_report.append({'name': name, 'definitions': case_count, 'records': len(case_list),
                            'match': not case_failures and case_total == 0})
        failures.extend(f'{name}: {message}' for message in case_failures)
        mismatches.extend(case_details)

    # -- axle_dist x87 block over original bytes --------------------------
    stock_values = stock_axle_values(emulated_decoded)
    if not stock_values:
        failures.append('no stock axle_dist values found in ITEMS.DEF')
    numeric = {'cases': 0, 'compared': 0, 'mismatches': 0, 'samples': [],
               'stock_values': stock_values,
               'original_only': ['nan: no ITEMS.DEF spelling; script text cannot reach it']}
    for value, text in numeric_cases(stock_values):
        numeric['cases'] += 1
        bits = struct.unpack('<Q', struct.pack('<d', value))[0]
        try:
            wanted = machine.numeric_block(bits)
        except ValueError as error:
            failures.append(f'numeric block {value!r}: {error}')
            continue
        if text is None:
            if wanted != -2147483648:
                failures.append(f'numeric block nan returned {wanted}, expected indefinite')
            continue
        native_value = axle_from_probe(probe, text)
        if native_value != wanted:
            numeric['mismatches'] += 1
            mismatch_total += 1
            if len(numeric['samples']) < 5:
                numeric['samples'].append({'value': text, 'original': wanted, 'native': native_value})
        numeric['compared'] += 1
    if numeric['mismatches']:
        failures.append(f'axle x87 block mismatches: {numeric["mismatches"]}')

    report = {
        'source_sha256': SOURCE_SHA,
        'items_def': {'archive': archive_name, 'size': len(raw),
                      'sha256': hashlib.sha256(raw).hexdigest()},
        'corpus': {'definitions': count, 'count_pass': count, 'populate_pass': end_count,
                   'records': len(records), 'bytes': len(records) * RECORD_SIZE,
                   'decode_matches_python': emulated_decoded == decode_python(raw)},
        'edge_cases': edge_report,
        'numeric_block': numeric,
        'native_test': 'passed',
        'original_instructions': 'tokenizer 0x4565F0, callbacks 0x481BB0/0x481BE0, atof 0x49CAAB, '
                                 'FRNDINT helper 0x49BAE6 and FISTP run from the original image; '
                                 'FPCW is set to 0x037F and the CRT errno pointer at 0xD0A56C is '
                                 'initialized like game startup',
        'hooks': [],
        'gaps': ['IEEE NaN cannot arise from ITEMS.DEF parsing; the numeric block checks it on the '
                 'original side only'],
        'scope': 'ITEMS.DEF decode (0x456EB0), tokenizer 0x4565F0, count callback 0x481BB0 and '
                 'populate callback 0x481BE0 over the shipped 306 definitions and listed edge '
                 'cases; every 0x13C record compared byte for byte with the native mapping, plus '
                 'an independent x87 axle_dist block',
        'failures': failures,
        'mismatches': mismatches[:10],
        'mismatch_count': mismatch_total,
    }
    text = json.dumps(report, indent=2)
    print(text)
    if args.report:
        args.report.write_text(text + '\n')
    return int(bool(failures or mismatch_total))


if __name__ == '__main__':
    sys.exit(main())
