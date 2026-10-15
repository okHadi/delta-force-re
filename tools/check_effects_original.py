#!/usr/bin/env python3
"""Replay original DF1 effect routing, effect pixel kernels and drain order.

Routing replays mesh dispatch 0x46b1d0 with effect bits and reads the four
original queues. Pixel checks replay the 32-bit kernel table 0x4686d0 and
selector 0x4683b0, then setup 0x4672a0 with spans 0x4672b0, 0x467ff0 and
0x4682a0. Drain order and parser rejection use original code bytes.
KABOOM.3DI and MC.3DI have no original reader, so their decodes only get
file invariant checks.
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess

SOURCE_SHA256 = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'

TARGETS = {
    'pixel_table_init': 0x4686d0,
    'pixel_selector': 0x4683b0,
    'setup_context': 0x4672a0,
    'span_no_write': 0x4672b0,
    'span_alpha_blend': 0x467ff0,
    'span_half_blend': 0x4682a0,
    'drain_mixed': 0x46be50,
    'drain_no_context': 0x46bf60,
    'drain_effect': 0x46bf10,
    'span_builder': 0x46cac0,
    'triangle_dispatch': 0x462df0,
    'flush_span_one': 0x46cc30,
    'flush_span_two': 0x46d190,
    'mesh_dispatch': 0x46b1d0,
}
SELECTION_SPANS = {0x4672b0: 'no_write', 0x467ff0: 'alpha_blend', 0x4682a0: 'half_blend'}


def load(executable):
    import pefile
    source = Path(executable).read_bytes()
    digest = hashlib.sha256(source).hexdigest()
    if digest != SOURCE_SHA256:
        raise ValueError('Unsupported original executable hash')
    return source, pefile.PE(data=source)


def machine_for(image):
    import unicorn
    u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    u.mem_map(0x400000, 0x1000000)
    u.mem_write(0x400000, image)
    u.mem_map(0x2000000, 0x10000)
    u.mem_map(0x3000000, 0x40000)
    u.mem_map(0x18000000, 0x10000)
    return u


def caller(u, image, counter):
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


def probe_lines(probe, command, lines):
    text = ''.join(' '.join(str(v) for v in line) + '\n' for line in lines)
    output = subprocess.check_output([str(probe), command], input=text.encode()).decode().splitlines()
    if len(output) != len(lines):
        raise ValueError(f'native {command} count differs from input')
    return output


def check_span32(source, pe, probe, count):
    import unicorn
    u = machine_for(pe.get_memory_mapped_image())
    call = caller(u, pe.get_memory_mapped_image(), count)
    call(TARGETS['pixel_table_init'], 0, 1, 32)  # 32 bpp, textured kernels
    rng = random.Random(6803)
    selections = {0x104: 'no_write', 0x214: 'alpha_blend', 0x414: 'half_blend'}
    cases = []
    for test in range(count):
        face_flags = list(selections)[test % len(selections)]
        bx, by = rng.randrange(9), rng.randrange(9)
        length = rng.randrange(0, 65)
        pixel = rng.randrange(0, 512 - length)
        cases.append((test, face_flags, bx, by, pixel, length,
                      rng.randrange(-0x80000000, 0x80000000), rng.randrange(-0x80000000, 0x80000000),
                      rng.randrange(-0x80000000, 0x80000000), rng.randrange(0x1000000),
                      rng.randrange(-0x80000000, 0x80000000), rng.randrange(-0x80000000, 0x80000000),
                      rng.randrange(-0x80000000, 0x80000000), rng.randrange(-0x80000000, 0x80000000)))
    native = probe_lines(probe, 'span32', cases)
    mapping = probe_lines(probe, 'span-map', [(0x104,), (0x214,), (0x414,)])
    seen = {}

    for case, line in zip(cases, native):
        test, face_flags, bx, by, pixel, length, su, sv, sz, shade, du, dv, ddz, dshade = case
        u.mem_write(0x3000000, bytes((i * 37 + test * 13) & 255 for i in range(1 << (bx + by))))
        u.mem_write(0x3021000, b''.join(struct.pack('<I', (i * 1234567 ^ 0xabcdef90) & 0xffffffff) for i in range(512)))
        u.mem_write(0x3020000, b''.join(struct.pack('<H', (i * 317 + test * 17) & 0xffff) for i in range(512)))
        u.mem_write(0x3010000, b''.join(struct.pack('<I', (i * 0x01010101 ^ 0xffa5c3e7) & 0xffffffff) for i in range(256)))
        context = bytearray(0x34)
        struct.pack_into('<I', context, 0x00, 0x00112233 ^ (test * 0x040404))
        struct.pack_into('<I', context, 0x0c, (1 << bx) - 1)
        struct.pack_into('<I', context, 0x10, (1 << by) - 1)
        struct.pack_into('<I', context, 0x14, bx)
        struct.pack_into('<I', context, 0x1c, 0x3020000)
        struct.pack_into('<I', context, 0x20, 0x3021000)
        struct.pack_into('<I', context, 0x24, 0x3000000)
        struct.pack_into('<I', context, 0x28, 0x3010000 - 0x8000)
        u.mem_write(0x2001000, bytes(context))
        u.mem_write(0x2002000, struct.pack('<10i', pixel, length, su, sv, sz, shade, du, dv, ddz, dshade))
        u.mem_write(0x2003000, b'\0' * 8)
        call(TARGETS['pixel_selector'], face_flags, 0x2003000, 0x2003004)
        setup, span = struct.unpack('<2I', u.mem_read(0x2003000, 8))
        if setup != TARGETS['setup_context']:
            raise ValueError(f'selector setup differs at case {test}: {setup:#x}')
        if span not in SELECTION_SPANS:
            raise ValueError(f'selector span differs at case {test}: {span:#x}')
        seen[face_flags] = SELECTION_SPANS[span]
        call(setup, 0x2001000)
        call(span, 0x2002000)
        colours = struct.unpack('<512I', u.mem_read(0x3021000, 2048))
        depths = struct.unpack('<512H', u.mem_read(0x3020000, 1024))
        expected = [value for pair in zip(colours, depths) for value in pair]
        called = list(map(int, line.split()))
        if called != expected:
            raise ValueError(f'effect span mismatch at case {test}, flags {face_flags:#x}')
    for face_flags, name in ((0x104, mapping[0]), (0x214, mapping[1]), (0x414, mapping[2])):
        if seen.get(face_flags) != name:
            raise ValueError(f'native selection differs from original for {face_flags:#x}')
    # Unhandled keys keep the previous selection in the original.
    call(TARGETS['pixel_selector'], 0x414, 0x2003000, 0x2003004)
    call(TARGETS['pixel_selector'], 0x114, 0x2003000, 0x2003004)
    setup, span = struct.unpack('<2I', u.mem_read(0x2003000, 8))
    if (setup, span) != (TARGETS['setup_context'], TARGETS['span_half_blend']):
        raise ValueError('unhandled effect key no longer keeps the previous kernels')
    code = pe.get_data(TARGETS['pixel_table_init'] - pe.OPTIONAL_HEADER.ImageBase, 0x1000)
    return dict(cases=count, mismatches=0, target_pixels=count * 1024, selections=seen,
                selector_function='0x4683b0', init_function='0x4686d0',
                kernels=['0x4672b0', '0x467ff0', '0x4682a0'], setup='0x4672a0',
                code_sha256=hashlib.sha256(code).hexdigest(),
                scope='32 bpp textured kernel table; 64x8 target; texture bits 0-8; random signed UV, depth and steps; full colour and depth planes')


def check_route(source, pe, probe, count):
    import unicorn
    u = machine_for(pe.get_memory_mapped_image())
    call = caller(u, pe.get_memory_mapped_image(), count)
    call(0x477510)  # sine table
    call(TARGETS['pixel_table_init'], 0, 1, 32)
    camera = bytearray(80)
    struct.pack_into('<8I', camera, 0x20, 0x3021000, 32, 24, 128, 0, 0, 31, 23)
    struct.pack_into('<2I', camera, 0x48, 90, 32)
    u.mem_write(0x18007000, bytes(camera))
    call(0x44a820, 0x8cfa20, 0x18007000)
    for address, value in ((0x8ee1e0, 0x8cfa20), (0x8ee1e8, 0)):
        u.mem_write(address, struct.pack('<I', value))
    for y in range(24):
        u.mem_write(0x8cfa20 + 0x68 + y * 4, struct.pack('<I', 0x3021000 + y * 32 * 4))
        u.mem_write(0x8cfa20 + 0xc68 + y * 4, struct.pack('<I', 0x3020000 + y * 32 * 2))
    u.mem_write(0xbfc104, struct.pack('<I', 0x3010000))
    u.mem_write(0xbfc108, struct.pack('<I', 0x3024000))
    u.mem_write(0x3010000, b''.join(struct.pack('<I', (i * 0x01010101 ^ 0xffa5c3e7) & 0xffffffff) for i in range(64 * 256)))
    identity = [4194304, 0, 0, 0, 0, 4194304, 0, 0, 0, 0, 4194304, 0, 0, 0, 0, 0]
    u.mem_write(0x18003000, struct.pack('<16i', *identity))
    points = [[512, 128, 128], [512, 128, -128], [512, -128, 128]]
    u.mem_write(0x18004000, b''.join(struct.pack('<4h', *p, 0) for p in points))
    u.mem_write(0x18005000, b''.join(struct.pack('<4h', 0, 0, 0, 0) for _ in range(3)))
    face = bytearray(72)
    struct.pack_into('<HH6i6h', face, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 1, 2)
    struct.pack_into('<I', face, 68, 0x18006000)
    u.mem_write(0x18002000, bytes(face))
    u.mem_write(0x18001000, struct.pack('<7I', 0, 3, 0x18004000, 1, 0x18002000, 3, 0x18005000) + bytes(84))
    # Material flags used by the survey: effect bits and the special bit.
    material_flags = [1 | 2, 0x9, 0x81, 0x8001, 0x401, 0x409, 0x2001, 0x4081]
    queue_counters = {'deferred': 0xad3d24, 'deferred_effect': 0xad3dac,
                      'effect_plain': 0xad3d20, 'deferred_no_context': 0xad3dbc}
    queue_bases = {'deferred': 0xa1a42c, 'deferred_effect': 0xa184ec,
                   'effect_plain': 0xa165ac, 'deferred_no_context': 0xaa8d6c}
    rows = []
    cases = []
    for test in range(count):
        material = material_flags[test % len(material_flags)]
        mode = (test // len(material_flags)) & 1
        enabled = (test // (2 * len(material_flags))) & 1
        expected_flags = 4
        if material & 1: expected_flags |= 0x10
        if material & 2: expected_flags |= 0x40
        if material & 0x200 and not material & 0x8000: expected_flags |= 0x20
        if material & 8: expected_flags |= 0x100
        if material & 0x8000: expected_flags |= 0x200
        if material & 0x80: expected_flags |= 0x400
        cases.append((expected_flags, material, mode, enabled))
    native = probe_lines(probe, 'route', cases)
    hits = {name: 0 for name in queue_counters}
    direct = 0
    for number, ((expected_flags, material, mode, enabled), line) in enumerate(zip(cases, native)):
        material_bytes = bytearray(76)
        struct.pack_into('<I', material_bytes, 16, material)
        struct.pack_into('<4H', material_bytes, 32, 0, 0, 1, 1)
        struct.pack_into('<I', material_bytes, 68, 0x3000000)
        u.mem_write(0x18006000, bytes(material_bytes))
        u.mem_write(0x18002000, bytes(face))
        for address, value in [(0xad3d2c, 0), (0xad3d7c, 0 if enabled else 1), (0xad3dc4, 0),
                               (0xad3d28, mode), (0xad3d78, 0), (0xad3d74, 0)]:
            u.mem_write(address, struct.pack('<I', value))
        for counter in queue_counters.values():
            u.mem_write(counter, struct.pack('<I', 0))
        call(TARGETS['mesh_dispatch'], 0x18001000, 0x18003000, 0x3e, 0)
        observed = None
        record_flags = None
        for name, counter in queue_counters.items():
            value = struct.unpack('<I', u.mem_read(counter, 4))[0]
            if value:
                if observed is not None:
                    raise ValueError(f'face reaches two queues at case {number}')
                observed = name
                entry = struct.unpack('<I', u.mem_read(queue_bases[name] + 4, 4))[0]
                record_flags = struct.unpack('<I', u.mem_read(entry, 4))[0]
                hits[name] += 1
        drawn = struct.unpack('<I', u.mem_read(0xad3d78, 4))[0] > 0
        if drawn:
            direct += 1
        if not enabled:
            expected_route = 'skip'
        elif not expected_flags & 0x700 and not mode:
            expected_route = 'draw'
        elif not mode:
            expected_route = 'deferred_no_context'
        elif material & 0x400:
            expected_route = 'effect_plain'
        elif expected_flags & 0x700:
            expected_route = 'deferred_effect'
        else:
            expected_route = 'deferred'
        if expected_route in ('draw', 'skip'):
            if observed is not None:
                raise ValueError(f'route {expected_route} queued a face at case {number}')
        elif observed != expected_route:
            raise ValueError(f'route mismatch at case {number}: original {observed}, expected {expected_route}')
        if observed and (record_flags & 0x700) != (expected_flags & 0x700):
            raise ValueError(f'queued face effect bits differ at case {number}')
        native_route = line.split()[0]
        if native_route != expected_route:
            raise ValueError(f'native route differs at case {number}: {native_route} against {expected_route}')
        if observed is not None and (record_flags & 0x700) != int(line.split()[2], 16):
            raise ValueError(f'native effect bits differ at case {number}')
        rows.append(expected_route)
    return dict(cases=count, mismatches=0, queue_hits=hits, direct_draws=direct,
                routes=sorted(set(rows)), dispatch='0x46b1d0',
                scope='effect and plain faces across effect mode, material bit 0x400 and draw enable; queue identity, queued flags and material record fields')


def calls_in(pe, start, end):
    import capstone
    base = pe.OPTIONAL_HEADER.ImageBase
    code = pe.get_data(start - base, end - start)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    targets = []
    for insn in md.disasm(code, start):
        if insn.mnemonic == 'call' and insn.op_str.startswith('0x'):
            targets.append(int(insn.op_str, 16))
    return targets, code


def check_drain(pe, probe):
    mixed, mixed_code = calls_in(pe, 0x46be50, 0x46bf0c)
    effect, effect_code = calls_in(pe, 0x46bf10, 0x46bf4d)
    plain, plain_code = calls_in(pe, 0x46bf60, 0x46bf9d)
    expected_mixed = [TARGETS['triangle_dispatch'], TARGETS['span_builder'],
                      TARGETS['flush_span_one'], TARGETS['flush_span_two'], TARGETS['triangle_dispatch']]
    if mixed != expected_mixed:
        raise ValueError(f'0x46be50 call order differs: {[hex(v) for v in mixed]}')
    if effect != [TARGETS['triangle_dispatch']] or plain != [TARGETS['triangle_dispatch']]:
        raise ValueError('plain drain functions differ from the expected single dispatch call')
    frame, _ = calls_in(pe, 0x485610, 0x485720)
    order = [value for value in frame if value in (TARGETS['drain_mixed'], TARGETS['drain_no_context'], TARGETS['drain_effect'])]
    if order != [TARGETS['drain_mixed'], TARGETS['drain_no_context'], TARGETS['drain_effect']]:
        raise ValueError(f'world frame drain order differs: {[hex(v) for v in order]}')
    # 0x46bb20 runs at the start of the effect pass and clears every queue
    # counter and the face record cursor.
    import capstone
    base = pe.OPTIONAL_HEADER.ImageBase
    reset_code = pe.get_data(0x46bb20 - base, 0xae)
    reset_text = '\n'.join(f'{insn.mnemonic} {insn.op_str}' for insn in capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32).disasm(reset_code, 0x46bb20))
    for counter in ('0xad3d20', '0xad3d24', '0xad3dac', '0xad3dbc', '0xad3dc4'):
        if f'mov dword ptr [{counter}], ecx' not in reset_text:
            raise ValueError(f'frame reset for {counter} differs from 0x46bb20')
    reset_world, world_code = calls_in(pe, 0x485610, 0x4856f8)
    if 0x485420 not in reset_world:
        raise ValueError('effect pass 0x485420 is not called by the world frame')
    special, _ = calls_in(pe, 0x485420, 0x4854d0)
    if 0x46bb20 not in special:
        raise ValueError('effect pass start 0x46bb20 is not called by 0x485420')
    cases = [(0x4, 0, 1), (0x104, 0, 1), (0x4, 0, 1), (0x104, 0, 0), (0x4, 0x400, 1), (0x4, 0, 1)]
    lines = subprocess.check_output([str(probe), 'plan'],
        input=''.join(' '.join(str(v) for v in case) + '\n' for case in cases).encode()).decode().splitlines()
    deferred = [line.split() for line in lines if line.startswith('op ')]
    flushes = [int(line.split()[1]) for line in lines if line.startswith('flush ')]
    plain_lines = [line.split() for line in lines if line.startswith('plain ')]
    if flushes != [2]:
        raise ValueError('native drain flush count differs from the two original flush calls')
    # Probe cases in order: (0x4,0,1) A, (0x104,0,1) B, (0x4,0,1) A,
    # (0x104,0,0) D, (0x4,0x400,1) C, (0x4,0,1) A.
    expected_ops = [('plain', '4'), ('deferred_span', '4'), ('deferred_span', '4'),
                    ('deferred_span', '4')]
    if [(line[1], line[2]) for line in deferred] != expected_ops:
        raise ValueError(f'native deferred plan differs: {deferred}')
    # Queue C drains first inside 0x46be50 with the plain path; 0x46bf60
    # (no context) runs before 0x46bf10 (effect).
    if [flags for _, flags in plain_lines] != ['104', '104']:
        raise ValueError(f'native plain plan differs: {plain_lines}')
    return dict(mismatches=0, mixed_calls=[hex(v) for v in mixed],
                mixed_code_sha256=hashlib.sha256(mixed_code).hexdigest(),
                effect_calls=[hex(v) for v in effect], plain_calls=[hex(v) for v in plain],
                world_drain_order=[hex(v) for v in order],
                frame_reset='0x46bb20 clears queue counters and the face record cursor',
                scope='0x46be50, 0x46bf10 and 0x46bf60 call order; 0x485610 frame order; 0x46bb20 reset site; native drain plan order')


def check_parser_reject(pe, kaboom_path, mc_path):
    base = pe.OPTIONAL_HEADER.ImageBase
    block = pe.get_data(0x4863ae - base, 0x28)
    expected = bytes.fromhex('80fc330f85e6000000807b01440f85dc000000807b02490f85d2000000807b03050f82c8000000')
    if block[:len(expected)] != expected:
        raise ValueError('0x486370 version check bytes differ from the documented block')
    sources = {}
    for name, path in (('KABOOM.3DI', kaboom_path), ('MC.3DI', mc_path)):
        if path is None:
            continue
        data = Path(path).read_bytes()
        magic_ok = data[:3] == b'3DI'
        version = data[3] if len(data) > 3 else 0
        sources[name] = dict(sha256=hashlib.sha256(data).hexdigest(), size=len(data),
                             magic_ok=magic_ok, version=version, accepted=magic_ok and 0x05 <= version)
    return dict(check_address='0x4863ae-0x4863d5', check_sha256=hashlib.sha256(block).hexdigest(),
                sources=sources,
                scope='only 3DI magic with version >= 5 is accepted; both files fail that check')


def check_sprite(probe, sprite_path, prefix_path):
    def run(command, path):
        output = subprocess.check_output([str(probe), command, str(path)]).decode().splitlines()
        fields = {}
        for line in output:
            keys = line.split()
            fields.setdefault(keys[0], []).append(keys[1:])
        return fields
    result = {}
    if sprite_path:
        fields = run('sprite', sprite_path)
        texture = fields['texture'][0]
        if fields['magic'] != [['kaboom']]:
            raise ValueError('sprite magic differs')
        if texture[0] != 'SPARKS08' or int(texture[3]) * int(texture[4]) != int(texture[5]):
            raise ValueError('sprite texture record differs')
        materials = [(m[0], int(m[1], 16)) for m in fields['material']]
        if materials != [('FLARE', 0x4083), ('FLASH', 0x4008)]:
            raise ValueError(f'sprite trailer records differ: {materials}')
        data = Path(sprite_path).read_bytes()
        unknown = [int(v) for v in fields['unknown'][0]]
        if unknown[1] - unknown[0] != 5928 - 5264 or len(data) != 6080:
            raise ValueError('sprite unknown range differs')
        result['sprite'] = dict(sha256=hashlib.sha256(data).hexdigest(),
                                texture=texture, materials=materials, unknown=unknown)
    if prefix_path:
        fields = run('prefix', prefix_path)
        data = Path(prefix_path).read_bytes()
        counts = [int(v) for v in fields['counts'][0]]
        if fields['version'] != [['3']] or int(fields['truncated'][0][0]) != 1:
            raise ValueError('model prefix version or truncation differs')
        if int(fields['declared_lod_size'][0][0]) != 16172:
            raise ValueError('model prefix declared size differs')
        if int(fields['available_lod_bytes'][0][0]) != 3760:
            raise ValueError('model prefix available bytes differ')
        if len(fields['texture']) != 8 or int(fields['texture_count'][0][0]) != 8:
            raise ValueError('model prefix texture count differs')
        result['prefix'] = dict(sha256=hashlib.sha256(data).hexdigest(), version=fields['version'][0][0],
                                declared=int(fields['declared_lod_size'][0][0]),
                                available=int(fields['available_lod_bytes'][0][0]), counts=counts,
                                textures=len(fields['texture']))
    result['scope'] = 'file invariant checks only: the original executable has no reader for either file'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable')
    parser.add_argument('--probe', required=True)
    parser.add_argument('--cases', type=int, default=60)
    parser.add_argument('--sprite', type=Path)
    parser.add_argument('--prefix', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    source, pe = load(args.executable)
    probe = Path(args.probe).resolve()
    result = dict(source_sha256=SOURCE_SHA256,
                  routing=check_route(source, pe, probe, args.cases),
                  span32=check_span32(source, pe, probe, args.cases),
                  drain=check_drain(pe, probe),
                  parser=check_parser_reject(pe, args.sprite, args.prefix),
                  files=check_sprite(probe, args.sprite, args.prefix))
    output = json.dumps(result, indent=2)
    print(output)
    if args.report:
        args.report.write_text(output + '\n')


if __name__ == '__main__':
    main()
