#!/usr/bin/env python3
"""Compare the raw450830 helicopter shot with source instructions."""
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
    a, p, d, f, frame_original = 0x7DDE80, 0x794680, 0x1302000, 0x1304000, STACK - 0x2C
    regions = [(a, 0x114), (p, 0xB4 * 2), (d, 0x13C), (frame_original, 0x1C)]
    rng = random.Random(0x450830)
    rows, expected, counts, failures = [], [], {'fixture_angles': 0, 'real_angles': 0, 'shot': 0}, 0
    edges = [0, 1, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0xE0000000, 0xDFFFFFFF,
             0x20000000, 0x20000001, 0xA0000000, 0x9FFFFFFF, 0xF4000000, 0xF3FFFFFF]
    for case in range(args.cases):
        math, mutation, seed = case % 2, (case * 37) & 1023, rng.getrandbits(32)
        boundary = case < 3 * len(edges) * len(edges)
        if boundary:
            math, mutation = 0, 0
        real_edge = bool(math and case % 17 == 0)
        if real_edge:
            mutation = 0
        actor, person, other, definition, frame = [bytearray(rng.randbytes(n)) for n in (0x114, 0xB4, 0xB4, 0x13C, 0x1C)]
        put = lambda image, offset, n: struct.pack_into('<I', image, offset, n & 0xFFFFFFFF)
        put(actor, 4, d)
        put(actor, 0xC, p)
        yaw, pitch, distance = rng.choice(edges), rng.choice(edges), rng.getrandbits(32)
        put(actor, 0x30, rng.getrandbits(32))
        put(actor, 0x34, rng.getrandbits(32))
        base_yaw, base_pitch = struct.unpack_from('<II', actor, 0x30)
        if boundary:
            yaw, pitch = edges[case % len(edges)], edges[(case // len(edges)) % len(edges)]
        yaw = (base_yaw + yaw) & 0xFFFFFFFF
        pitch = (base_pitch + pitch) & 0xFFFFFFFF
        kind = [0, 0x7E2, 0x7E5][(case // (len(edges) * len(edges))) % 3 if boundary else case % 3]
        if real_edge:
            kind = 0
            delta = [(0, 0, 0), (0x80000000, 0, 0), (0, 0x80000000, 0x80000000),
                     (0xFFFFFFFF, 1, 0), (1, 0xFFFFFFFF, 0x7FFFFFFF)][case % 5]
            for axis in range(3):
                put(person, 0x48 + axis * 4, struct.unpack_from('<I', frame, axis * 4)[0] + delta[axis])
        put(definition, 0x20, kind)
        rows.append([math, mutation, seed, yaw, pitch, distance, actor.hex(), person.hex(), other.hex(), definition.hex(), frame.hex()])
        for at, image in ((a, actor), (p, person), (p + 0xB4, other), (d, definition), (frame_original, frame)):
            r.u.mem_write(at, bytes(image))
        r.write(0xBD0060, seed)
        trace, pending, angle_calls, draws = [], [], 0, 0

        def normalize(at):
            return f + at - frame_original if frame_original <= at < frame_original + 0x1C else at

        def flush():
            while pending:
                pending.pop(0)()

        def parent():
            return 0x450830 <= r.u.reg_read(UC_X86_REG_EIP) < 0x45096A

        def access(_u, kind, at, width, value, _data):
            if not parent() or not any(base <= at and at + width <= base + size for base, size in regions):
                return
            assert width == 4
            if kind == unicorn.UC_MEM_READ:
                value = r.read(at)[0]
                trace.append(f'R{normalize(at)},4,{value};')
                if mutation & 16 and at == a + 4:
                    r.write(frame_original + 0xC, 0x12345678)
                    r.write(a + 0xB8, 123)
            else:
                trace.append(f'W{normalize(at)},4,{value & 0xFFFFFFFF};')
                if mutation & 4 and at == frame_original + 0xC:
                    pending.append(lambda: r.write(frame_original + 0x18, 0x12345678))
                if mutation & 8 and at == frame_original + 0x10:
                    pending.append(lambda: r.write(a + 0x34, 0x30000000))
                    pending.append(lambda: r.write(a + 0x30, 0x20000000))
                if mutation & 512 and at == frame_original + 0x10:
                    def fault():
                        raise RuntimeError('Fixture store stopped')
                    pending.append(fault)

        def call_args(n):
            return r.read(r.u.reg_read(UC_X86_REG_ESP) + 4, n)

        def record(site, argv):
            trace.append('C' + str(site) + ''.join(',' + str(n) for n in argv) + ';')

        def pose():
            owner, buffer = call_args(2)
            record(0x450630, (owner, normalize(buffer)))
            if mutation & 128:
                raise RuntimeError('Fixture pose stopped')
            if mutation & 1:
                r.write(a + 0xC, p + 0xB4)
            if mutation & 2:
                r.write(a + 0x94, 7)

        def shot():
            owner, buffer = call_args(2)
            record(0x45EA00, (owner, normalize(buffer)))
            trace.append('D' + ','.join(map(str, [r.read(a + 0x94)[0], r.read(a + 0xC)[0], *r.read(frame_original, 6)])) + ';')
            counts['shot'] += 1
            if mutation & 64:
                r.write(a + 0x34, 0xABCDEF01)

        def angle():
            nonlocal angle_calls
            record(0x477770, call_args(2))
            angle_calls += 1
            r.u.reg_write(UC_X86_REG_EAX, yaw if angle_calls == 1 else pitch)

        def sqrt():
            record(0x477620, call_args(1))
            r.u.reg_write(UC_X86_REG_EAX, distance)

        def code(_u, at, _size, _data):
            nonlocal draws
            flush()
            if at == 0x477860:
                record(at, ())
                draws += 1
                if mutation & 256:
                    raise RuntimeError('Fixture random stopped')
                if mutation & 32:
                    if draws == 1:
                        r.write(frame_original + 0xC, r.read(frame_original + 0xC)[0] + 0x10000)
                        r.write(a + 0x94, 7)
                    else:
                        r.write(frame_original + 0x10, r.read(frame_original + 0x10)[0] - 0x10000)
            if math and at in (0x477620, 0x477770):
                caller = r.read(r.u.reg_read(UC_X86_REG_ESP))[0]
                if 0x450830 <= caller < 0x45096A:
                    record(at, call_args(1 if at == 0x477620 else 2))

        hooks = [r.u.hook_add(unicorn.UC_HOOK_MEM_READ | unicorn.UC_HOOK_MEM_WRITE, access),
                 r.u.hook_add(unicorn.UC_HOOK_CODE, code)]
        r.hook(0x450630, pose)
        r.hook(0x45EA00, shot)
        if not math:
            r.hook(0x477620, sqrt)
            r.hook(0x477770, angle)
        failed = False
        try:
            r.call(0x450830, a)
            flush()
        except RuntimeError as error:
            assert str(error) in ('Fixture pose stopped', 'Fixture store stopped', 'Fixture random stopped')
            failed, failures = True, failures + 1
        for hook in hooks:
            r.u.hook_del(hook)
        r.clear_hooks()
        digest = 1469598103934665603
        for base, size in regions:
            for byte in r.u.mem_read(base, size):
                digest = ((digest ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
        expected.append(f'{int(failed)} {r.read(0xBD0060)[0]} {digest} ' + ''.join(trace))
        counts['real_angles' if math else 'fixture_angles'] += 1
    process = subprocess.run([str(args.probe.resolve())], input='\n'.join(' '.join(map(str, row)) for row in rows) + '\n', capture_output=True, text=True)
    assert process.returncode == 0, process.stderr
    actual = process.stdout.splitlines()
    assert len(actual) == len(expected), (len(actual), len(expected))
    for index, (found, wanted) in enumerate(zip(actual, expected)):
        assert found == wanted, (index, rows[index][:6], found, wanted)
    report = {'source_sha256': HASH, 'cases': counts, 'total': len(rows), 'differences': 0,
              'failure_prefix_cases': failures,
              'comparison': 'Every parent source read/store/call, full actor/two-person/definition/28byteframe, signed angle gates, currentweapon/person atshotdispatch, shared RNG and failureprefix',
              'original_bodies': ['450830', '477860', '477620/477770/49BAE6 in real-angle mode'],
              'fixtures': ['450630 supplies currentpose in sameframe with person/weapon mutations', '45EA00 records currentweapon/person/pose and mutates canonicalstate', 'Controlled angle returns for boundary mode'],
              'scope': 'Raw source450830 owner is group-agnostic; existing live Spawn currently supports group3 only. Actual nongroup3 allocator/wire semantics require separateproof.'}
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
