#!/usr/bin/env python3
"""Compare mine movement with the supplied original instructions."""
import argparse
import hashlib
import json
import random
import struct
import subprocess
from pathlib import Path
import unicorn
from check_projectile_hits_original import Replay, HASH


def run(a):
    raw = (a.game / 'Df.exe').read_bytes()
    assert hashlib.sha256(raw).hexdigest() == HASH
    rig, rng, base = Replay(raw), random.Random(0x451DD0), 0x7DDE80
    rows, expected, failures = [], [], 0
    edges = [0, 1, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x2FFFF, 0x30000,
             0x17FFFFFF, 0x18000000, 0x18000001, 0xE7FFFFFF, 0xE8000000, 0xE8000001]
    for case in range(a.cases):
        image = bytearray(rng.randbytes(0x114))
        for off in (0x24, 0x28, 0x2C, 0x3C, 0x40, 0x7C, 0xE0):
            struct.pack_into('<I', image, off, rng.choice(edges))
        struct.pack_into('<I', image, 0xE4, case % 3)
        mutation, fail_at = case % 4, (1 + case % 16 if case % 3 == 2 else 0)
        rows.append(f'{mutation} {fail_at} {image.hex()}')
        rig.u.mem_write(base, bytes(image))
        trace, pending, accesses = [], [], [0]

        def flush(*_):
            while pending:
                pending.pop(0)()

        def stop():
            raise RuntimeError('Fixture access stopped')

        def access(_u, kind, at, width, value, _data):
            if not base <= at or at + width > base + 0x114:
                return
            assert width == 4
            accesses[0] += 1
            if kind == unicorn.UC_MEM_READ:
                value = rig.read(at)[0]
                trace.append(f'R{at},{value};')
                if accesses[0] == fail_at:
                    stop()
            else:
                value &= 0xFFFFFFFF
                trace.append(f'W{at},{value};')
                if at == base + 0x24 and mutation & 1:
                    pending.append(lambda: rig.write(base + 0x7C, 0x30000))
                if at == base + 0x2C and mutation & 2:
                    pending.append(lambda: rig.write(base + 0xE4, 1))
                if accesses[0] == fail_at:
                    pending.append(stop)
        hooks = [rig.u.hook_add(unicorn.UC_HOOK_CODE, flush),
                 rig.u.hook_add(unicorn.UC_HOOK_MEM_READ | unicorn.UC_HOOK_MEM_WRITE, access)]
        failed = False
        try:
            rig.call(0x451DD0, base)
            flush()
        except RuntimeError as error:
            assert str(error) == 'Fixture access stopped'
            failed, failures = True, failures + 1
        for hook in hooks:
            rig.u.hook_del(hook)
        digest = 1469598103934665603
        for byte in rig.u.mem_read(base, 0x114):
            digest = ((digest ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
        expected.append(f'{int(failed)} {digest} ' + ''.join(trace))
    result = subprocess.run([str(a.probe.resolve())], input='\n'.join(rows) + '\n', capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    found = result.stdout.splitlines()
    assert len(found) == len(expected)
    for i, (actual, wanted) in enumerate(zip(found, expected)):
        assert actual == wanted, (i, rows[i], actual, wanted)
    report = {'source_sha256': HASH, 'body': '451DD0..451E66', 'cases': len(rows),
              'differences': 0, 'failure_prefix_cases': failures,
              'comparison': 'Full actor bytes and ordered reads/stores; callback mutations and stopped access prefixes'}
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
