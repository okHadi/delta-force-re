#!/usr/bin/env python3
"""Compare the NPC muzzle/shot/request chain with the supplied x86 program."""
import argparse
import hashlib
import json
import random
import struct
import subprocess
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP
from check_projectile_hits_original import Replay, HASH, pack


def run(args):
    source = (args.game / 'Df.exe').read_bytes()
    assert hashlib.sha256(source).hexdigest() == HASH
    r = Replay(source)
    r.call(0x477510)
    r.call(0x481AF0)
    actor = 0x7CCA80 + 0x114
    people, graphic, model, lod, points, slots, callback, items = (
        0x2205000, 0x220B000, 0x220A000, 0x220D000,
        0x220E000, 0x220C000, 0x220F000, 0x2230000)
    r.write(0xAD3ED0, 0x4F9934)
    r.write(0xAD3EE4, 0x4F9154)
    r.write(0xC23810, items)
    r.write(0x67F528, 1)
    r.write(model + 0x50, lod)
    r.write(lod + 0xA4, points)
    r.write(graphic + 0x4C, callback)
    r.write(graphic + 0x50, 0x12345678)
    for i in range(32):
        r.write(points + i * 12, 10, 20, 30)
    for w in range(39):
        secondary = r.read(0x4F9934 + w * 0xA8 + 0x68)[0]
        r.write(0x4F9934 + w * 0xA8 + 0x98, int(secondary != 0))
    identity = [0x400000, 0, 0, 0, 0, 0x400000, 0, 0, 0, 0, 0x400000, 0]
    rng = random.Random(0x45E7A0)
    rows, wanted = [], []
    for case in range(args.cases):
        weapon = case % 39
        mutation = (case // 39) % 7
        burst = rng.choice([0, 1, 2, 0xFFFFFFFF, 0x80000000])
        seed, yaw, pitch, roll = [rng.getrandbits(32) for _ in range(4)]
        flags = 4 if case % 3 == 0 else 0
        x, y, z = [rng.randrange(-0x400000, 0x400000) & 0xFFFFFFFF for _ in range(3)]
        graphic_present = int(case % 31 != 0)
        model_present = int(case % 29 != 0)
        rows.append([weapon, burst, seed, flags, yaw, pitch, roll, x, y, z,
                     mutation, graphic_present, model_present])
        r.u.mem_write(actor, bytes(0x114))
        r.u.mem_write(people, bytes(0xB4 * 3))
        r.write(actor, 1)
        r.write(actor + 0xC, people + 0xB4)
        r.write(actor + 0x10, graphic if graphic_present else 0)
        r.write(actor + 0x24, x, y, z, yaw, pitch, roll)
        r.write(actor + 0x6C, flags)
        r.write(actor + 0x94, weapon)
        r.write(people + 0xB4 + 0x6C, burst)
        r.write(items + 0x13C + 0x6C, model if model_present else 0)
        r.write(items + 0x6C, 0)
        r.write(0xBD0060, seed)
        r.write(slots + 0x1C0, *identity)
        trace = []

        def call_args(n):
            return r.read(r.u.reg_read(UC_X86_REG_ESP) + 4, n)

        def graphic_call():
            assert call_args(3) == (0x12345678, actor, 0)
            trace.append(1)
            if mutation in (1, 3, 5):
                r.write(actor + 0x94, 1 if mutation == 5 else 7)
                r.write(actor + 0x30, 0xAABBCCDD, 0x10203040)
                r.write(actor + 0xC, people + 0xB4 * 2)
                r.write(people + 0xB4 * 2 + 0x6C, 3)
            if mutation == 3:
                raise RuntimeError('graphic stopped')
            if mutation == 6:
                r.write(actor + 0x6C, flags ^ 4)
                r.write(actor + 0x30, 0x12345678, 0x87654321)
                r.write(actor + 0x24, (x + 0x10000) & 0xFFFFFFFF)
            r.u.reg_write(UC_X86_REG_EAX, slots)

        def ground():
            trace.extend([2, *call_args(2)])
            r.u.reg_write(UC_X86_REG_EAX, 0)

        def deliver():
            (packet,) = call_args(1)
            trace.extend([3, *r.u.mem_read(packet, 15)])
            if mutation in (2, 4):
                r.write(actor + 0xC, people)
                r.write(people + 0x6C, 27)
            if mutation == 4:
                raise RuntimeError('delivery stopped')

        def allocate():
            pose, owner, _kind = call_args(3)
            assert owner == actor
            trace.extend([4, *r.read(pose, 5)])

        def melee():
            assert call_args(1) == (actor,)
            trace.append(5)

        for at, fn in ((callback, graphic_call), (0x47DF40, ground),
                       (0x445B80, deliver), (0x462F20, allocate), (0x470B00, melee)):
            r.hook(at, fn)
        failed = False
        try:
            r.call(0x45E7A0, actor)
        except RuntimeError as error:
            assert str(error) in ('graphic stopped', 'delivery stopped')
            failed = True
        r.clear_hooks()
        values = [int(failed), r.read(0xBD0060)[0],
                  *[r.read(people + i * 0xB4 + 0x6C)[0] for i in range(3)],
                  r.read(actor + 0x94)[0], *r.read(actor + 0x30, 2),
                  (r.read(actor + 0xC)[0] - people) // 0xB4,
                  *r.read(slots + 0x1C0, 12), len(trace), *trace]
        wanted.append(' '.join(map(str, values)))
    process = subprocess.run([str(args.probe.resolve())],
                             input='\n'.join(' '.join(map(str, row)) for row in rows) + '\n',
                             capture_output=True, text=True)
    assert process.returncode == 0, process.stderr
    actual = process.stdout.splitlines()
    assert len(actual) == len(wanted), (len(actual), len(wanted), process.stderr)
    for index, (found, expected) in enumerate(zip(actual, wanted)):
        assert found == expected, (index, rows[index], found, expected)
    report = {'source_sha256': HASH, 'cases': len(rows), 'differences': 0,
              'original_bodies': ['45E7A0', '45EA00', '46FB40', '477860',
                                  '477510', '481AF0', 'matrix/angle/sqrt helpers'],
              'comparison': 'Current weapon/person, shared RNG, slot-seven image, ordered effects, exact packet bytes, caller pose and failure prefix',
              'fixtures': ['graphic callback with current actor mutations', 'terrain height zero',
                           '445B80 delivery with person mutations', '462F20 allocation boundary', '470B00 melee'],
              'native_scope': 'Native probe runs the actual shared MissionProjectileSpawn allocator; original allocator body has a separate checked module.'}
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--cases', type=int, default=1170)
    parser.add_argument('--report', type=Path)
    run(parser.parse_args())
