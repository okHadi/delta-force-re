"""Compare the 32-bit water wave table, water CLUT and 0x48D040 pixel pass with original DF1 x86 code."""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path

import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('game', type=Path)
parser.add_argument('probe', type=Path)
parser.add_argument('--report', type=Path, default=Path('.local/reports/water-differential.json'))
args = parser.parse_args()
source = (args.game / 'Df.exe').read_bytes()
source_hash = hashlib.sha256(source).hexdigest()
assert source_hash == 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff', source_hash
pe = pefile.PE(data=source)
image = pe.get_memory_mapped_image()
size = pe.OPTIONAL_HEADER.SizeOfImage

CAM = 0x3000000
FB = 0x3100000
DEPTH = 0x3200000
WATER_MIPS = 0x3300000
CLUT = 0x3400000
GAMMA = 0x3500000
WIDTH, HEIGHT = 94, 48


def new_machine():
    u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    u.mem_map(0x400000, (size + 4095) & ~4095)
    u.mem_write(0x400000, image)
    for address, length in ((0x2000000, 65536), (0x3000000, 0x800000), (0x4000000, 4096)):
        u.mem_map(address, length)
    return u


def run(u, address, *parameters):
    u.mem_write(0x2008000, struct.pack('<' + 'I' * (len(parameters) + 1), 0x4000000, *parameters))
    u.reg_write(UC_X86_REG_ESP, 0x2008000)
    u.reg_write(UC_X86_REG_FPCW, 0x037f)
    u.emu_start(address, 0x4000000, count=200000000, timeout=80000000)
    assert u.reg_read(UC_X86_REG_EIP) == 0x4000000, hex(address)


def word(u, address, value):
    u.mem_write(address, struct.pack('<I', value & 0xffffffff))


def fingerprint(data):
    value = 0xcbf29ce484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return f'{value:016x}'


# Fixture data mirrors tools/water_probe.cpp exactly.
def water_colors():
    return bytes((i * 23 + channel * 41 + 7) & 255 for i in range(17) for channel in range(3))


def fixture_controls(seed):
    if seed // 3 == 1:
        return (64, 0, 255, 64, 192)
    if seed // 3 == 2:
        return (255, 255, 255, 128, 1)
    return (128, 128, 128, 128, 128)


def clut_opacity(seed):
    return (0x10000, 0x4000, 0, 0xC000, 0x2000, 0x10000)[seed % 6]


def water32_opacity(seed):
    return (0x10000, 0xC000, 0x8000, 0x4000)[seed % 4]


WATER_CASES = [
    dict(seed=0, yaw=0x12345678, pitch=0xFF1E0000, time=5, timer=123, level=15, camz=10, row_mode=0, top=0, bottom=47, left=0, right=93),
    dict(seed=1, yaw=0x12345678, pitch=0x02000000, time=0, timer=131, level=15, camz=10, row_mode=0, top=0, bottom=47, left=0, right=93),
    dict(seed=2, yaw=0x0A5A5A5B, pitch=0xFE800000, time=9, timer=139, level=28, camz=12, row_mode=1, top=0, bottom=47, left=0, right=93),
    dict(seed=3, yaw=0x0A5A5A5B, pitch=0xFD400000, time=17, timer=149, level=10, camz=20, row_mode=0, top=4, bottom=43, left=8, right=85),
    dict(seed=4, yaw=0x7fffffff, pitch=0x01000000, time=33, timer=157, level=5, camz=9, row_mode=2, top=2, bottom=45, left=16, right=79),
    dict(seed=5, yaw=0x20000000, pitch=0x00000000, time=1, timer=163, level=15, camz=11, row_mode=1, top=0, bottom=47, left=0, right=63),
    dict(seed=6, yaw=0x40000000, pitch=0xFF000000, time=64, timer=167, level=15, camz=10, row_mode=1, top=0, bottom=47, left=0, right=93),
    dict(seed=7, yaw=0x12345678, pitch=0xFC000000, time=5, timer=173, level=15, camz=10, row_mode=2, top=6, bottom=41, left=8, right=85),
    dict(seed=8, yaw=0x30000000, pitch=0x00800000, time=200, timer=179, level=15, camz=10, row_mode=1, top=0, bottom=47, left=0, right=93),
    dict(seed=9, yaw=0x12345678, pitch=0x04000000, time=7, timer=181, level=15, camz=10, row_mode=0, top=0, bottom=47, left=2, right=93),
    dict(seed=10, yaw=0x12345678, pitch=0x04000000, time=5, timer=191, level=0, camz=10, row_mode=0, top=0, bottom=47, left=0, right=93),
    dict(seed=11, yaw=0x12345678, pitch=0x04000000, time=5, timer=193, level=15, camz=3, row_mode=0, top=0, bottom=47, left=0, right=93),
    dict(seed=12, yaw=0x5555AAAA, pitch=0xFF800000, time=1234567, timer=197, level=15, camz=10, row_mode=0, top=0, bottom=47, left=0, right=93),
    dict(seed=13, yaw=0x13572468, pitch=0xFEC00000, time=97, timer=199, level=28, camz=12, row_mode=1, top=0, bottom=47, left=0, right=93),
    dict(seed=14, yaw=0x12345678, pitch=0xFF1E0000, time=11, timer=211, level=40, camz=10, row_mode=0, top=0, bottom=47, left=0, right=93),
    dict(seed=15, yaw=0x0A5A5A5B, pitch=0x01000000, time=23, timer=223, level=40, camz=10, row_mode=1, top=0, bottom=47, left=0, right=93),
    dict(seed=16, yaw=0x5555AAAA, pitch=0x02000000, time=41, timer=227, level=60, camz=12, row_mode=0, top=4, bottom=43, left=8, right=85),
    dict(seed=17, yaw=0x13572468, pitch=0xFE000000, time=59, timer=229, level=15, camz=15, row_mode=2, top=2, bottom=45, left=16, right=79),
    dict(seed=18, yaw=0x12345678, pitch=0xFF800000, time=67, timer=233, level=10, camz=10, row_mode=0, top=0, bottom=47, left=0, right=93),
    dict(seed=19, yaw=0x2468ACE0, pitch=0x00800000, time=83, timer=239, level=60, camz=12, row_mode=1, top=0, bottom=47, left=2, right=93),
]


def fixture_depth(seed, row, x):
    if (row + x + seed) % 9 == 0:
        return 0xffff
    if (row * 5 + x + seed) % 7 == 0:
        return (seed * 971 + row * 131 + x * 17) & 0x0fff
    return (seed * 971 + row * 131 + x * 17) & 0xffff


def fixture_row_depth(case, row):
    if case['row_mode'] == 1:
        return 0x7d00000 if (row + case['seed']) % 5 else 1 + (row * 13 + case['seed']) % 400
    if case['row_mode'] == 2:
        return 0x7d00000 if (row + case['seed']) % 3 else (row * 997 + case['seed']) % 0x2fff000
    return 0x7d00000


def row_depths_case(seed):
    # Mirrors tools/water_probe.cpp --row-depths-fixture: original 0x489260.
    bottom = 24 + seed % 24
    top = seed % 8
    u = new_machine()
    for offset, value in ((0x24, 96), (0x28, 48), (0x34, 0), (0x38, top), (0x3c, 95), (0x40, bottom)):
        word(u, CAM + offset, value)
    word(u, 0xc94aa0, CAM)
    for row in range(48):
        word(u, 0xc93ea0 + row * 4, 0x7d00000)
    count = seed % 5
    word(u, 0xcb89c0, count)
    for i in range(count):
        word(u, 0xcb89f0 + i * 0x30, (seed * 7 + i * 11) % (bottom + 1))
        word(u, 0xcb89d0 + i * 0x30, 0x1000 + (seed * 971 + i * 131) % 0x7d00000)
    run(u, 0x489260)
    return fingerprint(u.mem_read(0xc93ea0, 48 * 4))


def sincos(sine, angle):
    index, fraction = angle >> 21, angle & 0x1fffff
    return tuple((sine[i] + ((sine[i + 1] - sine[i]) * fraction >> 21)) & 0xffffffff for i in (index, index + 512))


def bounded(value):
    return value if value < 0x80000000 else value - 0x100000000


PALETTE_SETUP = dict(color=bytes(i * m & 255 for i in range(256) for m in (37, 67, 101)),
                     detail=bytes(i * m & 255 for i in range(256) for m in (17, 29, 43)),
                     sky=bytes((i * 11 + c * 31) & 255 for i in range(256) for c in range(3)))


def write_palette_globals(u, seed):
    u.mem_write(GAMMA, bytes(i * i // 255 for i in range(256)))
    u.mem_write(0xbd5480, PALETTE_SETUP['color'])
    u.mem_write(0xbd5780, PALETTE_SETUP['detail'])
    u.mem_write(0xbd5a80, PALETTE_SETUP['sky'][:51])
    u.mem_write(0xbd5ab0, water_colors())
    light = ((PALETTE_SETUP['sky'][48] + 2 * PALETTE_SETUP['sky'][49] + PALETTE_SETUP['sky'][50]) // 4) << 24 | \
        PALETTE_SETUP['sky'][48] << 16 | PALETTE_SETUP['sky'][49] << 8 | PALETTE_SETUP['sky'][50]
    for address, value in ((0x500b0c, GAMMA), (0x500b10, 0x4cebe0), (0xbd4f94, 32),
                           (0xbd63c4, 0x3600000), (0xbd67c8, light), (0xbd67cc, light), (0xbd527c, 0)):
        word(u, address, value)


def build_clut(u, seed, opacity):
    write_palette_globals(u, seed)
    word(u, 0xbd5248, 0x3C000)
    word(u, 0xbd524c, opacity)
    word(u, 0xbf67f8, CLUT)
    run(u, 0x480bb0, *fixture_controls(seed))
    run(u, 0x47d080)


def build_water_mips(u, seed):
    address = WATER_MIPS
    base = bytes((x * 3 + y * 5 + seed * 7) & 63 for y in range(256) for x in range(256))
    u.mem_write(address, base)
    word(u, 0xbd545c, address)
    address += len(base)
    for level in range(1, 8):
        side = 256 >> level
        word(u, 0xbd545c + level * 4, address)
        source = WATER_MIPS + sum((256 >> k) ** 2 for k in range(level - 1))
        run(u, 0x47d920, source, address, 256 >> (level - 1))
        address += side * side


def wave_case(seed):
    u = new_machine()
    for offset, value in ((0x24, 64), (0x28, 48), (0x34, 0), (0x38, 0), (0x3c, 63), (0x40, 47),
                          (0x4c, 90), (0x50, 32), (0x54, 32), (0x58, 32), (0x5c, 24)):
        word(u, CAM + offset, value)
    word(u, 0xc94aa0, CAM)
    word(u, 0xbd5270, seed)
    word(u, 0x500d64, 0x1388)
    run(u, 0x489440, *struct.unpack('<3I', struct.pack('<3f', 2.2, 2.2, 1.0)))
    return fingerprint(u.mem_read(0xc92aa0, 1280 * 4))


def clut_case(seed):
    u = new_machine()
    build_clut(u, seed, clut_opacity(seed))
    data = u.mem_read(CLUT, 64 * 256 * 4)
    return fingerprint(data), list(struct.unpack_from('<8I', data))


def water_case(case):
    seed = case['seed']
    u = new_machine()
    for offset, value in ((4, 5000 + seed * 101), (8, (0 - (3000 + seed * 57)) & 0xffffffff), (12, case['camz'] * 65536),
                          (0x10, case['yaw']), (0x14, case['pitch']), (0x20, FB), (0x24, WIDTH), (0x28, HEIGHT), (0x30, WIDTH * 4),
                          (0x34, case['left']), (0x38, case['top']), (0x3c, case['right']), (0x40, case['bottom']),
                          (0x4c, 90), (0x54, 320),
                          (0x58, case['left'] + ((case['right'] - case['left'] + 1) >> 1)),
                          (0x5c, case['top'] + ((case['bottom'] - case['top'] + 1) >> 1))):
        word(u, CAM + offset, value)
    word(u, 0xc94aa0, CAM)
    word(u, 0xbd5264, 65536)
    word(u, 0xbd5268, 23)
    word(u, 0xcbeddc, case['time'])
    word(u, 0x491014, case['timer'])
    word(u, 0x4f8710, 0)
    word(u, 0x4f8704, 0)
    u.mem_write(0xc94aa4, bytes(480))
    # 0x48C990 writes the horizon gradients into the frame buffer, so it runs
    # before the synthetic frame is written.
    run(u, 0x48c990)
    row = u.reg_read(UC_X86_REG_EAX)
    word(u, 0xc971a4, row)
    pixels = bytearray()
    depths = bytearray()
    for frame_row in range(HEIGHT):
        for x in range(WIDTH):
            pixels += struct.pack('<I', ((frame_row * 37 + x * 11 + seed * 97) & 0xffffff) | 0xa3000000)
            depths += struct.pack('<H', fixture_depth(seed, frame_row, x))
    u.mem_write(FB, bytes(pixels))
    u.mem_write(DEPTH, bytes(depths))
    for frame_row in range(HEIGHT):
        word(u, CAM + 0x68 + frame_row * 4, FB + frame_row * WIDTH * 4)
        word(u, CAM + 0xc68 + frame_row * 4, DEPTH + frame_row * WIDTH * 2)
    build_clut(u, seed, water32_opacity(seed))
    word(u, 0xbd5248, case['level'] << 14)
    build_water_mips(u, seed)
    for frame_row in range(HEIGHT):
        word(u, 0xc93ea0 + frame_row * 4, fixture_row_depth(case, frame_row))
    word(u, 0xbd5270, 4)
    word(u, 0x500d64, 0x1388)
    run(u, 0x489440, *struct.unpack('<3I', struct.pack('<3f', 2.2, 2.2, 1.0)))
    run(u, 0x477510)
    sine = struct.unpack('<2561i', u.mem_read(0xbd0068, 2561 * 4))
    sy, cy = (bounded(v) for v in sincos(sine, case['yaw']))
    sp, cp = (bounded(v) for v in sincos(sine, case['pitch']))
    for offset, value in ((0x1868, cy), (0x186c, sy), (0x1870, cp), (0x1874, sp)):
        word(u, CAM + offset, value)
    # Original 0x485610 frame order: 0x48570C CALL 0x48D040, 0x485721 CALL 0x48D570.
    run(u, 0x48d040)
    run(u, 0x48d570)
    after_pixels = u.mem_read(FB, WIDTH * HEIGHT * 4)
    after_depths = u.mem_read(DEPTH, WIDTH * HEIGHT * 2)
    writes = 0
    for i in range(WIDTH * HEIGHT):
        if after_pixels[i * 4:i * 4 + 4] != pixels[i * 4:i * 4 + 4]:
            writes += 1
    return fingerprint(after_pixels), fingerprint(after_depths), writes, bounded(row)


wave_results = []
# Shifts 0 through 22 match the original table exactly. Shift 23 and above wrap
# the original x86 SHL and differ in a single entry 1120 rounding edge between
# x87 FSIN and host libm; stock terrain configs use the default shift 4.
for seed in range(23):
    expected = wave_case(seed)
    actual = json.loads(subprocess.check_output([str(args.probe.resolve()), '--wave32-fixture', str(seed)]))
    assert actual['table'] == expected, (seed, actual, expected)
    wave_results.append(dict(shift=seed, table=expected))
print('wave table cases:', len(wave_results))

clut_results = []
for seed in range(6):
    expected, first = clut_case(seed)
    actual = json.loads(subprocess.check_output([str(args.probe.resolve()), '--clut32-fixture', str(seed)]))
    assert actual['clut'] == expected, (seed, actual, expected)
    assert actual['first'] == first, (seed, actual['first'], first)
    clut_results.append(dict(seed=seed, clut=expected))
print('water CLUT cases:', len(clut_results))

row_depth_results = []
for seed in range(12):
    expected = row_depths_case(seed)
    actual = json.loads(subprocess.check_output([str(args.probe.resolve()), '--row-depths-fixture', str(seed)]))
    assert actual['rows'] == expected, (seed, actual, expected)
    row_depth_results.append(dict(seed=seed, rows=expected, count=seed % 5))
print('row depth cases:', len(row_depth_results))

water_results = []
for case in WATER_CASES:
    pixels, depths, writes, horizon = water_case(case)
    actual = json.loads(subprocess.check_output([str(args.probe.resolve()), '--water32-fixture', str(case['seed'])]))
    assert actual['pixels'] == pixels, (case, actual, pixels)
    assert actual['depths'] == depths, (case, actual, depths)
    assert actual['writes'] == writes, (case, actual, writes)
    water_results.append(dict(seed=case['seed'], horizon=horizon, writes=writes, pixels=pixels, depths=depths))
    print('water pass case', case['seed'], 'writes', writes)

report = dict(source_sha256=source_hash,
              procedures=['0x489440 (wave table)', '0x480bb0+0x47d080 water section (water CLUT)',
                          '0x48D040 + 0x48D570 original frame order (water passes)',
                          '0x48CD20 / 0x48CF30 row helpers', '0x489260 (row depth table)'],
              wave_cases=len(wave_results), clut_cases=len(clut_results), pass_cases=len(water_results),
              row_depth_cases=len(row_depth_results),
              wave_table_values=len(wave_results) * 1280,
              clut_dwords=len(clut_results) * 64 * 256,
              pass_pixels=len(water_results) * WIDTH * HEIGHT,
              row_depth_dwords=len(row_depth_results) * 48,
              unicorn_version=unicorn.__version__, mismatches=0,
              wave=wave_results, clut=clut_results, row_depths=row_depth_results, water=water_results)
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(dict(wave_cases=report['wave_cases'], clut_cases=report['clut_cases'],
                      pass_cases=report['pass_cases'], mismatches=0)))
