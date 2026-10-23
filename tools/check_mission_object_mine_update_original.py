#!/usr/bin/env python3
"""Compare451D40 with the supplied original x86 body."""
import argparse
import hashlib
import json
import random
import struct
import subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP
from check_projectile_hits_original import Replay, HASH, STACK


def run(a):
    raw = (a.game / 'Df.exe').read_bytes()
    assert hashlib.sha256(raw).hexdigest() == HASH
    rig, rng, actor, frame, physical = Replay(raw), random.Random(0x451D40), 0x7DDE80, 0x1300000, STACK - 0x14
    rows, expected, failures, removals = [], [], 0, 0
    edges = [0, 1, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x2FFFF, 0x30000, 0x640000, 0x640001]
    for case in range(a.cases):
        image, locals_ = bytearray(rng.randbytes(0x114)), rng.randbytes(8)
        for off in (0x7C, 0x2C, 0x30):
            struct.pack_into('<I', image, off, rng.choice(edges))
        sine, cosine = rng.choice(edges), rng.choice(edges)
        mutation, fail_at = case % 32, (1 + case % 14 if case % 3 == 2 else 0)
        rows.append(f'{sine} {cosine} {mutation} {fail_at} ' + (image + locals_).hex())
        rig.u.mem_write(actor, bytes(image))
        rig.u.mem_write(physical, locals_)
        trace, pending, count = [], [], [0]

        def flush(*_):
            while pending:
                pending.pop(0)()

        def stop():
            raise RuntimeError('Fixture stopped')

        def access(_u, kind, at, width, value, _data):
            if actor <= at and at + width <= actor + 0x114:
                logical = at
            elif physical <= at and at + width <= physical + 8:
                logical = frame + at - physical
            else:
                return
            count[0] += 1
            if kind == unicorn.UC_MEM_READ:
                value = int.from_bytes(rig.u.mem_read(at, width), 'little')
                trace.append(f'R{logical},{width},{value};')
                if count[0] == fail_at:
                    stop()
            else:
                value &= (1 << (width * 8)) - 1
                trace.append(f'W{logical},{width},{value};')
                if mutation & 2 and at == actor + 0x3C:
                    def change():
                        rig.write(actor + 0x7C, 0x80000000)
                        rig.write(physical, 0x7FFFFFFF)
                    pending.append(change)
                if count[0] == fail_at:
                    pending.append(stop)

        def trig():
            args = rig.read(rig.u.reg_read(UC_X86_REG_ESP) + 4, 3)
            assert args[1:] == (physical, physical + 4)
            trace.append(f'C{0x477820},{args[0]},{frame},{frame+4};')
            rig.write(physical, sine, cosine)
            if mutation & 1:
                rig.write(actor + 0x7C, 0x7FFFFFFF)
            if mutation & 8:
                stop()

        def remove():
            nonlocal removals
            removals += 1
            assert rig.read(rig.u.reg_read(UC_X86_REG_ESP) + 4)[0] == actor
            trace.append(f'C{0x45DA30},{actor};')
            if mutation & 4:
                rig.u.mem_write(actor, bytes(0x114))
            if mutation & 16:
                stop()
        hooks = [rig.u.hook_add(unicorn.UC_HOOK_CODE, flush),
                 rig.u.hook_add(unicorn.UC_HOOK_MEM_READ | unicorn.UC_HOOK_MEM_WRITE, access)]
        rig.hook(0x477820, trig)
        rig.hook(0x45DA30, remove)
        failed = False
        try:
            rig.call(0x451D40, actor)
            flush()
        except RuntimeError as error:
            assert str(error) == 'Fixture stopped'
            failed, failures = True, failures + 1
        for hook in hooks:
            rig.u.hook_del(hook)
        rig.clear_hooks()
        digest = 1469598103934665603
        for data in (rig.u.mem_read(actor, 0x114), rig.u.mem_read(physical, 8)):
            for byte in data:
                digest = ((digest ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
        expected.append(f'{int(failed)} {digest} ' + ''.join(trace))
    result = subprocess.run([str(a.probe.resolve())], input='\n'.join(rows) + '\n', capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    found = result.stdout.splitlines()
    assert len(found) == len(expected)
    for i, (actual, wanted) in enumerate(zip(found, expected)):
        assert actual == wanted, (i, rows[i], actual, wanted)
    report = {'source_sha256': HASH, 'body': '451D40..451DC8', 'cases': len(rows),
              'differences': 0, 'failure_prefix_cases': failures, 'removal_calls': removals,
              'comparison': 'Every actor/frame byte and ordered parent access/call; mutable providers and stopped prefixes',
              'fixtures': ['477820 current sine/cosine', '45DA30 current removal']}
    if a.report:
        a.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('game', type=Path)
    p.add_argument('probe', type=Path)
    p.add_argument('--cases', type=int, default=2048)
    p.add_argument('--report', type=Path)
    run(p.parse_args())
