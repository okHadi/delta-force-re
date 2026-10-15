#!/usr/bin/env python3
"""Prepare local runtime data from the supplied DF1 executable."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

SOURCE_HASH = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
BLEND_VA = 0x500d68


def word(data, at, form='<I'):
    size = struct.calcsize(form)
    if at < 0 or at > len(data) - size:
        raise ValueError('Truncated PE field')
    return struct.unpack_from(form, data, at)[0]


def extract_virtual(data, address, count):
    if data[:2] != b'MZ' or count < 0:
        raise ValueError('Expected a PE file and a valid byte count')
    header = word(data, 60)
    if word(data, header) != 0x4550:
        raise ValueError('Invalid PE signature')
    sections = word(data, header + 6, '<H')
    optional = word(data, header + 20, '<H')
    if optional < 32 or word(data, header + 24, '<H') != 0x10b or not 1 <= sections <= 96:
        raise ValueError('Expected a bounded PE32 section table')
    base = word(data, header + 24 + 28)
    rva = address - base
    matches = []
    for index in range(sections):
        section = header + 24 + optional + index * 40
        virtual = word(data, section + 12)
        size = word(data, section + 16)
        offset = word(data, section + 20)
        flags = word(data, section + 36)
        within = rva - virtual
        if not offset or flags & 0x80 or within < 0 or count > size - within:
            continue
        start = offset + within
        if start > len(data) or count > len(data) - start:
            raise ValueError('PE source range exceeds the file')
        matches.append(data[start:start + count])
    if len(matches) != 1:
        raise ValueError('Expected one file-backed address range')
    return matches[0]


def write_local(path, data):
    if path.exists() or path.is_symlink():
        if path.read_bytes() != data:
            raise ValueError(f'Existing output differs: {path}')
        return
    with path.open('xb') as output:
        output.write(data)


def prepare(game_folder, output_folder):
    game, output = game_folder.resolve(), output_folder.resolve()
    # Mac and Windows game folders can use case-insensitive names.
    source_key = str(game).casefold().rstrip('/\\')
    output_key = str(output).casefold()
    if output_key == source_key or output_key.startswith(source_key + '/') or output_key.startswith(source_key + '\\'):
        raise ValueError('Keep runtime output outside the original game folder')
    matches = [p for p in game.iterdir() if p.is_file() and p.name.lower() == 'df.exe']
    if len(matches) != 1:
        raise ValueError('Expected one Df.exe in the supplied game folder')
    source = matches[0].read_bytes()
    if hashlib.sha256(source).hexdigest() != SOURCE_HASH:
        raise ValueError('Expected the documented Df.exe SHA-256')
    blend = extract_virtual(source, BLEND_VA, 1024)
    selectors = struct.unpack('<256I', blend)
    if any(value > 11 for value in selectors):
        raise ValueError('Terrain blend selector exceeds 11')
    report = {'source_sha256': SOURCE_HASH, 'files': [{
        'name': 'detail-blend.u32le', 'source_va': hex(BLEND_VA),
        'bytes': len(blend), 'format': '256 little-endian uint32 selectors',
        'sha256': hashlib.sha256(blend).hexdigest()}]}
    output.mkdir(parents=True, exist_ok=True)
    write_local(output / 'detail-blend.u32le', blend)
    write_local(output / 'runtime-data.json', (json.dumps(report, indent=2) + '\n').encode())
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_folder', type=Path)
    parser.add_argument('output_folder', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(prepare(args.game_folder, args.output_folder), indent=2))
        return 0
    except (OSError, ValueError) as error:
        print(f'prepare_runtime_data: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
