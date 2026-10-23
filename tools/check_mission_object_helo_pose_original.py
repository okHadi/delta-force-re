#!/usr/bin/env python3
"""Check both helicopter pose callers with raw and real matrix services."""
import argparse
import hashlib
import json
import random
import struct
import subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from check_projectile_hits_original import Replay, HASH, STACK


def run(args):
    raw = (args.game / 'Df.exe').read_bytes()
    assert hashlib.sha256(raw).hexdigest() == HASH
    r = Replay(raw)
    r.call(0x477510)
    a, g, l, f, hp, hp2 = 0x7DDE80, 0x1301000, 0x1302000, 0x1304000, 0x1306000, 0x1307000
    regions = [(a, 0x114), (g, 0x200), (l, 0x200), (0x1303000, 0x18), (f, 0x80), (hp, 0x200), (hp2, 0x200)]
    rng = random.Random(0x450630)
    rows, expected, counts, failures = [], [], {'raw': 0, 'real': 0}, 0
    for case in range(args.cases):
        op, math, mutation = case % 2, (case // 2) % 2, (case * 37) & 255
        out = [0x1303000, a + 0x24, a + 0xC4, hp + 0x30][(case // 4) % 4]
        frame_original = STACK - (0x8C if op else 0x48)
        values = {}
        for base, size in regions:
            for offset in range(0, size - 3, 4):
                values[base + offset] = rng.getrandbits(32)
        values[a + 0x10] = g
        values[g + 0x50] = l
        values[g + 0x150] = l + 0x100
        values[l + 0xA4] = hp
        values[l + 0x1A4] = hp2
        for base in (hp, hp2):
            for offset in (0x18, 0x1C, 0x20, 0x30, 0x34, 0x38, 0x60, 0x64, 0x68):
                values[base + offset] = rng.choice([0, 0x80000000, 0x7FFFFFFF, 0xFFFFFFFF, rng.getrandbits(32)])
        rows.append([op, math, mutation, out, len(values), *[n for pair in sorted(values.items()) for n in pair]])
        for at, value in values.items():
            r.write(frame_original + at - f if f <= at < f + 0x80 else at, value)
        trace, pending = [], []
        body_start, body_end = (0x450630, 0x450707) if op else (0x4505D0, 0x45062C)

        def normalize(at):
            return f + at - frame_original if frame_original <= at < frame_original + (0x80 if op else 0x40) else at

        def parent():
            return body_start <= r.u.reg_read(UC_X86_REG_EIP) < body_end

        def flush():
            while pending:
                pending.pop(0)()

        def access(_u, kind, at, width, value, _data):
            if not parent():
                return
            assert width == 4
            logical = normalize(at)
            if kind == unicorn.UC_MEM_READ:
                if not any(base <= logical and logical + width <= base + size for base, size in regions):
                    return
                value = r.read(at)[0]
                trace.append(f'R{logical},4,{value};')
                if mutation & 16 and at == l + 0xA4:
                    r.write(hp + 0x34, 0x80000000)
            else:
                if not any(base <= logical and logical + width <= base + size for base, size in regions):
                    return
                trace.append(f'W{logical},4,{value & 0xFFFFFFFF};')
                if mutation & 2 and at == out:
                    pending.append(lambda: r.write(l + 0xA4, hp2))
                if mutation & 4 and at == out + 0xC:
                    pending.append(lambda: r.write(a + 0xC8, 0x11223344))
                if mutation & 64 and at == out + 4:
                    def fault():
                        raise RuntimeError('Fixture store stopped')
                    pending.append(fault)

        sites = {0x45D1E0: 2, 0x45D400: 2, 0x45CB40: 3, 0x45CDA0: 3}

        def matrix(site, count):
            argv = r.read(r.u.reg_read(UC_X86_REG_ESP) + 4, count)
            trace.append('C' + str(site) + ''.join(',' + str(normalize(n)) for n in argv) + ';')
            if site == 0x45D1E0:
                if mutation & 1:
                    r.write(a + 0x10, g + 0x100)
                    r.write(g + 0x50, l + 0x100)
                    r.write(l + 0xA4, hp2)
                if mutation & 32:
                    raise RuntimeError('Fixture compose stopped')
            if site == 0x45D400 and mutation & 8:
                r.write(frame_original + 0x40, 0xFFC00000)
            if site == 0x45CDA0 and mutation & 128:
                raise RuntimeError('Fixture transform stopped')
            if not math:
                destination = argv[2] if site in (0x45CB40, 0x45CDA0) else argv[0]
                source = argv[1]
                size = 3 if site == 0x45CDA0 else 12
                output = [r.read(source + (i % 6) * 4)[0] ^ site ^ (i * 0x10203) for i in range(size)]
                r.write(destination, *output)

        def code(_u, at, _size, _data):
            flush()
            if math and at in sites:
                caller = r.read(r.u.reg_read(UC_X86_REG_ESP))[0]
                if body_start <= caller < body_end:
                    matrix(at, sites[at])

        hooks = [r.u.hook_add(unicorn.UC_HOOK_MEM_READ | unicorn.UC_HOOK_MEM_WRITE, access),
                 r.u.hook_add(unicorn.UC_HOOK_CODE, code)]
        if not math:
            for site, count in sites.items():
                r.hook(site, lambda site=site, count=count: matrix(site, count))
        failed = False
        try:
            r.call(body_start, a, out)
            flush()
        except RuntimeError as error:
            assert str(error) in ('Fixture store stopped', 'Fixture compose stopped', 'Fixture transform stopped')
            failed, failures = True, failures + 1
        for hook in hooks:
            r.u.hook_del(hook)
        r.clear_hooks()
        # Caller frame differs by function. Its unused words remain caller-owned.
        # The raw native owner stores a full80h fixture; source smoke uses only40h.
        digest = 1469598103934665603
        for base, size in regions:
            data = bytearray()
            if base == f:
                used = 0x80 if op else 0x40
                data.extend(r.u.mem_read(frame_original, used))
                if used < size:
                    data.extend(b''.join(struct.pack('<I', values[f + i]) for i in range(used, size, 4)))
            else:
                data.extend(r.u.mem_read(base, size))
            for byte in data:
                digest = ((digest ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
        expected.append(f'{int(failed)} {digest} ' + ''.join(trace))
        counts['real' if math else 'raw'] += 1
    process = subprocess.run([str(args.probe.resolve())], input='\n'.join(' '.join(map(str, row)) for row in rows) + '\n', capture_output=True, text=True)
    assert process.returncode == 0, process.stderr
    actual = process.stdout.splitlines()
    assert len(actual) == len(expected), (len(actual), len(expected))
    for index, (found, wanted) in enumerate(zip(actual, expected)):
        assert found == wanted, (index, rows[index][:5], found, wanted)
    report = {'source_sha256': HASH, 'cases': counts, 'total': len(rows), 'differences': 0,
              'failure_prefix_cases': failures,
              'comparison': 'All parent source reads/writes/call order, full actor/graphic/LOD/hardpoint/pose/frame images, pointer changes and aliasing',
              'real_matrix_bodies': ['45D1E0', '45D400', '45CB40', '45CDA0', '477510'],
              'matrix_scope': 'Real mode compares pure math results/current buffers. Typed helper internal memory access order is outside this parent contract.',
              'raw_matrix_fixture': 'Declared deterministic matrix effects preserve current pointer/alias/failure observations.',
              'native_pivot_fix': 'model_special_pivot_matrix sourceNEG uses unsigned32 subtraction atINT_MIN.'}
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
