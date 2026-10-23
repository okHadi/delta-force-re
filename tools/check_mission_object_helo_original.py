#!/usr/bin/env python3
"""Replay helicopter movement and scalar helpers against exact x86 instructions."""
import argparse
import hashlib
import json
import random
import struct
import subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP
from check_projectile_hits_original import Replay, HASH


def run(args):
    raw = (args.game / 'Df.exe').read_bytes()
    assert hashlib.sha256(raw).hexdigest() == HASH
    rig = Replay(raw)
    rig.call(0x477510)
    a, p, d = 0x7DDE80, 0x794680, 0x1302000
    regions = [(a, 0x114), (p, 0xB4 * 2), (d, 0x13C)]
    bodies = [0x451590, 0x44FF30, 0x44FFC0, 0x450010, 0x450090]
    rng = random.Random(0x451590)
    rows, expected, counts = [], [], {hex(body): 0 for body in bodies}
    calls = {hex(site): 0 for site in (0x47DF40, 0x49A620, 0x450D50, 0x450830, 0x477860)}
    failures = 0
    edges = [0, 1, 2, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x40000, 0x14000000]
    for case in range(args.cases):
        op = 0 if case % 2 == 0 else 1 + (case // 2) % 4
        desired, seed = rng.getrandbits(32), rng.getrandbits(32)
        host, mutation = case % 3 != 0, (case // 10) & 0xF7
        height = rng.choice([0, 0x10000, 0xFFFFFFFF, 0x80000000])
        actor, person, other, definition = [bytearray(rng.randbytes(n)) for n in (0x114, 0xB4, 0xB4, 0x13C)]
        put = lambda image, offset, value: struct.pack_into('<I', image, offset, value & 0xFFFFFFFF)
        put(actor, 4, d)
        put(actor, 0xC, p)
        for offset in (0x98, 0xAC, 0xB0, 0xB4, 0xC4, 0xC8, 0xCC, 0xD0, 0xD4):
            put(actor, offset, rng.choice(edges))
        put(actor, 0x2C, rng.choice([0, 0x40000, 0x7FFFFFFF, 0xFFFFFFFF]))
        put(actor, 0xDC, 0)
        put(person, 0x44, case % 4)
        put(person, 0x90, rng.choice([0, 1, 2, 0xFFFFFFFF]))
        put(person, 0x6C, rng.choice([0, 1, 8, 9, 0xFFFFFFFF]))
        put(definition, 0x20, rng.choice([0x7E5, 0x7E2, 0]))
        put(definition, 0xDC, rng.choice([0, 1, 0x100, 0xFFFFFFFF, 0x80000000]))
        put(definition, 0xE0, rng.choice([0, 1, 32, 0xFFFFFFFF, 0x80000000]))
        rows.append([op, desired, int(host), seed, mutation, height,
                     actor.hex(), person.hex(), other.hex(), definition.hex()])
        for address, image in ((a, actor), (p, person), (p + 0xB4, other), (d, definition)):
            rig.u.mem_write(address, bytes(image))
        rig.write(0x67F528, int(host))
        rig.write(0xBD0060, seed)
        trace, pending = [], []

        def flush():
            while pending:
                action = pending.pop(0)
                action()

        def code(_u, at, _size, _data):
            flush()
            if at == 0x477860:
                calls[hex(at)] += 1
                trace.append(f'C{at};')
                if mutation & 16:
                    rig.write(a + 0xC, p + 0xB4)
                    rig.write(p + 0x90, 88)

        def owned(at, width):
            return at == 0x67F528 or any(base <= at and at + width <= base + n for base, n in regions)

        def access(_u, kind, at, width, value, _data):
            if not owned(at, width):
                return
            if kind == unicorn.UC_MEM_READ:
                value = int.from_bytes(rig.u.mem_read(at, width), 'little')
                trace.append(f'R{at},{width},{value};')
            else:
                value &= (1 << (8 * width)) - 1
                trace.append(f'W{at},{width},{value};')
                if mutation & 4 and at == a + 0x9C:
                    pending.append(lambda: rig.write(a + 0xC, p + 0xB4))
                if mutation & 32 and at == a + 0x2C:
                    def fault():
                        raise RuntimeError('Fixture store stopped')
                    pending.append(fault)

        handles = [rig.u.hook_add(unicorn.UC_HOOK_CODE, code),
                   rig.u.hook_add(unicorn.UC_HOOK_MEM_READ | unicorn.UC_HOOK_MEM_WRITE, access)]

        def foreign(site, count):
            def callback():
                calls[hex(site)] += 1
                argv = rig.read(rig.u.reg_read(UC_X86_REG_ESP) + 4, count) if count else ()
                trace.append('C' + str(site) + ''.join(',' + str(n) for n in argv) + ';')
                if site == 0x47DF40:
                    if mutation & 1:
                        rig.write(a + 0xC, p + 0xB4)
                        rig.write(d + 0xDC, 0x123)
                    if mutation & 64:
                        raise RuntimeError('Fixture terrain stopped')
                    rig.u.reg_write(UC_X86_REG_EAX, height)
                elif site == 0x49A620:
                    rig.u.mem_write(argv[0], bytes([argv[1] & 255]) * argv[2])
                    rig.u.reg_write(UC_X86_REG_EAX, argv[0])
                elif site == 0x450D50 and mutation & 2:
                    rig.u.mem_write(a + 0x6C, struct.pack('<H', 0x700))
                    rig.write(a + 0x88, 0x9000)
                elif site == 0x450830 and mutation & 128:
                    rig.write(p + 0x90, 77)
            return callback

        for site, count in ((0x47DF40, 2), (0x49A620, 3), (0x450D50, 1), (0x450830, 1)):
            rig.hook(site, foreign(site, count))
        failed = False
        try:
            rig.call(bodies[op], a, *([desired] if op == 2 else []))
            flush()
        except RuntimeError as error:
            assert str(error) in ('Fixture store stopped', 'Fixture terrain stopped')
            failed = True
            failures += 1
        for handle in handles:
            rig.u.hook_del(handle)
        rig.clear_hooks()
        digest = 1469598103934665603
        for base, size in regions:
            for byte in rig.u.mem_read(base, size):
                digest = ((digest ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
        expected.append(f'{int(failed)} {rig.read(0xBD0060)[0]} {digest} ' + ''.join(trace))
        counts[hex(bodies[op])] += 1
    process = subprocess.run([str(args.probe.resolve())], input='\n'.join(' '.join(map(str, row)) for row in rows) + '\n', capture_output=True, text=True)
    assert process.returncode == 0, process.stderr
    found = process.stdout.splitlines()
    assert len(found) == len(expected), (len(found), len(expected))
    for index, (actual, wanted) in enumerate(zip(found, expected)):
        assert actual == wanted, (index, rows[index], actual, wanted)
    report = {'source_sha256': HASH, 'cases': counts, 'total': len(rows), 'differences': 0,
              'calls': calls, 'failure_prefix_cases': failures,
              'comparison': 'Full actor/two-person/definition images, every ordered source read/store, required calls, shared RNG, callback mutations and failure prefixes',
              'real_original_helpers': ['451590', '44FF30', '44FFC0', '450010', '450090', '477820', '477860', '477510'],
              'fixtures': ['47DF40 terrain', '450D50 crash', '450830 shot', '49A620 clear'],
              'native_reuse': '477820 uses the existing exact quantized kSineTableQ22 table;477860 uses existing behavior_rng_next.'}
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--cases', type=int, default=2048)
    parser.add_argument('--report', type=Path)
    run(parser.parse_args())
