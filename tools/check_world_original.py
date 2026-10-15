#!/usr/bin/env python3
"""Check single-player object placement against original DF1 instructions."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import subprocess

import pefile
from PIL import Image
import unicorn
from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_EBX, UC_X86_REG_EIP, UC_X86_REG_ESP
from mission import Resolver, terrain_name, map_names


def check(game, probe):
    source = (game / 'Df.exe').read_bytes()
    source_hash = hashlib.sha256(source).hexdigest()
    if source_hash != 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff':
        raise ValueError('Unsupported original executable hash')
    pe = pefile.PE(data=source)
    u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    u.mem_map(0x400000, 0xa00000)
    u.mem_write(0x400000, pe.get_memory_mapped_image())
    u.mem_map(0x2000000, 0x10000)
    u.mem_map(0x3000000, 0x100000)
    groups = [(0x7dde80, 0x8cfa1c, 2048), (0x867e80, 0x8cf904, 1024),
              (0x8ace80, 0x8cf900, 512), (0x7cca80, 0x8cfa18, 256)]
    resolver = Resolver(game)
    images = {}

    def read(name):
        return resolver.read(*resolver.find(name))[0]

    def word(address, value):
        u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    def signed(value):
        return value if value < 0x80000000 else value - 0x100000000

    rows = json.loads(subprocess.check_output([str(probe.resolve()), str(game), '--all']))
    if len(rows) != len({row['mission'] for row in rows}):
        raise ValueError('Repeated mission in native placement output')
    expected_names = {e.name for archive in resolver.archives for e in archive.entries
                      if not e.deleted and e.name.lower().endswith('.bms')}
    if {row['mission'] for row in rows} != expected_names:
        raise ValueError('Native placement output does not cover the mission corpus')
    checked = skipped = 0
    counts_by_group = [0] * 4
    for row in rows:
        bms = read(row['mission'])
        height_name = map_names(read(terrain_name(bms)))['elev_map']
        if height_name not in images:
            image = Image.open(io.BytesIO(read(height_name)))
            image.load()
            if image.size != (1024, 1024):
                raise ValueError('Unexpected terrain height size')
            images[height_name] = image.transpose(Image.Transpose.FLIP_TOP_BOTTOM).tobytes()
        u.mem_write(0x3000000, images[height_name])
        word(0xbd5284, 0x3000000)
        counts = struct.unpack_from('<4I', bms, 152)
        offset = 268
        enabled = []
        for group, ((base, count_address, limit), count) in enumerate(zip(groups, counts)):
            if count > limit:
                raise ValueError(f'BMS group exceeds original placement buffer: {row["mission"]} group {group} count {count}')
            word(count_address, count)
            u.mem_write(base, bytes(count * 0x114))
            for index in range(count):
                raw = bms[offset:offset + 96]
                offset += 96
                if struct.unpack_from('<I', raw, 12)[0] & 0x90:
                    skipped += 1
                    continue
                at = base + index * 0x114
                u.mem_write(0x2001000, raw)
                u.reg_write(UC_X86_REG_EBP, 0x2001000)
                u.reg_write(UC_X86_REG_EBX, at)
                u.reg_write(UC_X86_REG_ESP, 0x2008000)
                u.emu_start(0x4497a3, 0x4497ed, count=100)
                if u.reg_read(UC_X86_REG_EIP) != 0x4497ed:
                    raise ValueError('Original object pose conversion did not finish')
                enabled.append((group, index, at, raw))
        u.reg_write(UC_X86_REG_ESP, 0x2008000)
        u.emu_start(0x403c93, 0x403dd4, count=5000000)
        if u.reg_read(UC_X86_REG_EIP) != 0x403dd4:
            raise ValueError('Original object height loop did not finish')
        expected = []
        for group, index, at, raw in enabled:
            values = struct.unpack('<6I', u.mem_read(at + 0x24, 24))
            expected.append(dict(group=group, index=index, object_id=struct.unpack_from('<I', raw, 8)[0],
                                 type_id=struct.unpack_from('<I', raw)[0],
                                 position=[signed(v) for v in values[:3]], angles=list(values[3:])))
            counts_by_group[group] += 1
        if row['objects'] != expected:
            raise ValueError(f'World placement mismatch: {row["mission"]}')
        checked += len(expected)
    return dict(source_sha256=source_hash, missions=len(rows), objects=checked,
                excluded_objects=skipped, group_counts=counts_by_group, mismatches=0,
                functions=['0x4497a3..0x4497ed', '0x403c93..0x403dd4', '0x47df40'],
                scope='single-player pose conversion and complete original height loops; enabled BMS objects in source group order; terrain base height; group 3 extra 65536; six pose words; raw identity fields')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    result = check(args.game, args.probe)
    output = json.dumps(result, indent=2)
    print(output)
    if args.report:
        args.report.write_text(output + '\n')
