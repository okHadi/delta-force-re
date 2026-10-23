#!/usr/bin/env python3
"""Compare the full450E40 helicopter update with the original x86 body."""
import argparse
import hashlib
import json
import random
import struct
import subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP
from check_projectile_hits_original import Replay, HASH, STACK


def run(args):
    raw = (args.game / 'Df.exe').read_bytes()
    assert hashlib.sha256(raw).hexdigest() == HASH
    rig = Replay(raw)
    a, p, d, f = 0x7DDE80, 0x794680, 0x1302000, 0x1304000
    original_frame = STACK - 0x54
    regions = [(a, 0x114), (p, 0xB4 * 2), (d, 0x13C), (original_frame, 0x44)]
    sites = {0x43F020: 2, 0x4505D0: 2, 0x42A440: 3, 0x43EF80: 4,
             0x4503F0: 1, 0x450710: 1, 0x450D50: 1, 0x49A620: 3,
             0x4509B0: 1, 0x450AE0: 1, 0x4509F0: 1, 0x450C40: 1,
             0x450100: 4, 0x486FD0: 3}
    calls = {hex(site): 0 for site in (*sites, 0x477860)}
    rng = random.Random(0x450E40)
    rows, expected, failures = [], [], 0
    for case in range(args.cases):
        message, host, seed = int(case % 7 == 0), int(case % 4 != 0), rng.getrandbits(32)
        mutation = (case * 37) & 0x3DF
        search, condition, patrol = (0x7CCA80 if case % 5 == 0 else 0), case % 3, case % 2
        distance, height, damage = rng.choice([0, 0x700000, 0xFFFFFFFF]), rng.choice([0, 0x10000, 0xFFFFFFFF]), rng.choice([3, 4, 6, 7])
        actor, person, other, definition, locals_ = [bytearray(rng.randbytes(n)) for n in (0x114, 0xB4, 0xB4, 0x13C, 0x44)]
        put = lambda image, offset, n: struct.pack_into('<I', image, offset, n & 0xFFFFFFFF)
        put(actor, 4, d)
        put(actor, 0xC, p)
        put(actor, 0x1C, 0 if case % 3 else 0x6E0700)
        actor[0x6C] = rng.choice([0, 0, 0, 4, 12])
        struct.pack_into('<H', actor, 0x74, rng.choice([0, 1, 100, 0x7FFF, 0xFFFF]))
        for offset in (0x98, 0xA0):
            put(actor, offset, rng.choice([0, 1, 0x700000, 0x14000000, 0x1B000000, 0x80000000]))
        for offset in (0xC4, 0xD4):
            put(actor, offset, rng.choice([0, 1, 2, 3, 5, 16, 17, 0xFFFFFFFF]))
        put(actor, 0x2C, rng.choice([0, 0x40000, 0x100000, 0xFFFFFFFF]))
        put(actor, 0xB4, rng.choice([0, 0x80000, 0x90000, 0xA0000]))
        put(actor, 0x7C, rng.choice([0, 0x3FF, 0x400, 0x10000]))
        put(actor, 0xA8, rng.choice([0, 1, 2]))
        person[0x24] = rng.choice([0, 1, 2, 0x40, 0x42])
        person[0x9C] = rng.choice([0, 2])
        put(person, 0x44, 0 if case % 3 else 0x7CCA80)
        put(person, 0x38, rng.choice([0, 1, 2, 0xFFFFFFFF]))
        put(person, 0x90, rng.choice([0, 1, 0xFFFFFFFF]))
        put(definition, 0x20, rng.choice([0, 0x7E2, 0x7E5]))
        rows.append([message, host, seed, mutation, search, condition, patrol, distance, height, damage,
                     actor.hex(), person.hex(), other.hex(), definition.hex(), locals_.hex()])
        for at, image in ((a, actor), (p, person), (p + 0xB4, other), (d, definition), (original_frame, locals_)):
            rig.u.mem_write(at, bytes(image))
        rig.write(0x67F528, host)
        rig.write(0xA158C0, damage)
        rig.write(0xBD0060, seed)
        trace, pending = [], []

        def normalize(at):
            return f + at - original_frame if original_frame <= at < original_frame + 0x44 else at

        def flush():
            while pending:
                pending.pop(0)()

        def code(_u, at, _size, _data):
            flush()
            if at == 0x477860:
                calls[hex(at)] += 1
                trace.append(f'C{at};')

        def owned(at, width):
            return at in (0x67F528, 0xA158C0) or any(base <= at and at + width <= base + size for base, size in regions)

        def access(_u, kind, at, width, value, _data):
            if not owned(at, width):
                return
            if kind == unicorn.UC_MEM_READ:
                value = int.from_bytes(rig.u.mem_read(at, width), 'little')
                trace.append(f'R{normalize(at)},{width},{value};')
            else:
                value &= (1 << (8 * width)) - 1
                trace.append(f'W{normalize(at)},{width},{value};')
                if mutation & 1 and at == a + 0xD4:
                    pending.append(lambda: rig.write(0xA158C0, 6))
                if mutation & 128 and at == p + 0x40:
                    pending.append(lambda: rig.write(a + 0xFC, 57))
                if mutation & 256 and at == a + 0x20:
                    def fault():
                        raise RuntimeError('Fixture store stopped')
                    pending.append(fault)

        handles = [rig.u.hook_add(unicorn.UC_HOOK_CODE, code),
                   rig.u.hook_add(unicorn.UC_HOOK_MEM_READ | unicorn.UC_HOOK_MEM_WRITE, access)]

        def foreign(site, count):
            def callback():
                calls[hex(site)] += 1
                argv = rig.read(rig.u.reg_read(UC_X86_REG_ESP) + 4, count)
                trace.append('C' + str(site) + ''.join(',' + str(normalize(n)) for n in argv) + ';')
                result = 0
                if site == 0x486FD0 and mutation & 512:
                    raise RuntimeError('Fixture ground stopped')
                if site == 0x49A620:
                    rig.u.mem_write(argv[0], bytes([argv[1] & 255]) * argv[2])
                    result = argv[0]
                elif site == 0x4505D0:
                    rig.write(argv[1], 0x10000, 0x20000, 0x30000)
                    if mutation & 2:
                        rig.write(original_frame + 0x40, 1)
                        rig.write(original_frame + 0x3C, 0)
                elif site == 0x43EF80:
                    rig.write(a + 0x1C, 0x6E0700)
                    if mutation & 4:
                        rig.write(a + 0xC, p + 0xB4)
                elif site == 0x43F020:
                    rig.write(a + 0x1C, 0)
                elif site == 0x450C40:
                    if mutation & 4:
                        rig.write(a + 0xC, p + 0xB4)
                    result = search
                elif site == 0x450AE0:
                    result = condition
                elif site == 0x4509F0:
                    result = patrol
                elif site == 0x4509B0:
                    result = distance
                elif site == 0x486FD0:
                    result = height
                elif site == 0x42A440:
                    if mutation & 64:
                        rig.write(a + 0xFC, 32)
                elif site == 0x450100:
                    if mutation & 8:
                        rig.write(a + 0xA8, 0)
                        rig.write(a + 0x98, 0)
                elif site in (0x4503F0, 0x450710, 0x450D50) and mutation & 16:
                    rig.write(a + 0xA8, 0)
                rig.u.reg_write(UC_X86_REG_EAX, result)
            return callback

        for site, count in sites.items():
            rig.hook(site, foreign(site, count))
        failed = False
        try:
            rig.call(0x450E40, a, message)
            flush()
        except RuntimeError as error:
            assert str(error) in ('Fixture store stopped', 'Fixture ground stopped')
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
    process = subprocess.run([str(args.probe.resolve())], input='\n'.join(' '.join(map(str, row)) for row in rows) + '\n', capture_output=True, text=True)
    assert process.returncode == 0, process.stderr
    actual = process.stdout.splitlines()
    assert len(actual) == len(expected), (len(actual), len(expected))
    for index, (found, wanted) in enumerate(zip(actual, expected)):
        assert found == wanted, (index, rows[index], found, wanted)
    report = {'source_sha256': HASH, 'cases': len(rows), 'differences': 0, 'calls': calls,
              'failure_prefix_cases': failures,
              'comparison': 'Full actor/two-person/definition/current44hframe images, every ordered source read/store, message branches, calls and shared RNG',
              'original_bodies': ['450E40', '477860'],
              'source_frame_normalization': {'original': hex(original_frame), 'native': hex(f), 'bytes': 0x44},
              'foreign_fixtures': [hex(site) for site in sites]}
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
