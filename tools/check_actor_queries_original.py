#!/usr/bin/env python3
"""Compare NPC queries with hash-bound Df.exe instructions.

Run the query bodies, integer helpers, angle routine, and RNG. Supply sine,
terrain, sight, weapon-frame tables, and effect sinks as declared fixtures.
Compare all actor/person bytes, RNG, return values, and effect-time state.
"""
import argparse
import hashlib
import json
import random
import struct
import subprocess
from pathlib import Path
import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW

SHA = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
ACTOR, PERSON, DEFS, FRAMES, OUT, STACK, STOP = 0x7cca80, 0x794680, 0x2100000, 0x2200000, 0x2300000, 0x3008000, 0x3000000
SITES = [0x45f010, 0x45f090, 0x45fd90, 0x45fe30, 0x460410, 0x45e280, 0x45fb00, 0x45edd0, 0x45f2c0]
MASK = 0xffffffff

def i32(x):
    x &= MASK
    return x if x < 0x80000000 else x - 0x100000000

def w(image, offset, value):
    struct.pack_into('<I', image, offset, value & MASK)

def h(image, offset, value):
    struct.pack_into('<H', image, offset, value & 0xffff)

def r(image, offset):
    return struct.unpack_from('<I', image, offset)[0]

def sine(angle, mode):
    if mode:
        return ((angle * 1664525 + 1013904223) & 0x7fffff) - 4194304, ((angle * 22695477 + 1) & 0x7fffff) - 4194304
    s = [0, 4194304, 0, -4194304]
    return s[angle >> 30], s[((angle >> 30) + 1) & 3]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--cases', type=int, default=1800)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    raw = (args.game / 'Df.exe').read_bytes()
    if hashlib.sha256(raw).hexdigest() != SHA:
        raise SystemExit('Unsupported original executable hash')
    pe = pefile.PE(data=raw)
    u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    u.mem_map(0x400000, 0xa00000)
    u.mem_write(0x400000, pe.get_memory_mapped_image())
    u.mem_map(0x2000000, 0x400000)
    u.mem_map(STOP, 0x10000)
    def put(address, value):
        u.mem_write(address, struct.pack('<I', value & MASK))
    def word(address):
        return struct.unpack('<I', u.mem_read(address, 4))[0]
    randomizer = random.Random(20261017)
    cases = []
    for n in range(args.cases):
        site = SITES[n % len(SITES)]
        count = randomizer.randrange(2, 14)
        actors, people = [], []
        for i in range(count):
            a, p = bytearray(0x114), bytearray(0xb4)
            for offset, value in [(0, i + 1), (4, i + 1), (8, i + 13), (0xc, i), (0x24, randomizer.randrange(-0x400000, 0x400000)), (0x28, randomizer.randrange(-0x400000, 0x400000)), (0x2c, randomizer.randrange(-0x40000, 0x40000)), (0x30, randomizer.getrandbits(32)), (0x34, randomizer.getrandbits(32))]:
                w(a, offset, value)
            h(a, 0x70, randomizer.choice([0, 1, 1, 2, 2, 0xffff]))
            h(a, 0x72, randomizer.choice([0, 1, 3, 0xffff]))
            a[0x6c] = randomizer.choice([0, 0, 0, 2, 4])
            a[0x6d] = randomizer.choice([0, 0x40])
            p[0x24] = randomizer.randrange(256)
            p[0x9c] = randomizer.choice([0, 1, 2, 2])
            for offset in [0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c, 0x40, 0x70, 0x74, 0x7c, 0x94, 0x98]:
                w(p, offset, randomizer.randrange(0, 2000))
            w(p, 0x74, randomizer.choice([0x10000, 0x20000, 0x8000, 0x7fffffff, 0xffffffff]))
            w(p, 0x98, randomizer.randrange(256) << 9)
            w(p, 0x7c, randomizer.choice([0, 1, 4, 16, 128]))
            w(p, 0x44, randomizer.choice([0, ACTOR + randomizer.randrange(count) * 0x114]))
            for offset in [0x48, 0x4c, 0x50]:
                w(p, offset, randomizer.randrange(-0x40000, 0x40000))
            w(p, 0x6c, randomizer.randrange(2))
            actors.append(a); people.append(p)
        # Target and teammate fixtures exercise live and shared targets.
        if n % 4 == 0:
            people[0][0x9c] = 2; w(people[0], 0x44, ACTOR + 0x114)
            w(actors[1], 0x24, r(actors[0], 0x24) + 0x10000)
            w(actors[1], 0x28, r(actors[0], 0x28))
            w(actors[1], 0x2c, r(actors[0], 0x2c))
            actors[1][0x6c] = 0
            if site in [0x45f090, 0x45fb00]:
                h(actors[1], 0x70, struct.unpack_from('<H', actors[0], 0x70)[0])
            if site == 0x45fb00:
                w(actors[0], 0x30, 0); w(actors[0], 0x34, 0)
        # Empty records are zero in all fields.
        if n % 7 == 0 and count > 2:
            actors[-1] = bytearray(0x114); people[-1] = bytearray(0xb4)
        zones = []
        for k in range(3):
            zones.append([randomizer.randrange(4), randomizer.choice([0, 1, 2, MASK]), *sum(([i32(r(actors[0], at)) - (0x200000 if k == 1 else 1), i32(r(actors[0], at)) + (0x200000 if k == 1 else -1)] for at in [0x24, 0x28, 0x2c]), [])])
        case = dict(site=site, rng=randomizer.getrandbits(32), water=randomizer.choice([0, 0, 0x10000, MASK]), double=n % 2, excluded=randomizer.randrange(2), local=count-1, range=randomizer.choice([0x200000, 0x1000000, MASK]), radius=randomizer.choice([0x10000, 0x20000, 0x100000]), terrain=randomizer.randrange(-0x60000, 0x60000), terrain_mode=randomizer.randrange(2), sine_mode=randomizer.randrange(2), actors=actors, people=people, zones=zones)
        if site == 0x45fb00 and n % 4 == 0:
            case['sine_mode'] = 0
        if n % 13 == 0:
            w(actors[0], 0x2c, randomizer.choice([0x7fffffff, 0x7fff4000, 0x80000000, 0xffffffff]))
        cases.append(case)
    current = None
    trace = []
    def hook(machine, address, size, unused):
        if address not in [0x4776c0, 0x477820, 0x47df40, 0x42e2b0, 0x44a3e0, 0x445ce0, 0x45e7a0]:
            return
        sp = machine.reg_read(UC_X86_REG_ESP)
        arg = lambda i: word(sp + 4 + i * 4)
        if address in [0x4776c0, 0x477820]:
            a, b = sine(arg(0), current['sine_mode']); put(arg(1), a); put(arg(2), b)
        elif address == 0x47df40:
            x, y = i32(arg(0)), i32(arg(1))
            delta = (((x >> 8) ^ (y >> 8)) & 0x7ffff) - 0x40000 if current['terrain_mode'] else 0
            machine.reg_write(UC_X86_REG_EAX, (current['terrain'] + delta) & MASK)
        elif address == 0x42e2b0:
            machine.reg_write(UC_X86_REG_EAX, ((arg(2) - ACTOR) // 0x114) % 5)
        else:
            values = [arg(0), 0, 0] if address == 0x44a3e0 else [(arg(0)-ACTOR)//0x114, arg(1), 0] if address == 0x445ce0 else [0, 0, 0]
            trace.append([address, *values, word(ACTOR+0x30), word(ACTOR+0x34), word(ACTOR+0x2c), u.mem_read(PERSON+0x9c, 1)[0], word(PERSON+0x44), word(0xbd0060)])
        machine.reg_write(UC_X86_REG_EIP, word(sp)); machine.reg_write(UC_X86_REG_ESP, sp + 4)
    u.hook_add(unicorn.UC_HOOK_CODE, hook)
    rows, expected = [], []
    coverage = {hex(site): dict(nonzero_returns=0, effects=0) for site in SITES}
    for current in cases:
        c = current; trace.clear(); count = len(c['actors'])
        u.mem_write(ACTOR, bytes(256 * 0x114)); u.mem_write(PERSON, bytes(256 * 0xb4))
        for i, (a, p) in enumerate(zip(c['actors'], c['people'])):
            ai = bytearray(a)
            if r(ai, 4):
                definition = DEFS + i * 0x13c; w(ai, 4, definition); w(ai, 0xc, PERSON + i*0xb4)
                put(definition + 0x78, FRAMES + i*0x100)
                put(FRAMES + i*0x100 + 0x10, 32768 + i*9137)
                put(FRAMES + i*0x100 + 0x14, ((i % 19)-2)*65536)
            u.mem_write(ACTOR+i*0x114, bytes(ai)); u.mem_write(PERSON+i*0xb4, bytes(p))
        put(0x8cfa18, count); put(0xbd0060, c['rng']); put(0xbd5248, c['water']); put(0x67f548, c['double']*0x80); put(0x67dbc9, int(c['excluded'])*8); put(0xc23820, ACTOR+c['local']*0x114)
        put(0x4f8d44, len(c['zones']))
        for i, zone in enumerate(c['zones']):
            u.mem_write(0x8e48b0+i*32, struct.pack('<8I', *(v & MASK for v in zone)))
        u.mem_write(STACK, struct.pack('<6I', STOP, ACTOR, OUT if c['site']==0x45e280 else c['range'], c['radius'], 1, 0))
        u.reg_write(UC_X86_REG_ESP, STACK); u.reg_write(UC_X86_REG_FPCW, 0x37f)
        u.emu_start(c['site'], STOP, count=300000)
        if u.reg_read(UC_X86_REG_EIP) != STOP:
            raise RuntimeError(f"Original query did not return at {c['site']:x}")
        value = 0 if c['site'] in [0x45fd90, 0x45fe30, 0x45f2c0] else u.reg_read(UC_X86_REG_EAX)
        coverage[hex(c['site'])]['nonzero_returns'] += int(value != 0)
        coverage[hex(c['site'])]['effects'] += len(trace)
        distance = word(OUT) if c['site']==0x45e280 else 0
        record_images = []
        for i in range(count):
            a = bytearray(u.mem_read(ACTOR+i*0x114, 0x114)); p = bytes(u.mem_read(PERSON+i*0xb4, 0xb4))
            if r(a, 4):
                w(a, 4, r(a, 0)); w(a, 0xc, i)
            record_images.append(a.hex()+','+p.hex())
        text_trace = ';'.join(','.join(map(str, event)) for event in trace) or '-'
        expected.append(f"{value} {distance} {word(0xbd0060)} {text_trace} {';'.join(record_images)}")
        rows.append(' '.join(map(str, [c['site'], c['rng'], c['water'], c['double'], int(c['excluded']), c['local'], c['range'], c['radius'], c['terrain']&MASK, c['terrain_mode'], int(c['sine_mode']), ';'.join(a.hex()+','+p.hex() for a,p in zip(c['actors'],c['people'])), ';'.join(','.join(str(v&MASK) for v in zone) for zone in c['zones'])])))
    result = subprocess.run([str(args.probe.resolve())], input='\n'.join(rows)+'\n', capture_output=True, text=True, check=True)
    got = result.stdout.splitlines()
    if len(got) != len(expected):
        raise RuntimeError('Probe result count differs')
    failures = []
    counts = {hex(site): 0 for site in SITES}
    for i, (a, b) in enumerate(zip(expected, got)):
        counts[hex(cases[i]['site'])] += 1
        if a != b:
            first = next(j for j, (x, y) in enumerate(zip(a.split(), b.split())) if x != y)
            failures.append(dict(index=i, site=hex(cases[i]['site']), field=first, expected=a.split()[first][:700], got=b.split()[first][:700]))
    report = dict(source_sha256=SHA, cases=len(cases), functions=counts, coverage=coverage, mismatches=len(failures), samples=failures[:10], foreign_fixtures=['sine', 'terrain', 'sight', 'weapon frames', 'effect sinks'], angle_limit='Unicorn FPATAN uses reduced precision; native Intel original-instruction angle check covers boundary outputs separately')
    if args.report:
        args.report.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    raise SystemExit(bool(failures))

if __name__ == '__main__':
    main()
