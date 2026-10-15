#!/usr/bin/env python3
"""Compare rebuilt font tables with the original DF1 x86 font scanner."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

import pefile
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pff import Archive
from pcx import decode

SOURCE_HASH = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
FONTS = [('AVA12B', 'ava_12b.pcx'), ('AVA14B', 'ava_14b.pcx'),
         ('AVA10B', 'ava_10b.pcx'), ('LCD10', 'lcd_10.pcx'), ('LCD7', 'lcd_07.pcx')]
CODE_BASE = 0x44b000
STACK = 0x2000000
DATA = 0x3000000
STOP = 0x4000000


def original_table(code, image):
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    machine.mem_map(CODE_BASE, 0x2000)
    machine.mem_write(CODE_BASE, code)
    machine.mem_map(STACK, 0x10000)
    machine.mem_map(DATA, 0x200000)
    machine.mem_map(STOP, 0x1000)
    pixels, table, font = DATA + 0x10000, DATA + 0x1000, DATA
    if len(image.pixels) > 0x1f0000:
        raise ValueError('Font atlas exceeds emulator data limit')
    machine.mem_write(pixels, image.pixels)
    machine.mem_write(font, struct.pack('<8I', pixels, 0, table, image.width, image.height, 0, 0, 0))
    sp = STACK + 0x8000
    machine.mem_write(sp, struct.pack('<II', STOP, font))
    machine.reg_write(UC_X86_REG_ESP, sp)
    machine.emu_start(0x44bac0, STOP, timeout=5000000, count=10000000)
    if machine.reg_read(UC_X86_REG_EIP) != STOP:
        raise RuntimeError('Original font scanner did not return within its instruction limit')
    raw = bytes(machine.mem_read(table, 2048))
    return {'glyphs': [list(struct.unpack_from('<HHBBH', raw, i * 8)) for i in range(256)],
            'baseline': struct.unpack('<H', machine.mem_read(font + 30, 2))[0],
            'glyph_count': machine.mem_read(font + 29, 1)[0]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_exe', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--game-folder', type=Path, help='Defaults to the executable folder')
    args = parser.parse_args()
    try:
        executable = args.game_exe.read_bytes()
        if hashlib.sha256(executable).hexdigest() != SOURCE_HASH:
            raise ValueError('Expected the documented DF1 executable SHA-256')
        pe = pefile.PE(data=executable)
        offset = pe.get_offset_from_rva(CODE_BASE - pe.OPTIONAL_HEADER.ImageBase)
        code = executable[offset:offset + 0x2000]
        pe.close()
        if len(code) != 0x2000:
            raise ValueError('Original font code range is truncated')
        folder = (args.game_folder or args.game_exe.parent).resolve()
        archive = Archive(folder / 'Dfbase.pff')
        results = []
        for name, filename in FONTS:
            matches = [e for e in archive.entries if not e.deleted and e.name.lower() == filename]
            if len(matches) != 1:
                raise ValueError('Expected one original font asset: ' + filename)
            entry = matches[0]
            with archive.path.open('rb') as source:
                source.seek(entry.offset)
                data = source.read(entry.size)
            expected = original_table(code, decode(data))
            process = subprocess.run([str(args.probe.resolve()), 'font', str(folder), name],
                                     capture_output=True, text=True, check=True)
            actual = json.loads(process.stdout)
            if len(actual.get('glyphs', [])) != 256:
                raise ValueError('Probe must return 256 glyph records')
            mismatches = [i for i in range(256) if expected['glyphs'][i] != actual['glyphs'][i]]
            results.append({'font': name, 'filename': filename,
                            'asset_sha256': hashlib.sha256(data).hexdigest(),
                            'glyph_entries': 256, 'mismatches': mismatches,
                            'original_baseline': expected['baseline'], 'rebuilt_baseline': actual['baseline'],
                            'original_glyph_count': expected['glyph_count'],
                            'rebuilt_glyph_count': actual['glyph_count']})
        print(json.dumps({'source_sha256': SOURCE_HASH, 'function': '0x44BAC0', 'helper': '0x44B9A0',
                          'code_range': '0x44B000-0x44CFFF',
                          'code_sha256': hashlib.sha256(code).hexdigest(),
                          'unicorn_version': unicorn.__version__, 'results': results}, indent=2))
        return int(any(r['mismatches'] or r['original_baseline'] != r['rebuilt_baseline'] or
                       r['original_glyph_count'] != r['rebuilt_glyph_count'] for r in results))
    except (OSError, ValueError, RuntimeError, pefile.PEFormatError, unicorn.UcError,
            subprocess.CalledProcessError, KeyError) as error:
        print(f'check_font: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
