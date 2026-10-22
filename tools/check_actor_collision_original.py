#!/usr/bin/env python3
"""Compare actor collision and trigger state with hash-bound original x86."""
import argparse
import hashlib
import json
import random
import struct
from pathlib import Path
import unicorn as uc
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP
from check_projectile_hits_original import Replay, HASH, match, pack, setword, signed

ACTOR, TARGET, CARRIED, DEF, CDEF = 0x2201000, 0x2202000, 0x2203000, 0x2204000, 0x2205000
SITES = {0x446f00:1, 0x446f60:1, 0x45da30:1, 0x4605d0:1, 0x460470:2,
         0x447280:1, 0x4472d0:2, 0x447360:2, 0x446fb0:3, 0x447610:4, 0x43e720:1}


def run(args):
    source = (args.game / 'Df.exe').read_bytes()
    assert hashlib.sha256(source).hexdigest() == HASH
    r = Replay(source)
    r.call(0x477510)
    rng = random.Random(0x4889c0)
    cases, expected = [], []
    ids = [0, 2, 0xfa7, 0xfa8, 0xfa9, 0xfaa, 0xfab, 0xfb1, 0xfb2, 0xfb3, 0xfb5,
           0xfc1, 0xfc2, 0xfc3, 0xfc4, 0xfc5, 0xfc6, 0xfca, 0xfcb, 0xfcc, 0xfcd,
           0xfce, 0xfd5, 0xfd6, 0xfd7, 0xfd8, 0xfed, 0x80000000, 0xffffffff]
    site_counts = {hex(site): 0 for site in SITES}

    def setup(index):
        actor, target, carried = [bytearray(rng.randbytes(0x114)) for _ in range(3)]
        setword(actor, 0x90, rng.choice([0, CARRIED]));setword(actor, 0x78, rng.choice([0, 0, 1]))
        actor[0x6c] = rng.choice([0, 1, 4, 5, 0x81]);setword(actor, 0x70, rng.randrange(4))
        setword(target, 4, DEF);setword(carried, 4, rng.choice([0, CDEF]))
        setword(target, 0x74, rng.choice([0, 1, 100, 0xffff, 0x8000]))
        definition = [ids[index % len(ids)], 0x10]
        carried_definition = [rng.choice([0, 0xfc3, 0xfc4]), 0]
        host, incoming, seed, mutation = rng.randrange(10) != 0, rng.getrandbits(32), rng.getrandbits(32), rng.randrange(64)
        scenario = index // len(ids) % 16
        # Keep complete deposit, carry, delivery and objective paths in the
        # deterministic battery, in addition to all ID dispatch boundaries.
        if scenario < 15:
            host = True;actor[0x6c] = 0 if scenario == 0 else 5 if scenario == 7 else 1
            side = 2 if scenario in (2, 4, 6, 10, 12) else 0 if scenario in (0, 8, 13) else 1
            setword(actor, 0x70, side);setword(actor, 0x78, int(scenario == 13))
            setword(actor, 0x90, CARRIED if scenario in (3, 4, 5, 6, 9, 10) else 0)
            setword(carried, 4, 0 if scenario in (5, 6) else CDEF)
            carried_definition[0] = 0xfc4 if scenario in (4, 9) else 0xfc3
            setword(target, 0x74, 0x8000 if scenario == 14 else 100)
            mutation = mutation | 4 if scenario in (11, 12) else mutation & ~4
        else:host = False
        row = [actor.hex(), target.hex(), carried.hex(), *definition, *carried_definition, int(host), incoming, seed, mutation]
        for at, data in ((ACTOR, actor), (TARGET, target), (CARRIED, carried)):r.u.mem_write(at, bytes(data))
        for at, d in ((DEF, definition), (CDEF, carried_definition)):r.write(at + 0x20, *d)
        r.write(0x67f528, int(host))
        trace, calls = [], [0]
        def effect(site):
            site_counts[hex(site)] += 1
            arguments = r.read(r.u.reg_read(UC_X86_REG_ESP) + 4, SITES[site])
            trace.append(f'{site}' + ''.join(f',{n}' for n in arguments) + ':' + r.u.mem_read(ACTOR, 0x114).hex() + ':' + r.u.mem_read(TARGET, 0x114).hex() + ':' + r.u.mem_read(CARRIED, 0x114).hex() + ';')
            calls[0] += 1
            if mutation & 1 and site in (0x446f00, 0x446f60):r.write(ACTOR + 0x74, 0x01020304)
            if mutation & 2 and site == 0x460470:r.write(ACTOR + 0x90, TARGET)
            if mutation & 8 and site == 0x446fb0:r.write(TARGET + 0x24, r.read(TARGET + 0x24)[0] + 123)
            if mutation & 16 and site == 0x447610:r.u.mem_write(TARGET + 0x6c, bytes([r.u.mem_read(TARGET + 0x6c, 1)[0] | 0x20]))
            if mutation & 32 and site in (0x4472d0, 0x447360):r.write(ACTOR + 0x90, 0x12345678)
            answer = 0 if site == 0x4605d0 and mutation & 4 else seed ^ site ^ calls[0]
            r.u.reg_write(UC_X86_REG_EAX, answer)
        for site in SITES:r.hook(site, lambda site=site:effect(site))
        return row, trace

    def output(code, trace):
        return f'{code} {r.u.mem_read(ACTOR, 0x114).hex()} {r.u.mem_read(TARGET, 0x114).hex()} {r.u.mem_read(CARRIED, 0x114).hex()} {"".join(trace) or "-"}'

    for index in range(args.cases):
        row, trace = setup(index)
        # Set the incoming EAX without changing the original function body.
        r.u.mem_write(0x22f0000, b'\xb8' + pack(row[-3]) + b'\xe9' + pack(0x488f00 - (0x22f0000 + 10)))
        r.u.ctl_remove_cache(0x22f0000, 0x22f0010)
        code = r.call(0x22f0000, ACTOR, TARGET)
        cases.append(row);expected.append(output(code, trace));r.clear_hooks()
    match(args.probe, 'trigger', cases, expected)
    counts = {'488F00_full_records_ordered_calls': len(cases)}

    cases, expected = [], []
    identity = [0x400000, 0, 0, 0, 0, 0x400000, 0, 0, 0, 0, 0x400000, 0]
    for index in range(args.cases):
        row, trace = setup(index)
        actor, target = bytearray.fromhex(row[0]), bytearray.fromhex(row[1])
        cylinder_case = index % 3 == 0
        setword(actor, 0x24, rng.randint(-0x20000, 0x20000));setword(actor, 0x28, 0 if cylinder_case else rng.randint(-0x20000, 0x20000))
        setword(actor, 0x2c, rng.randint(-0x20000, 0x20000))
        for at in (0x24, 0x28, 0x2c):setword(target, at, 0)
        target[0x6c] = rng.choice([0, 8])
        primary, husk = 0x2240000, 0x2250000
        setword(target, 0x10, primary if index % 37 else 0)
        setword(target, 0x14, husk if index % 11 else 0)
        row[0], row[1] = actor.hex(), target.hex()
        # Physical and trigger models use the same volume resolution.
        row[4] = rng.choice([0, 0, 0x10])
        r.write(DEF + 0x24, row[4])
        r.u.mem_write(ACTOR, bytes(actor));r.u.mem_write(TARGET, bytes(target))
        radius, present, state_present, count = rng.choice([0, 0x20000, 0x40000]), index % 13 != 0, index % 17 != 0, rng.randrange(4)
        row.extend([radius, int(present), int(state_present), count])
        data, states, meshes_at = 0x2260000, 0x2261000, 0x2262000
        for model in (primary, husk):
            r.write(model + 0x64, radius);r.write(model + 0x50, data);r.write(model + 0x4c, model + 0x80)
            def radius_read(u, access, address, size, value, _, model=model):trace.append(f'm,{model};')
            r.hooks.append(r.u.hook_add(uc.UC_HOOK_MEM_READ, radius_read, begin=model + 0x64, end=model + 0x64))
            def state(model=model):
                trace.append(f's,{model}:{r.u.mem_read(ACTOR, 0x114).hex()}:{r.u.mem_read(TARGET, 0x114).hex()};')
                r.u.reg_write(UC_X86_REG_EAX, states if state_present else 0)
            r.hook(model + 0x80, state)
        r.write(data + 0x98, count, meshes_at);r.write(data + 0xb0, int(present))
        for mesh in range(count):
            matrix = identity[:]
            if not cylinder_case:
                for at in (3, 7, 11):matrix[at] = rng.randint(-0x10000, 0x10000)
                # Exact quarter-turn rotation includes off-diagonal terms.
                if mesh % 2:matrix[:3], matrix[4:7] = [0, -0x400000, 0], [0x400000, 0, 0]
            bounds = [-0x20000, 0x20000, -0x20000, 0x20000, -0x20000, 0x20000]
            volume_count = 1 if cylinder_case else rng.randrange(5)
            row.extend([*matrix, *bounds, volume_count])
            r.write(states + mesh * 0x40, *matrix)
            r.u.mem_write(states + mesh * 0x40 + 0x3c, bytes([rng.randrange(256)]))
            mesh_at, volume_at = meshes_at + mesh * 0x70, 0x2270000 + mesh * 0x1000
            r.write(mesh_at + 0x1c, volume_count, volume_at);r.write(mesh_at + 0x40, *bounds)
            for volume in range(volume_count):
                center = [0, 0, 0] if cylinder_case else [rng.randint(-0x10000, 0x10000) for _ in range(3)]
                extent = [rng.choice([0, 0x4000, 0xc000, 0x10000]) for _ in range(3)]
                box = [value for c, e in zip(center, extent) for value in (c - e, c + e)]
                kind = 3 if cylinder_case else rng.choice([0, 1, 1, 1, 2])
                horizontal = rng.choice([0, 0x8000, 0x10000])
                raw = [kind, rng.getrandbits(32), *center, 0, horizontal, horizontal, *extent, 0, *box]
                assert len(raw) == 18
                r.write(volume_at + volume * 72, *raw);row.extend(raw)
        r.u.ctl_remove_cache(0x400000, 0x500000)
        code = r.call(0x4889c0, ACTOR, TARGET)
        cases.append(row);expected.append(output(code, trace));r.clear_hooks()
    match(args.probe, 'collision', cases, expected)
    counts['4889C0_full_records_ordered_calls'] = len(cases)
    report = {'source_sha256': HASH, 'cases': counts,
              'foreign_call_counts': site_counts,
              'foreign_fixtures': ['model radius and volume views', 'model state callback', 'inventory and mission trigger calls'],
              'angle_coverage': 'cylinder direction uses axis-aligned fixtures; box matrices include quarter turns'}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--cases', type=int, default=2000)
    parser.add_argument('--report', type=Path)
    run(parser.parse_args())
