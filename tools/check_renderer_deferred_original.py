#!/usr/bin/env python3
"""Replay the renderer deferred bridge against original DF1 x86.

Checks the join of the exact DeferredPass to the model draw path:
- 0x46cac0/0x46cc30/0x46d190 span plans and the complete 0x4683b0 selector
  over the 32-bit pixel table (flat, textured, effect and uniform kernels),
- pixel and depth results with varied unused one-pixel step bytes,
- the 0x46be50/0x46bf60/0x46bf10 drain planes with hand-filled queues,
- 0x46b1d0 routing to the four queues through the bridge queue stage.

The original executable stays read-only; only its memory image is emulated.
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess

SOURCE_SHA256 = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
SPAN_FIELDS = ['pixel', 'length', 'u', 'v', 'depth', 'shade', 'du', 'dv', 'ddepth', 'dshade']
KERNELS = {
    0x4672b0: 'no_write', 0x467320: 'uniform_fill', 0x467430: 'flat_shade',
    0x468cda: 'texture_uniform', 0x4694df: 'texture_varying',
    0x4690cc: 'texture_cutout', 0x469945: 'texture_varying_cutout',
    0x467ff0: 'effect_alpha_blend', 0x4682a0: 'effect_half_blend',
}
# Keys that set a kernel; every other masked key keeps the previous kernel.
KEY_KERNEL = {
    0x000: 'uniform_fill', 0x004: 'flat_shade', 0x008: 'flat_shade',
    0x010: 'no_write', 0x014: 'texture_uniform', 0x018: 'texture_varying',
    0x030: 'no_write', 0x034: 'texture_cutout', 0x038: 'texture_varying_cutout',
    0x100: 'no_write', 0x104: 'no_write', 0x108: 'no_write',
    0x200: 'no_write', 0x204: 'no_write', 0x208: 'no_write',
    0x210: 'effect_alpha_blend', 0x214: 'effect_alpha_blend', 0x218: 'effect_alpha_blend',
    0x410: 'effect_half_blend', 0x414: 'effect_half_blend', 0x418: 'effect_half_blend',
    0x430: 'effect_half_blend', 0x434: 'effect_half_blend', 0x438: 'effect_half_blend',
}
DEFAULT_KERNEL = 'no_write'


def to_signed(value):
    return value - 0x100000000 if value >= 0x80000000 else value


def to_unsigned(value):
    return value & 0xffffffff


def load(executable):
    import pefile
    source = Path(executable).read_bytes()
    if hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
        raise ValueError('Unsupported original executable hash')
    return pefile.PE(data=source)


def machine(pe):
    import unicorn
    u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    u.mem_map(0x400000, 0x1000000)
    u.mem_write(0x400000, pe.get_memory_mapped_image())
    u.mem_map(0x2000000, 0x10000)
    u.mem_map(0x3000000, 0x40000)
    u.mem_map(0x18000000, 0x10000)
    return u


def caller(u):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW

    def call(address, *args):
        u.mem_write(0x18000800, struct.pack('<' + 'I' * (len(args) + 1), 0x18000000, *args))
        u.reg_write(UC_X86_REG_ESP, 0x18000800)
        u.reg_write(UC_X86_REG_FPCW, 0x037f)
        u.emu_start(address, 0x18000000, count=2000000)
        if u.reg_read(UC_X86_REG_EIP) != 0x18000000:
            raise ValueError(f'original function {address:x} did not return')
    return call


def texture_bytes(seed, bits_x, bits_y):
    return bytes((i * 37 + seed * 13) & 255 for i in range(1 << (bits_x + bits_y)))


def shade_bytes():
    return b''.join(struct.pack('<I', (i * 0x01010101 ^ 0xffa5c3e7) & 0xffffffff) for i in range(64 * 256))


def build_material(colour, bits_x, bits_y, texture=0x3000000):
    material = bytearray(0x60)
    struct.pack_into('<I', material, 0x14, colour)
    struct.pack_into('<I', material, 0x18, 0)
    struct.pack_into('<H', material, 0x20, bits_x)
    struct.pack_into('<H', material, 0x22, bits_y)
    struct.pack_into('<H', material, 0x24, max(0, (1 << bits_x) - 1))
    struct.pack_into('<H', material, 0x26, max(0, (1 << bits_y) - 1))
    struct.pack_into('<I', material, 0x44, texture)
    return bytes(material)


def probe_run(probe, script):
    return subprocess.run([str(probe)], input=script.encode(), capture_output=True)


def probe_fields(output):
    fields = {}
    for line in output.decode().splitlines():
        parts = line.split()
        fields.setdefault(parts[0], []).append(parts[1:])
    return fields


def native_planes(fields):
    colour = b''.join(struct.pack('<I', int(v, 16) & 0xffffffff) for v in fields['colour'][0])
    depth = b''.join(struct.pack('<H', int(v, 16) & 0xffff) for v in fields['depth'][0])
    return colour, depth


def original_spans_hook(u):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP
    spans = []

    def hook(uc, address, size, data):
        if address in (0x46d5b5, 0x46d760):
            span_address = struct.unpack('<I', uc.mem_read(uc.reg_read(UC_X86_REG_ESP), 4))[0]
            spans.append(struct.unpack('<10i', uc.mem_read(span_address, 40)))
    u.hook_add(unicorn.UC_HOOK_CODE, hook)
    return spans


def setup_target(u, width, height, colour_address, depth_address):
    target = bytearray(0x2000)
    struct.pack_into('<4i', target, 0x34, 0, 0, width - 1, height - 1)
    struct.pack_into('<I', target, 0x50, 0x20)
    for row in range(height):
        struct.pack_into('<I', target, 0x68 + row * 4, colour_address + row * width * 4)
        struct.pack_into('<I', target, 0xc68 + row * 4, depth_address + row * width * 2)
    u.mem_write(0x18001000, bytes(target))


def prefill(u, width, height, seed):
    u.mem_write(0x3021000, b''.join(struct.pack('<I', (i * 1234567 ^ 0xabcdef90 ^ seed) & 0xffffffff)
                                    for i in range(width * height)))
    u.mem_write(0x3020000, b''.join(struct.pack('<H', (i * 317 + seed * 17) & 0xffff)
                                    for i in range(width * height)))


def original_plan(u, call):
    call(0x46c390)
    call(0x46c3e0, 0x18001000, 0x10000, 0, 0x3010000, 0x3024000)


def face_vertices(u, vertices, address=0x18002000):
    raw = b''.join(struct.pack('<6i', *[to_signed(v & 0xffffffff) for v in vertex]) + bytes(8)
                   for vertex in vertices)
    u.mem_write(address, raw)


def random_vertices(rng, width, height, textured):
    """Screen 16.16 triples inside the original list bound and row table."""
    points = []
    for _ in range(3):
        x = rng.randrange(-8, min(width + 8, 64)) * 65536 + rng.randrange(65536)
        y = rng.randrange(0, height) * 65536 + rng.randrange(65536)
        points.append((x, y, rng.randrange(0x2001, 0x200000)))
    if textured:
        return [(x, y, z, rng.randrange(1 << 20), rng.randrange(1 << 20), rng.randrange(0x10000, 0x300000))
                for x, y, z in points]
    return [(x, y, z, 0, 0, rng.randrange(0x10000, 0x300000)) for x, y, z in points]


def wide_points():
    return [(4 << 16, 1 << 16, 1 << 20, 0x120000, 0x40000, 0x20000),
            (16 << 16, 7 << 16, 1 << 20, 0x120000, 0x40000, 0x20000),
            (28 << 16, 2 << 16, 1 << 20, 0x120000, 0x40000, 0x20000)]


def make_span_cases(rng, count):
    keys = [0x14, 0x4, 0x18, 0x34, 0x38, 0x214, 0x414, 0x418, 0x104, 0x200,
            0x0, 0x30, 0x24, 0x28, 0x44, 0x54, 0x214, 0x14]
    cases = []
    # Fixed wide, alpha, negative-x and cutout cases, then random sequences
    # that mix handled and unhandled keys to exercise the selector state.
    cases.append(dict(kind='wide', keys=[0x14], points=wide_points(), bits=(5, 5)))
    cases.append(dict(kind='flat', keys=[0x4], points=wide_points(), bits=(0, 0)))
    cases.append(dict(kind='alpha', keys=[0x214], points=wide_points(), bits=(5, 5)))
    cases.append(dict(kind='negative', keys=[0x14], points=[(-8 << 16, 1 << 16, 1 << 20, 0x120000, 0x40000, 0x20000),
                                                     (30 << 16, 9 << 16, 1 << 20, 0x120000, 0x40000, 0x20000),
                                                     (6 << 16, 5 << 16, 1 << 20, 0x120000, 0x40000, 0x20000)], bits=(4, 4)))
    cases.append(dict(kind='cutout', keys=[0x34], points=wide_points(), bits=(5, 5)))
    cases.append(dict(kind='uniform', keys=[0x0], points=wide_points(), bits=(0, 0)))
    for index in range(count):
        faces = rng.randrange(1, 6)
        keys_case = [rng.choice(keys) for _ in range(faces)]
        bits_x, bits_y = rng.randrange(7), rng.randrange(7)
        points = random_vertices(rng, 64, 8, textured=True)
        cases.append(dict(kind='random', keys=keys_case, points=points, bits=(bits_x, bits_y)))
    return cases


def span_case_script(case, seed, width, height):
    lines = [f"frame {width:x} {height:x} 0 0 {width - 1:x} {height - 1:x}",
             "mode 1",
             "steps 0 0 0 0",
             f"material 18004000 3010000 3024000 {case['bits'][0]:x} {case['bits'][1]:x} "
             f"{seed:x} {case['keys'][0]:x} {build_material(case['keys'][0], case['bits'][0], case['bits'][1]).hex()}"]
    for flags in case['keys']:
        points = case['points']
        if case['kind'] == 'random':
            points = case['points']
        fields = [f"{flags:x}", "18004000"]
        for vertex in points:
            fields.extend(f"{v & 0xffffffff:x}" for v in vertex)
        lines.append("addface " + " ".join(fields))
    lines += ["flush", f"draw {width:x} {height:x}", "plan", "counts"]
    return "\n".join(lines) + "\n"


def run_span_case(pe, probe, case, seed, width=64, height=8):
    import unicorn
    u = machine(pe)
    call = caller(u)
    call(0x4686d0, 0, 1, 32)
    setup_target(u, width, height, 0x3021000, 0x3020000)
    u.mem_write(0xbfc104, struct.pack('<I', 0x3010000))
    u.mem_write(0xbfc100, struct.pack('<I', 0x3024000))
    u.mem_write(0x3010000, shade_bytes())
    u.mem_write(0x3024000, b''.join(struct.pack('<I', (i * 0x01010101) & 0xffffffff) for i in range(256)))
    u.mem_write(0x3000000, texture_bytes(seed, case['bits'][0], case['bits'][1]))
    material = build_material(case['keys'][0], case['bits'][0], case['bits'][1])
    u.mem_write(0x18004000, material)
    original_plan(u, call)
    spans = original_spans_hook(u)
    for flags in case['keys']:
        vertices = case['points']
        face_vertices(u, vertices)
        call(0x46cac0, flags, 0x18002000, 0x18004000)
    call(0x46cc30)
    prefill(u, width, height, 1)
    # Pin the frame region the first 0x46d190 keeps its chunk steps in.
    u.mem_write(0x18000800 - 0xb8, bytes(0xb8))
    call(0x46d190)
    original_colour = bytes(u.mem_read(0x3021000, width * height * 4))
    original_depth = bytes(u.mem_read(0x3020000, width * height * 2))
    original_counters = struct.unpack('<4I', u.mem_read(0xbc5c50, 16))
    original_key = struct.unpack('<I', u.mem_read(0xa16538, 4))[0]
    original_kernel = struct.unpack('<I', u.mem_read(0xa164f4, 4))[0]
    original_kernel = KERNELS.get(original_kernel, hex(original_kernel))

    native = probe_run(probe, span_case_script(case, seed, width, height))
    if native.returncode != 0:
        raise ValueError(f'native span case failed: {native.stderr.decode()[:200]}')
    fields = probe_fields(native.stdout)
    problems = []
    native_plan = [list(map(lambda v: to_signed(int(v, 16)), line[:10])) for line in fields.get('span', [])]
    if len(native_plan) != len(spans):
        problems.append(f'span count {len(native_plan)} vs {len(spans)}')
    for index, (a, b) in enumerate(zip(native_plan, spans)):
        if used_span_fields(a) != used_span_fields(b):
            problems.append(f'span {index} differs: {a} vs {list(b)}')
    colour, depth = native_planes(fields)
    if colour != original_colour:
        problems.append('colour plane differs')
    if depth != original_depth:
        problems.append('depth plane differs')
    tokens = fields['counts'][0]
    native_counts = [int(v, 16) for v in tokens[:8]]
    # Counts order: records, nodes, segments, span records, span pixels,
    # record visits, slot visits, key, kernel name, steps.
    native_draw = [native_counts[6], native_counts[5], native_counts[3], native_counts[4]]
    if native_draw != list(original_counters):
        problems.append(f'draw counters differ: {native_draw} vs {list(original_counters)}')
    if native_counts[7] != original_key:
        problems.append(f'selector key differs: {native_counts[7]:#x} vs {original_key:#x}')
    if tokens[8] != original_kernel:
        problems.append(f'selector kernel differs: {tokens[8]} vs {original_kernel}')
    return dict(kind=case['kind'], spans=len(spans), problems=problems)


def narrow_points():
    return [(4 << 16, 1 << 16, 1 << 20, 0x120000, 0x40000, 0x20000),
            (4 << 16, 3 << 16, 1 << 20, 0x120000, 0x40000, 0x20000),
            (5 << 16, 2 << 16, 1 << 20, 0x120000, 0x40000, 0x20000)]


def used_span_fields(span):
    """One-pixel kernels use fields 0..5; their step fields are unused."""
    return list(span[:6]) if span[1] == 1 else list(span)


def plan_steps(spans):
    """Chunk steps the original frame keeps: the last computed (length != 1)
    span steps, zero before any computation."""
    steps = [0, 0, 0, 0]
    for span in spans:
        if span[1] != 1:
            steps = list(span[6:10])
    return steps


def persistence_case(pe, probe, width=64, height=8):
    """Vary original stack bytes and native diagnostic steps for all kernels.
    Compare every pixel, depth word and used span field across two frames."""
    problems = []
    keys = [0, 4, 0x10, 0x14, 0x18, 0x34, 0x38, 0x214, 0x414]
    wide = [(4 << 16, 1 << 16, 1 << 20, 0x120000, 0x40000, 0x20000),
            (16 << 16, 7 << 16, 1 << 19, 0x280000, 0x90000, 0x100000),
            (28 << 16, 2 << 16, 1 << 21, 0x70000, 0x170000, 0x60000)]
    def face_line(flags, points):
        fields = [f"{flags:x}", "18004000"]
        for vertex in points:
            fields.extend(f"{v & 0xffffffff:x}" for v in vertex)
        return "addface " + " ".join(fields) + "\n"
    total = 0
    for flags in keys:
        baseline = None
        for poison in (0, 0x6d):
            u = machine(pe)
            call = caller(u)
            call(0x4686d0, 0, 1, 32)
            setup_target(u, width, height, 0x3021000, 0x3020000)
            u.mem_write(0xbfc104, struct.pack('<I', 0x3010000))
            u.mem_write(0xbfc100, struct.pack('<I', 0x3024000))
            u.mem_write(0x3010000, shade_bytes())
            u.mem_write(0x3024000, shade_bytes()[:1024])
            u.mem_write(0x3000000, texture_bytes(9, 5, 5))
            u.mem_write(0x18004000, build_material(0x11, 5, 5))
            call(0x46c390)
            spans = original_spans_hook(u)
            originals = []
            for points in (wide, narrow_points()):
                call(0x46c3e0, 0x18001000, 0x10000, 0, 0x3010000, 0x3024000)
                face_vertices(u, points)
                call(0x46cac0, flags, 0x18002000, 0x18004000)
                call(0x46cc30)
                prefill(u, width, height, 1)
                u.mem_write(0x18000800 - 0xb8, bytes([poison]) * 0xb8)
                spans.clear()
                call(0x46d190)
                originals.append((list(spans), bytes(u.mem_read(0x3021000, width*height*4)),
                                  bytes(u.mem_read(0x3020000, width*height*2))))
            if not any(row[1] == 1 for row in originals[1][0]):
                problems.append(f'{flags:x}: narrow fixture produced no one-pixel span')
            used = [([used_span_fields(row) for row in plan], colour, depth)
                    for plan, colour, depth in originals]
            if baseline is not None and used != baseline:
                problems.append(f'{flags:x}: unused original stack bytes changed the result')
            baseline = used
            script = (f"frame {width:x} {height:x} 0 0 {width-1:x} {height-1:x}\nmode 1\n"
                      f"steps {poison:x} {poison:x} {poison:x} {poison:x}\n"
                      f"material 18004000 3010000 3024000 5 5 9 0 {build_material(0x11,5,5).hex()}\n")
            for frame, points in enumerate((wide, narrow_points())):
                if frame:
                    script += f"frame {width:x} {height:x} 0 0 {width-1:x} {height-1:x}\n"
                script += face_line(flags, points) + f"flush\ndraw {width:x} {height:x}\nplan\ncounts\n"
            native = probe_run(probe, script)
            if native.returncode:
                raise ValueError(native.stderr.decode()[:200])
            fields = probe_fields(native.stdout)
            native_spans = [list(map(lambda v: to_signed(int(v,16)), line[:10]))
                            for line in fields.get('span', [])]
            expected_spans = [row for original in originals for row in original[0]]
            if [used_span_fields(row) for row in native_spans] != [used_span_fields(row) for row in expected_spans]:
                problems.append(f'{flags:x}/{poison:x}: used span fields differ')
            for frame, (_, colour, depth) in enumerate(originals):
                native_colour = b''.join(struct.pack('<I',int(v,16)) for v in fields['colour'][frame])
                native_depth = b''.join(struct.pack('<H',int(v,16)) for v in fields['depth'][frame])
                if native_colour != colour or native_depth != depth:
                    problems.append(f'{flags:x}/{poison:x}/{frame}: pixel or depth plane differs')
            total += 2
    return dict(kind='unused_one_pixel_steps', problems=problems, frames=total, kernels=len(keys))


def check_direct_cases(pe, probe, count):
    u=machine(pe);call=caller(u);call(0x4686d0,0,1,32)
    width,height=64,8
    setup_target(u,width,height,0x3021000,0x3020000)
    u.mem_write(0x8ee1e0,struct.pack('<I',0x18001000))
    u.mem_write(0x8ee1e8,bytes(4))
    u.mem_write(0xbfc104,struct.pack('<I',0x3010000))
    u.mem_write(0xbfc108,struct.pack('<I',0x3018000))
    u.mem_write(0xbfc100,struct.pack('<I',0x3024000))
    u.mem_write(0x3010000,shade_bytes())
    rng=random.Random(462991);script=[];expected=[]
    keys=[4,8,0x14,0x18,0x34,0x38,0x104,0x108,0x114,0x214,0x218,0x414,0x418,0x434,0x438]
    for number in range(count):
        flags=keys[number%len(keys)]
        bx,by=rng.randrange(7),rng.randrange(7)
        colour=rng.randrange(256) if not flags&0x200 else rng.randrange(0x1000000)
        material=bytearray(build_material(colour,bx,by))
        material_flags=(1 if flags&0x10 else 0)|(0x200 if flags&0x20 else 0)|(0x8000 if flags&0x200 else 0)|(0x80 if flags&0x400 else 0)|(8 if flags&0x100 else 0)
        struct.pack_into('<I',material,0x10,material_flags)
        points=random_vertices(rng,width,height,True)
        if number%13==0:
            x,y=rng.randrange(2,61),rng.randrange(1,6)
            points=[(x<<16,y<<16,1<<20,0,0,0x200000),
                    (x<<16,(y+1)<<16,1<<20,0,0,0x200000),
                    ((x+1)<<16,y<<16,1<<20,0,0,0x200000)]
        if not flags&8:
            points=[tuple(list(v[:5])+[points[0][5]]) for v in points]
        u.mem_write(0x3000000,texture_bytes(number+1,bx,by))
        u.mem_write(0x18004000,bytes(material))
        face_vertices(u,points)
        prefill(u,width,height,1)
        call(0x462df0,flags,0x18002000,0x18004000)
        expected.append((bytes(u.mem_read(0x3021000,width*height*4)),bytes(u.mem_read(0x3020000,width*height*2)),
                         struct.unpack('<I',u.mem_read(0xa16538,4))[0],struct.unpack('<I',u.mem_read(0xa164f4,4))[0]))
        tiny=((8192+(colour&255))*0x01010101^0xffa5c3e7)&0xffffffff
        script += [f'frame {width:x} {height:x} 0 0 {width-1:x} {height-1:x}',
                   f'material 18004000 3010000 3024000 {bx:x} {by:x} {number+1:x} {tiny:x} {material.hex()}',
                   'direct '+f'{flags:x} 18004000 '+' '.join(f'{v&0xffffffff:x}' for p in points for v in p),
                   'counts']
    done=probe_run(probe,'\n'.join(script)+'\n')
    if done.returncode:raise ValueError('Direct triangle probe failed: '+done.stderr.decode()[:500])
    fields=probe_fields(done.stdout)
    if len(fields.get('colour',[]))!=count or len(fields.get('depth',[]))!=count or len(fields.get('counts',[]))!=count:
        raise ValueError('Direct probe output count differs')
    failures=[]
    for index,(original_colour,original_depth,key,kernel) in enumerate(expected):
        colour=b''.join(struct.pack('<I',int(v,16)) for v in fields['colour'][index])
        depth=b''.join(struct.pack('<H',int(v,16)) for v in fields['depth'][index])
        record=fields['counts'][index]
        problems=[]
        if colour!=original_colour:
            differences=[i for i in range(width*height) if colour[i*4:i*4+4]!=original_colour[i*4:i*4+4]]
            problems.append('colour plane differs at '+str(differences[:8]))
        if depth!=original_depth:problems.append('depth plane differs')
        if int(record[7],16)!=key or record[8]!=KERNELS[kernel]:problems.append('selector state differs')
        if problems:failures.append(dict(case=index,flags=hex(keys[index%len(keys)]),problems=problems))
    return dict(cases=count,failures=failures)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable')
    parser.add_argument('--probe', required=True)
    parser.add_argument('--cases', type=int, default=60)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    pe = load(args.executable)
    probe = Path(args.probe).resolve()
    rng = random.Random(469190)
    cases = make_span_cases(rng, args.cases)
    failures = []
    total_spans = 0
    for index, case in enumerate(cases):
        result = run_span_case(pe, probe, case, index + 1)
        total_spans += result['spans']
        if result['problems']:
            failures.append(dict(case=index, kind=result['kind'], problems=result['problems'][:6]))
    direct = check_direct_cases(pe, probe, max(90,args.cases))
    failures.extend(dict(kind="direct",**item) for item in direct["failures"])
    persistence = persistence_case(pe, probe)
    if persistence['problems']:
        failures.append(dict(case='persistence', kind='persistence', problems=persistence['problems'][:6]))
    output = json.dumps(dict(source_sha256=SOURCE_SHA256, cases=len(cases), spans=total_spans,
                             persistence=persistence, direct_cases=direct["cases"], failures=failures,
                             scope='original 0x46c390/0x46c3e0/0x46cac0/0x46cc30/0x46d190 with the '
                                   'complete 32-bit 0x4683b0 selector and region kernels; span plan fields, '
                                   'colour and depth planes, draw counters, selector key and kernel, '
                                   'unused one-pixel step independence'), indent=2)
    print(output)
    if args.report:
        args.report.write_text(output + '\n')
    raise SystemExit(bool(failures))


if __name__ == '__main__':
    main()
