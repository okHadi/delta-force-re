#!/usr/bin/env python3
"""Compare the live player adapter with original DF1 instructions.

Execute real 482B00, 484C20, firing, muzzle, spread and recoil instructions.
Use null graphic models and declared synchronous effect/input/CRT fixtures.
"""
import argparse
import json
import random
import subprocess
from pathlib import Path
from check_player_weapon_runtime_original import Rig, G


class LiveRig(Rig):
    def __init__(self, game):
        super().__init__(game)
        for address in (0x482D90, 0x4847D0, 0x484740, 0x45E7A0):
            self.stubs.pop(address)

    def setup(self, weapon):
        super().setup(weapon)
        self.poke(0xC23A24, self.live_scope)

    def spawn(self):
        transform, actor, kind = self.args(3)
        self.calls.append([0x46FB40, kind, self.peek(self.burst(actor)),
                           *[self.peek(transform + i * 4) for i in range(6)]])
        self.change(actor)
        if self.mutation == 2:
            self.poke(transform + 12, self.peek(transform + 12) + 11)
            self.poke(transform + 16, self.peek(transform + 16) + 13)
        self.finish()

    def run_live(self, mode, values):
        self.live_scope = 0 if mode == 'aim' else values[10]
        if mode == 'view':
            weapon, _, _, _, burst, _, _, _, mutation, camera, steady, crt, seed = values
            result = self.run('view', [weapon, camera, steady, crt, mutation, burst, seed])
        else:
            result = self.run(mode, values)
        if mode != 'aim':
            result['calls'] = [c for c in result['calls'] if c[0] not in (0x45EA00, 0x477860)]
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    rig = LiveRig(args.game)
    rng = random.Random(0x482B0045E946)
    cases = []
    for i in range(300):
        cases.append(('aim', [rng.choice([0, 0, 1, 7, 0xffffffff]),
                              rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32), 0]))
    for weapon in range(39):
        for burst in [0, 1, 2, 0x80000000, 0xffffffff]:
            for mutation in [0, 1, 2, 3, 7, 9]:
                for camera in [0, 1]:
                    for scope in [0, 1]:
                        cases.append(('view', [weapon, 0, 0, 0, burst, 0, 0, 0, mutation,
                                               camera, scope, rng.getrandbits(32), rng.getrandbits(32)]))
    edge = [0, 1, 2, 0x7fffffff, 0x80000000, 0xffffffff]
    for i in range(1800):
        cases.append(('prefix', [rng.randrange(39), rng.choice(edge), rng.choice(edge),
                                  rng.choice(edge), rng.choice([0, 0, 1, 2]), rng.randrange(2),
                                  rng.randrange(2), rng.randrange(2), rng.choice([0, 1, 2, 3, 7, 8, 9]),
                                  rng.randrange(2), rng.randrange(2), rng.getrandbits(32), rng.getrandbits(32)]))
    lines = [mode + '|' + ','.join(map(str, values)) for mode, values in cases]
    process = subprocess.run([str(args.probe.resolve())], input='\n'.join(lines) + '\n',
                             text=True, capture_output=True)
    if process.returncode:
        raise RuntimeError(process.stderr)
    rows = process.stdout.splitlines()
    if len(rows) != len(cases):
        raise AssertionError(f'row count {len(rows)} != {len(cases)}')
    counts = {}
    for i, ((mode, values), row) in enumerate(zip(cases, rows)):
        expected, actual = rig.run_live(mode, values), json.loads(row)
        if actual != expected:
            raise AssertionError(f'case {i}: {lines[i]}\noriginal={expected}\nnative={actual}')
        counts[mode] = counts.get(mode, 0) + 1
    report = {'source_sha256': 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff',
              'status': 'passed', 'cases': counts, 'total': len(cases),
              'real_bodies': '482B00,484C20,firing,482D90,4847D0,484740,45E7A0,45EA00,477860,477510,4776C0',
              'foreign_bodies': 'declared input,CRT,spawn,melee,sound,message,placement fixtures',
              'models': 'null graphics; camera branch takes original no-graphic return'}
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
