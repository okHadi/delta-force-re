#!/usr/bin/env python3
"""Replay the deferred translucent pass against original DF1 x86.

Runs original 0x46c390, 0x46c3e0, 0x46cac0, 0x46cc30 and 0x46d190 with
the same faces and compares record and node pools, list state, segments,
the exact span plan and the 32-bit pixel output with the native port
through tools/effect_deferred_probe.

Cases cover signed coordinates, clip boundaries, pool exhaustion, and
the original divide faults (zero depth and depth overflow).
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess

SOURCE_SHA256 = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
STATE_BASE = 0xad3ef0


def to_signed(value):
    return value - 0x100000000 if value >= 0x80000000 else value


def hex_bytes(text):
    return bytes.fromhex(text)


def material_hex(material):
    return material.hex()


def make_case(rng, kind):
    """Return one case dict: frame, materials, faces, draw info."""
    materials = []
    table_a, table_b = 0x3010000, 0x3024000
    texture_address = 0x3000000
    material = bytearray(0x60)
    struct.pack_into('<4H', material, 30, 5, 5, 1 << 5, 1 << 5)  # +0x1e bits, +0x20 bits
    struct.pack_into('<I', material, 0x18, 0)                    # lod 0
    struct.pack_into('<I', material, 0x44, texture_address)
    struct.pack_into('<I', material, 0x14, 0x00112233)
    materials.append((0x18004000, bytes(material), table_a, table_b))
    if kind == 'many':
        points = []
        for index in range(60):
            x = 1 + index % 50
            y = 1 + (index % 5)
            points.append((0x414, x << 16, y << 16, 1 << 20))
        return dict(kind=kind, flags=0x414, points=points, materials=materials,
                    table_a=table_a, table_b=table_b, texture_address=texture_address,
                    texture_size=1 << (5 + 5))
    if kind == 'wide':
        points = [(4 << 16, 1 << 16, 1 << 20), (16 << 16, 7 << 16, 1 << 20), (28 << 16, 2 << 16, 1 << 20)]
        flags = 0x414
    elif kind == 'alpha':
        points = [(4 << 16, 1 << 16, 1 << 20), (16 << 16, 7 << 16, 1 << 20), (28 << 16, 2 << 16, 1 << 20)]
        flags = 0x214
    elif kind == 'negative':
        points = [(-8 << 16, 1 << 16, 1 << 20), (30 << 16, 9 << 16, 1 << 20), (6 << 16, 5 << 16, 1 << 20)]
        flags = 0x414
    elif kind == 'clip':
        points = [(-40 << 16, 0), (100 << 16, 7 << 16), (10 << 16, 4 << 16)]
        points = [(p[0], p[1], 1 << 20) for p in points]
        flags = 0x414
    else:
        points = [(rng.randrange(-8, 40) << 16, rng.randrange(0, 8) << 16, rng.randrange(0x2001, 1 << 22))
                  for _ in range(3)]
        flags = rng.choice([0x414, 0x214, 0x418, 0x218, 0x104, 0x204])
    faces = [(flags, point[0], point[1], point[2]) for point in points]
    return dict(kind=kind, flags=flags, points=faces, materials=materials,
                table_a=table_a, table_b=table_b, texture_address=texture_address,
                texture_size=1 << (5 + 5))


def build_script(case, width, height, texture_bytes, table_bytes, draw):
    lines = [f"frame {width:x} {height:x} 0 0 {width - 1:x} {height - 1:x} 20 "
             f"18001000 10000 0 {case['table_a']:x} {case['table_b']:x}"]
    for address, bytes_, table_a, table_b in case['materials']:
        lines.append(f"material {address:x} {table_a:x} {table_b:x} {bytes_.hex()}")
    for index, (flags, x, y, z) in enumerate(case['points']):
        vertices = [
            (x, y, z, 0, 0, 0x10000),
            ((x + 0x20000) & 0xffffffff, (y + 0x30000) & 0xffffffff, z, 0, 0, 0x10000),
            ((x + 0x40000) & 0xffffffff, y, z, 0, 0, 0x10000),
        ]
        fields = [f"{flags:x}", "18004000"]
        for vertex in vertices:
            for value in vertex:
                fields.append(f"{value & 0xffffffff:x}")
        lines.append("face " + " ".join(fields))
    if draw:
        image = bytearray(0x19000)
        image[0:len(texture_bytes)] = texture_bytes
        image[0x10000:0x10000 + 0x400] = table_bytes[:0x400]
        image[0x18000:0x18000 + 0x2000] = table_bytes[0x400:0x400 + 0x2000]
        lines.append(f"draw {case['table_a'] - 0x10000:x} {image.hex()}")
    lines.append("run")
    return "\n".join(lines) + "\n"


def make_original(pe):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
    u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    u.mem_map(0x400000, 0x1000000)
    u.mem_write(0x400000, pe.get_memory_mapped_image())
    u.mem_map(0x2000000, 0x10000)
    u.mem_map(0x3000000, 0x40000)
    u.mem_map(0x18000000, 0x100000)
    call = lambda address, *args: None

    def invoke(address, *args):
        u.mem_write(0x18000800, struct.pack('<' + 'I' * (len(args) + 1), 0x18000000, *args))
        u.reg_write(UC_X86_REG_ESP, 0x18000800)
        u.reg_write(UC_X86_REG_FPCW, 0x037f)
        u.emu_start(address, 0x18000000, count=2000000)
        if u.reg_read(UC_X86_REG_EIP) != 0x18000000:
            raise ValueError(f'original function {address:x} did not return')
    return u, invoke


def run_case(pe, case, probe, width, height):
    texture_size = case['texture_size']
    texture_bytes = bytes((i * 37 + 11) & 255 for i in range(texture_size))
    table_bytes = b''.join(struct.pack('<I', (i * 0x01010101 ^ 0xffa5c3e7) & 0xffffffff) for i in range(256 + 0x2000))
    script = build_script(case, width, height, texture_bytes, table_bytes, True)
    native = subprocess.check_output([str(probe)], input=script.encode()).decode().splitlines()
    native_fields = {}
    for line in native:
        parts = line.split()
        native_fields.setdefault(parts[0], []).append(parts[1:])

    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
    u, invoke = make_original(pe)
    # Original pixel table init and shared sentinel init.
    invoke(0x4686d0, 0, 1, 32)
    invoke(0x46c390)
    table_a = case['table_a']
    table_b = case['table_b']
    # Target record: clip at +0x34, depth mode +0x50, row tables +0x68/+0xc68.
    target = bytearray(0x2000)
    struct.pack_into('<4i', target, 0x34, 0, 0, width - 1, height - 1)
    struct.pack_into('<I', target, 0x50, 0x20)
    for row in range(height):
        struct.pack_into('<I', target, 0x68 + row * 4, 0x3021000 + row * width * 4)
        struct.pack_into('<I', target, 0xc68 + row * 4, 0x3020000 + row * width * 2)
    u.mem_write(0x18001000, bytes(target))
    u.mem_write(0xbfc104, struct.pack('<I', table_a))
    u.mem_write(0xbfc100, struct.pack('<I', table_b))
    invoke(0x46c3e0, 0x18001000, 0x10000, 0, table_a, table_b)
    # Materials, texture and table bytes.
    for address, bytes_, _, _ in case['materials']:
        u.mem_write(address, bytes_)
    u.mem_write(case['texture_address'], texture_bytes)
    for index in range(256 + 0x2000):
        u.mem_write((table_a + index * 4) if index < 256 else (table_a + 0x8000 + (index - 256) * 4),
                    struct.pack('<I', struct.unpack_from('<I', table_bytes, index * 4)[0]))
    # Faces.
    for index, (flags, _, _, _) in enumerate(case['points']):
        flags, x, y, z = case['points'][index]
        vertices = [
            (x, y, z, 0, 0, 0x10000),
            ((x + 0x20000) & 0xffffffff, (y + 0x30000) & 0xffffffff, z, 0, 0, 0x10000),
            ((x + 0x40000) & 0xffffffff, y, z, 0, 0, 0x10000),
        ]
        raw = b''.join(struct.pack('<6i', *[to_signed(v & 0xffffffff) for v in vertex]) + bytes(8)
                       for vertex in vertices)
        u.mem_write(0x18002000, raw)
        invoke(0x46cac0, flags, 0x18002000, 0x18004000)
    record_cursor = struct.unpack('<I', u.mem_read(0xbc5c64, 4))[0]
    node_cursor = struct.unpack('<I', u.mem_read(0xbc5c60, 4))[0]
    original_records = bytes(u.mem_read(0xbb11b0, record_cursor - 0xbb11b0))
    original_nodes = bytes(u.mem_read(0xb82370, node_cursor - 0xb82370))

    invoke(0x46cc30)
    original_records_after = bytes(u.mem_read(0xbb11b0, record_cursor - 0xbb11b0))
    original_nodes_after = bytes(u.mem_read(0xb82370, node_cursor - 0xb82370))
    original_globals = bytes(u.mem_read(0xbc59d0, 0xbc5c68 - 0xbc59d0))

    # Pixels: prefill and capture spans at the two call sites.
    u.mem_write(0x3021000, b''.join(struct.pack('<I', 0x20202020) for _ in range(width * height)))
    u.mem_write(0x3020000, b''.join(struct.pack('<H', 0xffff) for _ in range(width * height)))
    spans = []

    def hook(uc, address, size, data):
        if address in (0x46d5b5, 0x46d760):
            span_address = struct.unpack('<I', uc.mem_read(uc.reg_read(UC_X86_REG_ESP), 4))[0]
            spans.append(struct.unpack('<10i', uc.mem_read(span_address, 40)))
    u.hook_add(unicorn.UC_HOOK_CODE, hook)
    # The original keeps the previous chunk steps in its frame; the first
    # use before any full chunk reads stale stack. Pin that region to zero
    # so the native default matches.
    u.mem_write(0x18000800 - 0xb8, bytes(0xb8))
    invoke(0x46d190)
    original_colour = bytes(u.mem_read(0x3021000, width * height * 4))
    original_depth = bytes(u.mem_read(0x3020000, width * height * 2))
    original_counters = struct.unpack('<4I', u.mem_read(0xbc5c50, 16))

    # Compare.
    def parse_dump(label):
        entries = native_fields.get(label)
        if not entries:
            return b''
        parts = entries[0]
        start = int(parts[0], 16)
        data = bytes.fromhex(parts[-1])
        return start, data

    problems = []
    start, data = parse_dump('records')
    if data != original_records_after:
        problems.append(f'record pool differs ({len(data)} vs {len(original_records_after)})')
    start, data = parse_dump('nodes')
    if data != original_nodes_after:
        problems.append(f'node pool differs ({len(data)} vs {len(original_nodes_after)})')
    start, data = parse_dump('globals')
    if data != original_globals:
        for index, (a, b) in enumerate(zip(data, original_globals)):
            if a != b:
                problems.append(f'globals differ at {0xbc59d0 + index:#x}: {a:#x} vs {b:#x}')
                break
    native_counters = [int(v, 16) for v in native_fields['counts'][0]]
    # Counts line order: records, nodes, segments, span records, span
    # pixels, record visits, slot visits.
    native_draw = [native_counters[6], native_counters[5], native_counters[3], native_counters[4]]
    if native_draw != list(original_counters):
        problems.append(f'draw counters differ: {native_draw} vs {list(original_counters)}')
    native_plan = [list(map(lambda v: to_signed(int(v, 16)), line)) for line in native_fields.get('span', [])]
    if len(native_plan) != len(spans):
        problems.append(f'span count {len(native_plan)} vs {len(spans)}')
    for index, (a, b) in enumerate(zip(native_plan, spans)):
        if a[:10] != list(b):
            problems.append(f'span {index} differs: {a[:10]} vs {list(b)}')
    if not problems:
        native_colour = b''.join(struct.pack('<I', to_signed(int(v, 16)) & 0xffffffff)
                                 for v in native_fields['colour'][0])
        native_depth = b''.join(struct.pack('<H', to_signed(int(v, 16)) & 0xffff)
                                for v in native_fields['depth'][0])
        if native_colour != original_colour:
            problems.append('colour plane differs')
        if native_depth != original_depth:
            problems.append('depth plane differs')
    return dict(kind=case['kind'], spans=len(spans), problems=problems,
                records=record_cursor - 0xbb11b0, nodes=node_cursor - 0xb82370)


def check_traps(pe, probe):
    """Original divide faults: zero depth and reciprocal overflow."""
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
    results = []
    for kind, z in (('zero-depth', 0), ('shallow-depth', 0x1000)):
        material = bytearray(0x60)
        struct.pack_into('<4H', material, 30, 0, 0, 1, 1)
        struct.pack_into('<I', material, 0x44, 0x3000000)
        script = ("frame 40 8 0 0 3f 7 20 " f"18001000 10000 0 3010000 3024000\n"
                  f"material 18004000 3010000 3024000 {material.hex()}\n")
        for x, y in ((4, 1), (6, 4), (8, 1)):
            script += (f"face 414 18004000 {x << 16:x} {y << 16:x} {z:x} 0 0 10000 "
                       f"{(x + 2) << 16:x} {(y + 3) << 16:x} {z:x} 0 0 10000 "
                       f"{(x + 4) << 16:x} {y << 16:x} {z:x} 0 0 10000\n")
        script += "run\n"
        native = subprocess.run([str(probe)], input=script.encode(), capture_output=True)
        native_trapped = native.returncode != 0 and b'deferred' in native.stderr
        u, invoke = make_original(pe)
        invoke(0x4686d0, 0, 1, 32)
        invoke(0x46c390)
        target = bytearray(0x2000)
        struct.pack_into('<4i', target, 0x34, 0, 0, 63, 7)
        struct.pack_into('<I', target, 0x50, 0x20)
        for row in range(8):
            struct.pack_into('<I', target, 0x68 + row * 4, 0x3021000 + row * 64 * 4)
            struct.pack_into('<I', target, 0xc68 + row * 4, 0x3020000 + row * 64 * 2)
        u.mem_write(0x18001000, bytes(target))
        u.mem_write(0xbfc104, struct.pack('<I', 0x3010000))
        u.mem_write(0xbfc100, struct.pack('<I', 0x3024000))
        invoke(0x46c3e0, 0x18001000, 0x10000, 0, 0x3010000, 0x3024000)
        u.mem_write(0x18004000, bytes(material))
        vertices = [(x << 16, y << 16, z, 0, 0, 0x10000) for x, y in ((4, 1), (6, 4), (8, 1))]
        raw = b''.join(struct.pack('<6i', *vertex) + bytes(8) for vertex in vertices)
        u.mem_write(0x18002000, raw)
        original_trapped = False
        try:
            invoke(0x46cac0, 0x414, 0x18002000, 0x18004000)
        except unicorn.UcError:
            original_trapped = True
        results.append(dict(kind=kind, original_trapped=original_trapped, native_trapped=native_trapped,
                            ok=original_trapped and native_trapped))
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable')
    parser.add_argument('--probe', required=True)
    parser.add_argument('--cases', type=int, default=30)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    import pefile
    source = Path(args.executable).read_bytes()
    if hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
        raise ValueError('Unsupported original executable hash')
    pe = pefile.PE(data=source)
    probe = Path(args.probe).resolve()
    rng = random.Random(461772)
    kinds = ['wide', 'alpha', 'negative', 'clip'] + ['random'] * args.cases + ['many']
    total_spans = 0
    failures = []
    for index, kind in enumerate(kinds):
        case = make_case(rng, kind)
        result = run_case(pe, case, probe, 64, 8)
        total_spans += result['spans']
        if result['problems']:
            failures.append(dict(case=index, kind=kind, problems=result['problems'][:6]))
    traps = check_traps(pe, probe)
    output = json.dumps(dict(source_sha256=SOURCE_SHA256, cases=len(kinds), spans=total_spans,
                             traps=traps, failures=failures,
                             scope='original 0x46c390/0x46c3e0/0x46cac0/0x46cc30/0x46d190; '
                                   'record and node pools, globals, span plan fields, colour and depth planes; '
                                   'signed coordinates, clip edges, wide and tiny spans'),
                        indent=2)
    print(output)
    if args.report:
        args.report.write_text(output + '\n')
    raise SystemExit(bool(failures))


if __name__ == '__main__':
    main()
