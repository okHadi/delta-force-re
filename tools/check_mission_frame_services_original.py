#!/usr/bin/env python3
"""Compare mission frame services with original DF1 instructions."""
import argparse
import json
import random
import subprocess
from pathlib import Path
from check_weapons_original import OriginalRig
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_EIP, UC_X86_REG_ESP

Q = 0x8E3D74
RESOURCE = 0x1100000


class Rig(OriginalRig):
    def __init__(self, game):
        super().__init__(game)
        self.stubs.update({0x456E3B: self.rdtsc})
        for address in [0x456110, 0x447C60, 0x456000, 0x455B40]:
            self.stubs[address] = lambda address=address: self.script_call(address)

    def rdtsc(self):
        self.u.reg_write(UC_X86_REG_EAX, self.tsc & 0xFFFFFFFF)
        self.u.reg_write(UC_X86_REG_EDX, self.tsc >> 32)
        self.u.reg_write(UC_X86_REG_EIP, 0x456E3D)

    def args(self, count):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        return [self.peek(sp + 4 + i * 4) for i in range(count)]

    def queue(self, index, values):
        for i, value in enumerate(values):
            self.poke(Q + index * 16 + i * 4, value)

    def script_call(self, site):
        resource, index = 0, 0
        if site != 0x455B40:
            resource = self.args(1)[0]
        if site in [0x456110, 0x447C60]:
            index = self.args(2)[1]
        normalized = 0 if site == 0x455B40 else ((resource - RESOURCE) // 0x1000)
        if site in [0x447C60, 0x456000]:
            normalized += 4
        self.calls.append([site, normalized, index])
        if site == 0x456110 and not self.mutated:
            self.mutated = True
            if self.mutation == 1:
                self.poke(Q + 4, 1)
            if self.mutation == 2:
                self.poke(resource + 0x1C, 3)
                self.u.mem_write(resource + 0x69 + index * 0x44, bytes([251]))
                self.poke(0x8E3D64, 0xFFFFFFFF)
            if self.mutation == 4:
                n = self.peek(0x8E3D70)
                self.queue(n, [RESOURCE + 0x1000, 0, 1, 0])
                self.poke(0x8E3D70, n + 1)
        if site == 0x456000:
            n = self.peek(0x8E3D70)
            for i in range(n):
                if self.peek(Q + i * 16) + 4 == resource:
                    data = bytes(self.u.mem_read(Q + (i + 1) * 16, (n - i - 1) * 16))
                    if data:
                        self.u.mem_write(Q + i * 16, data)
                    self.poke(0x8E3D70, n - 1)
                    break
        self.finish()

    def run(self, mode, v):
        self.reset()
        self.calls = []
        if mode == 'profile':
            next_, current, previous, lo, hi, *totals = v
            self.tsc = lo | (hi << 32)
            self.poke(0x4F8718, current)
            self.poke(0x4F871C, previous)
            for i, value in enumerate(totals):
                self.poke(0x8E3F40 + i * 4, value)
            self.call(0x456E30, next_)
            state = [self.peek(0x4F8718), self.peek(0x4F871C)]
            state += [self.peek(0x8E3F40 + i * 4) for i in range(8)]
        elif mode == 'camera':
            for address, value in zip([0x6BA3D0, 0x6BA3D4, 0x6BA3D8, 0x6BA3E8], v[:4]):
                self.poke(address, value)
            self.poke(0x6BA3F0, RESOURCE)
            for i, value in enumerate(v[4:]):
                self.poke(RESOURCE + 0x24 + i * 4, value)
            self.call(0x42AB70)
            state = [self.peek(address) for address in [0x6BA3D0, 0x6BA3D4, 0x6BA3D8,
                                                      0x6BA3E0, 0x6BA3E4, 0x6BA3DC, 0x6BA3E8]]
        elif mode == 'timers':
            for i, value in enumerate(v):
                self.poke(0xBD4814 + i * 4, value)
            self.call(0x479ED0)
            state = [self.peek(0xBD4814 + i * 4) for i in range(3)]
        elif mode == 'stat':
            self.poke(0xBCFC24, v[0])
            self.call(0x4702A0, RESOURCE)
            state = [self.peek(0xBCFC24)]
            self.calls = [[RESOURCE, self.peek(RESOURCE)]]
        elif mode == 'request':
            self.poke(0x8EE1D4, v[0])
            self.call(0x461340, v[1])
            self.call(0x461350)
            state = [self.u.reg_read(UC_X86_REG_EAX)]
        elif mode == 'scripts':
            enabled, scale, pulse, self.mutation, n = v[:5]
            self.mutated = False
            self.poke(0x8E3D68, enabled)
            self.poke(0x8E3D64, scale)
            self.poke(0x8E3D70, n)
            for r in range(2):
                self.poke(RESOURCE + r * 0x1000 + 0x1C, v[5 + r])
                for j in range(4):
                    self.u.mem_write(RESOURCE + r * 0x1000 + 0x69 + j * 0x44, bytes([v[7 + r * 4 + j]]))
            for i in range(n):
                words = v[15 + 4 * i:19 + 4 * i]
                self.queue(i, [RESOURCE + words[0] * 0x1000, *words[1:]])
            self.call(0x4562A0, pulse)
            n = self.peek(0x8E3D70)
            state = [self.peek(0x8E3D64), n, self.peek(RESOURCE + 0x1C), self.peek(RESOURCE + 0x101C)]
            for i in range(n):
                state += [(self.peek(Q + i * 16) - RESOURCE) // 0x1000]
                state += [self.peek(Q + i * 16 + j * 4) for j in range(1, 4)]
        return {'state': state, 'calls': self.calls}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    rig, rng = Rig(args.game), random.Random(0x4562A042AB70)
    edge = [0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
    cases = []
    for _ in range(1200):
        cases.append(('profile', [rng.randrange(8), rng.randrange(8), *[rng.getrandbits(32) for _ in range(11)]]))
        cases.append(('camera', [rng.choice(edge) if rng.randrange(3) else rng.getrandbits(32) for _ in range(8)]))
    for _ in range(300):
        cases.append(('timers', [rng.choice(edge) for _ in range(3)]))
        cases.append(('stat', [rng.getrandbits(32)]))
        cases.append(('request', [rng.getrandbits(32), rng.getrandbits(32)]))
    for _ in range(1800):
        count = [rng.randrange(1, 4), rng.randrange(1, 4)]
        n = rng.randrange(4)
        v = [rng.choice([0, 1, 1, 1]), rng.getrandbits(32), rng.randrange(2), rng.choice([0, 1, 2, 4]), n,
             *count, *[rng.randrange(256) for _ in range(8)]]
        for _ in range(n):
            resource = rng.randrange(2)
            v += [resource, rng.randrange(count[resource] + 1), rng.choice([0, 0, 1, 0xFFFFFFFF]), rng.randrange(2)]
        cases.append(('scripts', v))
    lines = [mode + '|' + ','.join(map(str, values)) for mode, values in cases]
    process = subprocess.run([str(args.probe.resolve())], input='\n'.join(lines) + '\n', text=True, capture_output=True)
    if process.returncode:
        raise RuntimeError(process.stderr)
    rows = process.stdout.splitlines()
    assert len(rows) == len(cases)
    counts = {}
    for i, ((mode, values), row) in enumerate(zip(cases, rows)):
        expected, actual = rig.run(mode, values), json.loads(row)
        if actual != expected:
            raise AssertionError(f'case {i}: {lines[i]}\noriginal={expected}\nnative={actual}')
        counts[mode] = counts.get(mode, 0) + 1
    report = {'status': 'passed', 'source_sha256': 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff',
              'cases': counts, 'total': len(cases), 'providers': 'RDTSC and synchronous script bodies; real service instructions'}
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
