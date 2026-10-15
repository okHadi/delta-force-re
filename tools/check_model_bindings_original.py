#!/usr/bin/env python3
"""Compare model texture and material preparation with original DF1 x86.

The original path is original 485E30 material writes, 480600 bank palette install,
and 486880 texture remap plus flat and effect bindings. Texture records follow the
loader layout at 0xc54488. Several models share one bank so later spans start at a
nonzero global texture index. The 485BA0 share cache is off in the shipped build
(0xc54484 is zero with no writer), so equal names stay separate records.
"""
import argparse
import hashlib
import json
import math
import random
import struct
import subprocess
import sys
from pathlib import Path

import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX

SOURCE_HASH = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
MATERIAL_SIZE = 76
SHARE_FLAG = 0x80


def dimension_bits(dimension):
    value = dimension if dimension < 0x8000 else dimension - 0x10000
    bits = 0
    for bit in range(16):
        if value >= 1 << bit:
            bits = bit
    return bits


def gamma_variant(index):
    if index % 5 == 0:
        return list(range(256))
    if index % 5 == 1:
        return [i * i // 255 for i in range(256)]
    if index % 5 == 2:
        return [min(255, int(math.pow(i / 255.0, 0.8) * 255.0 + 0.5)) for i in range(256)]
    if index % 5 == 3:
        return [(i // 16) * 16 for i in range(256)]
    return [255 - i for i in range(256)]


def palette_variant(rng, index):
    if index % 4 == 0:
        return [rng.randrange(256) for _ in range(768)]
    if index % 4 == 1:
        return [(rng.randrange(256) // 16) * 16 for _ in range(768)]
    if index % 4 == 2:
        value = rng.randrange(256)
        return [value] * 768
    return [(i * 7 + index) % 256 for i in range(768)]


def make_material(rng, flags, tick, flats, keys):
    raw = bytearray(rng.randrange(256) for _ in range(MATERIAL_SIZE))
    struct.pack_into('<I', raw, 16, flags)
    struct.pack_into('<H', raw, 0x1c, tick & 0xffff)
    raw[0x1e] = rng.randrange(1, 32)
    raw[0x1f] = rng.randrange(8)
    for index, colour in enumerate(flats):
        raw[40 + index * 3:43 + index * 3] = bytes(colour)
    raw[52:56] = bytes(keys)
    return bytes(raw)


def make_case(rng, index):
    level = index % 4
    gamma = gamma_variant(index)
    base = palette_variant(rng, index)
    bank = index % 6
    models = []
    for model_index in range(1 + index % 3):
        textures = []
        for i in range(1 + index % 3):
            flags = rng.choice([1, 2, 4, 8, 15, 0x11, 0x21, 0x2d, 0x31, 0x3f])
            sequence = rng.choice([rng.randrange(256), rng.randrange(256, 512), 0x8000 + rng.randrange(4)])
            width, height = rng.choice([(8, 8), (16, 8), (32, 32), (64, 16), (128, 128)])
            pixels = [rng.randrange(256) for _ in range(rng.randrange(1, 40))]
            textures.append(dict(name=f'tex{model_index}_{i}', flags=flags, sequence=sequence,
                                 width=width, height=height, pixels=pixels,
                                 palette=[rng.randrange(256) for _ in range(768)]))
        textures[0]['flags'] |= 1 << level
        # Duplicate an earlier model name and size: the cache is off, so records stay separate.
        if model_index and index % 4 == 1:
            textures[0]['name'] = models[0]['textures'][0]['name']
            textures[0]['pixels'] = [value ^ 0x5a for value in textures[0]['pixels']]
        selected = [texture for texture in textures if texture['flags'] & (1 << level)]
        lods = []
        for lod in range(1 + index % 3):
            materials = []
            for m in range(1 + index % 5):
                flags = rng.choice([0, 1, 0x8000, 0x8001, 0x4000, 0x4001, 0x40, 0x41, 0x840, 0x8401, 0x200, 0x201])
                tick = rng.randrange(0, 256) if flags & 0x4000 else rng.randrange(0, 0x10000)
                flats = [[rng.randrange(256) for _ in range(3)] for _ in range(4)]
                keys = [rng.randrange(256) for _ in range(4)]
                if selected and flags & 1 and (m + lod) % 2 == 0:
                    keys[level] = selected[rng.randrange(len(selected))]['sequence'] & 0xff
                materials.append(make_material(rng, flags, tick, flats, keys))
            lods.append(materials)
        models.append(dict(textures=textures, lods=lods))
    selected_count = sum(len([t for t in model['textures'] if t['flags'] & (1 << level)]) for model in models)
    material_count = sum(sum(len(lod) for lod in model['lods']) for model in models)
    assert selected_count > 0 and material_count > 0
    return dict(level=level, gamma=gamma, base=base, bank=bank, models=models)


def check(executable, probe, count):
    source = Path(executable).read_bytes()
    digest = hashlib.sha256(source).hexdigest()
    if digest != SOURCE_HASH:
        raise ValueError('Unsupported original executable hash')
    u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    u.mem_map(0x400000, 0x1000000)
    u.mem_write(0x400000, pefile.PE(data=source).get_memory_mapped_image())
    u.mem_map(0x2000000, 0x20000)
    u.mem_map(0x3000000, 0x800000)
    u.mem_map(0x6000000, 4096)

    def word(address, value):
        u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    state = {'reads': [], 'heap': 0x3400000}

    def call(address, *values):
        u.mem_write(0x200f000, struct.pack('<' + 'I' * (len(values) + 1), 0x6000000, *values))
        u.reg_write(UC_X86_REG_ESP, 0x200f000)
        u.emu_start(address, 0x6000000, count=500000000)
        if u.reg_read(UC_X86_REG_EIP) != 0x6000000:
            raise ValueError(f'original call did not return: {address:x}')
        return u.reg_read(UC_X86_REG_EAX)

    def hook_read(uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
        dst, length = struct.unpack('<II', uc.mem_read(esp + 4, 8))
        if not state['reads']:
            raise ValueError('original read without queued data')
        data = state['reads'].pop(0)
        if length != len(data):
            raise ValueError(f'original read size {length} differs from {len(data)}')
        uc.mem_write(dst, data)
        uc.reg_write(UC_X86_REG_ESP, esp + 4)
        uc.reg_write(UC_X86_REG_EIP, ret)

    def hook_alloc(uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
        wanted, align = struct.unpack('<II', uc.mem_read(esp + 4, 8))
        pointer = (state['heap'] + align - 1) & ~(align - 1)
        state['heap'] = pointer + wanted
        if state['heap'] >= 0x3800000:
            raise ValueError('original allocation heap exhausted')
        uc.reg_write(UC_X86_REG_EAX, pointer)
        uc.reg_write(UC_X86_REG_ESP, esp + 4)
        uc.reg_write(UC_X86_REG_EIP, ret)

    u.hook_add(unicorn.UC_HOOK_CODE, hook_read, None, 0x499280, 0x499280)
    u.hook_add(unicorn.UC_HOOK_CODE, hook_alloc, None, 0x497f60, 0x497f60)

    rng = random.Random(0x486880)
    cases = [make_case(rng, index) for index in range(count)]
    payload = []
    for case in cases:
        matching = [case['gamma'][value] for value in case['base']]  # 480600 result table.
        row = [len(case['models']), case['bank'], case['level'], *case['gamma'], *matching]
        row.extend((i * 7) & 0xffffffff for i in range(64 * 256))
        for model in case['models']:
            row.append(len(model['textures']))
            for texture in model['textures']:
                name = texture['name'].encode()[:15].ljust(16, b'\0')
                row.extend(struct.unpack('<4I', name))
                row.extend([len(texture['pixels']), texture['sequence'], texture['flags'], texture['width'],
                            texture['height']])
                row.extend(texture['palette'])
                row.extend(texture['pixels'])
            row.append(len(model['lods']))
            for materials in model['lods']:
                row.append(len(materials))
                for material in materials:
                    row.extend(material)
        payload.append(' '.join(map(str, row)))
    lines = subprocess.check_output([str(Path(probe).resolve())], input=('\n'.join(payload) + '\n').encode()).splitlines()
    if len(lines) != count:
        raise ValueError(f'probe returned {len(lines)} lines for {count} cases')
    records_compared = 0
    materials_compared = 0
    fields_compared = 0
    for number, (case, line) in enumerate(zip(cases, lines)):
        level, bank = case['level'], case['bank']
        word(0x500b0c, 0x2005000)
        u.mem_write(0x2005000, bytes(case['gamma']))
        word(0x500b10, 0x2004000)
        u.mem_write(0x2004000, bytes(range(256)))
        word(0xc80d10, level)
        word(0xc65dcc, 0)
        state['heap'] = 0x3400000
        records = []
        spans = []
        record_ptrs = []
        fixture = 0x3000000
        for model in case['models']:
            start = len(records)
            for texture in model['textures']:
                if not texture['flags'] & (1 << level):
                    continue
                pixel_ptr, palette_ptr = fixture, fixture + len(texture['pixels'])
                u.mem_write(pixel_ptr, bytes(texture['pixels']))
                u.mem_write(palette_ptr, bytes(texture['palette']))
                fixture += len(texture['pixels']) + 768
                records.append(dict(name=texture['name'], count=len(texture['pixels']), flags=texture['flags'],
                                    width_bits=dimension_bits(texture['width']),
                                    height_bits=dimension_bits(texture['height']),
                                    pixel_ptr=pixel_ptr, palette_ptr=palette_ptr))
                descriptor = bytearray(36)
                struct.pack_into('<I', descriptor, 16, len(texture['pixels']))
                struct.pack_into('<HBB', descriptor, 20, texture['sequence'], texture['flags'], bank)
                struct.pack_into('<HH', descriptor, 24, texture['width'], texture['height'])
                struct.pack_into('<II', descriptor, 28, pixel_ptr, palette_ptr)
                u.mem_write(0xc54488 + (len(records) - 1) * 36, bytes(descriptor))
            spans.append((start, len(records)))
            for materials in model['lods']:
                header = bytearray(184)
                struct.pack_into('<I', header, 20, MATERIAL_SIZE * len(materials))
                struct.pack_into('<I', header, 168, len(materials))
                state['reads'] = [bytes(header), b''.join(materials)]
                record_ptrs.append(call(0x485e30, start, len(records), bank))
                if state['reads']:
                    raise ValueError(f'case {number}: 485E30 did not read the full LOD stream')
        if fixture >= 0x3400000:
            raise ValueError('fixture data overlaps the emulated heap')
        word(0xc54480, len(records))
        u.mem_write(0x3300000, bytes(case['base']))
        call(0x480600, 0x3300000)
        call(0x486880, bank)
        expected = []
        for index, record in enumerate(records):
            address = 0xc54488 + index * 36
            stored_flags = u.mem_read(address + 0x16, 1)[0]
            if stored_flags & SHARE_FLAG:
                raise ValueError(f'case {number}: original set the share flag on record {index}')
            pixel_count = struct.unpack('<I', u.mem_read(address + 0x10, 4))[0]
            pixel_ptr = struct.unpack('<I', u.mem_read(address + 0x1c, 4))[0]
            most = struct.unpack('<I', u.mem_read(address + 0xc, 4))[0]
            pixels = u.mem_read(pixel_ptr, pixel_count)
            expected.extend([0 if record['flags'] & 0x10 else 1, most, record['width_bits'],
                             record['height_bits'], pixel_count, *pixels])
        pointer = 0
        for model_index, model in enumerate(case['models']):
            start, end = spans[model_index]
            for materials in model['lods']:
                record_ptr = record_ptrs[pointer]
                pointer += 1
                material_count = struct.unpack('<I', u.mem_read(record_ptr + 0xa8, 4))[0]
                material_ptr = struct.unpack('<I', u.mem_read(record_ptr + 0xac, 4))[0]
                if material_count != len(materials):
                    raise ValueError(f'case {number}: material count differs')
                for index, original in enumerate(materials):
                    raw = bytearray(u.mem_read(material_ptr + index * MATERIAL_SIZE, MATERIAL_SIZE))
                    if raw[0x10] & 1:
                        global_slot = struct.unpack('<H', raw[0x18:0x1a])[0]
                        if not start <= global_slot < end:
                            raise ValueError(f'case {number}: material texture outside its model span')
                        slot = global_slot - start
                        address = 0xc54488 + global_slot * 36
                        pixel_ptr = struct.unpack('<I', u.mem_read(address + 0x1c, 4))[0]
                        palette_ptr = struct.unpack('<I', u.mem_read(address + 0x20, 4))[0]
                        if struct.unpack('<I', raw[0x44:0x48])[0] != pixel_ptr:
                            raise ValueError(f'case {number}: material pixel pointer differs')
                        if struct.unpack('<I', raw[0x48:0x4c])[0] != palette_ptr:
                            raise ValueError(f'case {number}: material palette pointer differs')
                        raw[0x44:0x4c] = original[0x44:0x4c]
                        # The port stores the model-local slot; normalize the global index.
                        struct.pack_into('<H', raw, 0x18, slot)
                        expected.extend([*raw, slot, raw[0x14], struct.unpack('<I', raw[0x14:0x18])[0]])
                    else:
                        if raw[0x44:0x4c] != original[0x44:0x4c]:
                            raise ValueError(f'case {number}: flat material pointer bytes changed')
                        expected.extend([*raw, -1, raw[0x14], struct.unpack('<I', raw[0x14:0x18])[0]])
        actual = list(map(int, line.split()))
        if len(actual) != len(expected):
            raise ValueError(f'case {number}: native fields {len(actual)} original fields {len(expected)}')
        texture_fields = sum(5 + record['count'] for record in records)
        for field, (got, want) in enumerate(zip(actual, expected)):
            if got == want:
                continue
            where = f'field {field}'
            if field < texture_fields:
                offset = field
                for index, record in enumerate(records):
                    size = 5 + record['count']
                    if offset < size:
                        slot = index
                        break
                    offset -= size
                if offset < 4:
                    where = f'texture {slot} ' + ['remapped', 'most frequent', 'width bits', 'height bits'][offset]
                elif offset == 4:
                    where = f'texture {slot} pixel count'
                else:
                    where = f'texture {slot} pixel {offset - 5}'
            else:
                offset = (field - texture_fields) % 79
                material = (field - texture_fields) // 79
                if offset < 76:
                    where = f'material {material} byte 0x{offset:02x}'
                elif offset == 76:
                    where = f'material {material} texture slot'
                elif offset == 77:
                    where = f'material {material} flat index'
                else:
                    where = f'material {material} bound colour'
            raise ValueError(f'case {number}: {where} native {got} original {want}')
        records_compared += len(records)
        materials_compared += sum(len(lod) for model in case['models'] for lod in model['lods'])
        fields_compared += len(actual)
        print(f'case {number + 1}/{count}: {len(records)} records, '
              f'{sum(len(lod) for model in case["models"] for lod in model["lods"])} materials match', flush=True)
    if records_compared == 0 or materials_compared == 0:
        raise ValueError('strict case coverage failed')
    return dict(source_sha256=digest, cases=count, compared_cases=count, records=records_compared,
                materials=materials_compared, fields=fields_compared, mismatches=0,
                addresses=['485e30', '480600', '486880', '480a30', '485ba0'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable')
    parser.add_argument('--probe', required=True)
    parser.add_argument('--cases', type=int, default=8)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    if args.cases < 1:
        parser.error('--cases must be positive')
    try:
        result = check(args.executable, args.probe, args.cases)
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(f'model bindings check failed: {error}', file=sys.stderr)
        return 1
    output = json.dumps(result, indent=2)
    print(output)
    if args.report:
        args.report.write_text(output + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
