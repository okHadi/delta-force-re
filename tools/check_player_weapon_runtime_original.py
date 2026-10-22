#!/usr/bin/env python3
"""Compare live player weapons with original x86 instructions.

Real bodies: 484C20, 45EA00, weapon fire paths and complete 482B00.
Declared callbacks: muzzle/geometry, animated action, input, CRT RNG,
spawn/melee, sound/message. Shared 477860 and sine table instructions run.
"""
import argparse
import json
import random
import struct
import subprocess
from pathlib import Path
from check_weapons_original import OriginalRig, ACTOR, ACTOR_DEF, G

A1 = ACTOR + 0x1000
P1 = ACTOR_DEF + 0x1000
TRANSFORM = 0x1004000
RNG = 0xBD0060


class Rig(OriginalRig):
    def __init__(self, game):
        super().__init__(game)
        self.stubs.pop(0x4776C0)
        self.stubs.pop(0x49BAE6)
        from unicorn.x86_const import UC_X86_REG_FPCW
        self.u.reg_write(UC_X86_REG_FPCW,0x037f)
        self.call(0x477510)
        self.calls = []
        self.stubs.update({0x477860: self.random_enter, 0x45EA00: self.shot_enter,
                           0x46FB40: self.spawn, 0x470B00: self.melee, 0x470F60: self.place,
                           0x49AF6C: self.crt_random, 0x484740: self.steady_query,
                           0x482D90: self.muzzle, 0x4847D0: self.barrel,
                           0x45E7A0: self.animated, 0x43E7D0: self.sound,
                           0x43E720: self.sound2, 0x46F3E0: self.queue,
                           0x479F00: self.message, 0x47A290: self.input_query})

    def args(self, count):
        from unicorn.x86_const import UC_X86_REG_ESP
        sp = self.u.reg_read(UC_X86_REG_ESP)
        return [self.peek(sp + 4 + 4 * i) for i in range(count)]

    def burst(self, actor):
        return self.peek(actor + 0xC) + 0x6C

    def change(self, actor):
        if self.mutation == 1:
            self.poke(self.burst(actor), 9)
        elif self.mutation == 3:
            self.poke(actor + 0xC, P1)
            self.poke(G['actor_ptr'], A1)
        elif self.mutation == 9:
            current = self.peek(G['slot_map'] + 4 * self.peek(G['slot']))
            self.set_slot_map([(current + 1) % 39] * 10)

    def random_enter(self):
        self.calls.append([0x477860])
        if self.mode == 'aim':
            if self.mutation:
                self.poke(G['aim_block'], 7)
        elif self.mutation == 4:
            self.poke(ACTOR_DEF + 0x6C, 3)
        # Real 477860 executes after this fixture callback.

    def shot_enter(self):
        actor, transform = self.args(2)
        if self.mode != 'shot':
            from unicorn.x86_const import UC_X86_REG_ESP
            site = self.peek(self.u.reg_read(UC_X86_REG_ESP)) - 5
            self.calls.append([0x45EA00, site, actor,
                               *[self.peek(transform + i * 4) for i in range(6)]])
        self.shot_actor = actor
        # Real 45EA00 executes.

    def spawn(self):
        transform, actor, kind = self.args(3)
        self.calls.append([0x46FB40, kind, self.peek(self.burst(actor))])
        self.change(actor)
        if self.mutation == 2:
            self.poke(transform + 12, self.peek(transform + 12) + 11)
            self.poke(transform + 16, self.peek(transform + 16) + 13)
        self.finish()

    def melee(self):
        actor = self.args(1)[0]
        self.calls.append([0x470B00])
        self.change(actor)
        self.finish()

    def place(self):
        self.calls.append([0x470F60, *self.args(5)])
        self.change(self.peek(G['actor_ptr']))
        self.finish()

    def crt_random(self):
        self.calls.append([0x49AF6C])
        if self.mutation == 7:
            self.poke(G['actor_ptr'], A1)
            self.poke(A1 + 0x30, 0x40000000)
            self.poke(0xC239E8, 123)
        self.finish(self.crt)

    def steady_query(self):
        self.calls.append([0x484740])
        if self.mutation == 5:
            self.poke(G['actor_ptr'], A1)
            self.poke(G['eye'], self.peek(G['eye']) + 256)
        self.finish(self.steady)

    def muzzle(self):
        self.calls.append([0x482D90])
        for i, value in enumerate([444, 555, 666]):
            self.poke(0xC23A80 + i * 4, value)
        if self.mutation == 6:
            self.poke(ACTOR + 0x94, (self.peek(ACTOR + 0x94) + 1) % 39)
        self.finish()

    def barrel(self):
        self.calls.append([0x4847D0])
        for address, value in zip(self.args(3), [111, 222, 333]):
            self.poke(address, value)
        self.finish()

    def animated(self):
        actor = self.args(1)[0]
        self.calls.append([0x45E7A0, actor])
        self.change(actor)
        self.finish()

    def sound(self):
        self.calls.append([0x43E7D0, self.args(1)[0]])
        self.change(self.peek(G['actor_ptr']))
        self.finish()

    def sound2(self):
        self.calls.append([0x43E720, self.args(1)[0]])
        self.change(self.peek(G['actor_ptr']))
        self.finish()

    def queue(self):
        self.calls.append([0x46F3E0, self.args(1)[0]])
        self.change(self.peek(G['actor_ptr']))
        self.finish()

    def message(self):
        self.calls.append([0x479F00, 0])
        self.change(self.peek(G['actor_ptr']))
        self.finish()

    def input_query(self):
        self.calls.append([0x47A290, self.args(1)[0]])
        if self.mutation == 8:
            self.poke(G['aim_block'], 1)
        self.finish(self.held)

    def setup(self, weapon):
        self.set_slot_map([weapon] * 10)
        self.resupply(False)
        self.poke(A1 + 0xC, P1)
        self.poke(ACTOR + 0x94, weapon)
        self.poke(A1 + 0x94, weapon)
        for actor, words in [(ACTOR, [100, 200, 300, 0x12345678, 0x23456789, 0x3456789A]),
                             (A1, [1100, 1200, 1300, 0x456789AB, 0x56789ABC, 0x6789ABCD])]:
            for index, word in enumerate(words):
                self.poke(actor + 0x24 + index * 4, word)
        self.calls.clear()

    def snapshot_full(self):
        result = [self.peek(G['fire_timer']), self.peek(G['reload_timer']), self.peek(G['raise']),
                  self.peek(G['aim_block']), self.peek(ACTOR_DEF + 0x6C), self.peek(P1 + 0x6C),
                  int(self.peek(ACTOR + 0xC) == P1), self.peek(G['actor_ptr']), self.peek(G['eye'])]
        result += [self.peek(0xC239E8 + i * 4) for i in range(3)]
        for actor in [ACTOR, A1]:
            result += [self.peek(actor + 0x24 + i * 4) for i in range(6)]
        result.append(self.peek(RNG))
        for index in range(28):
            result += [self.peek(G[key] + index * 4) for key in ['magazine', 'reserve', 'pending']]
        result.append(self.peek(G['slot_map'] + 4 * self.peek(G['slot'])))
        return result

    def run(self, mode, values):
        self.reset()
        self.mode, self.mutation, self.steady, self.crt, self.held = mode, 0, 0, 0, 0
        self.calls = []
        if mode == 'aim':
            block, sx, sy, seed, self.mutation = values
            for address, value in zip([G['aim_block'], 0xC23B68, 0xC23B6C, RNG], [block, sx, sy, seed]):
                self.poke(address, value)
            self.call(0x484C20)
            state = [self.peek(a) for a in [G['aim_block'], 0xC23B68, 0xC23B6C, RNG]]
        elif mode == 'shot':
            weapon, b0, b1, person, yaw, pitch, seed, self.mutation = values
            self.setup(weapon)
            self.poke(ACTOR + 0xC, [ACTOR_DEF, P1][person])
            self.poke(ACTOR_DEF + 0x6C, b0)
            self.poke(P1 + 0x6C, b1)
            self.poke(RNG, seed)
            for i, word in enumerate([0, 0, 0, yaw, pitch, 0]):
                self.poke(TRANSFORM + i * 4, word)
            self.call(0x45EA00, ACTOR, TRANSFORM)
            state = [self.peek(ACTOR_DEF + 0x6C), self.peek(P1 + 0x6C),
                     int(self.peek(ACTOR + 0xC) == P1), self.peek(TRANSFORM + 12),
                     self.peek(TRANSFORM + 16), self.peek(RNG)]
        else:
            weapon = values[0]
            self.setup(weapon)
            if mode == 'view':
                _, camera, self.steady, self.crt, self.mutation, burst, seed = values
                self.poke(ACTOR_DEF + 0x6C, burst)
                self.poke(RNG, seed)
                self.poke(G['camera_mode'], camera)
                self.call(0x483C70)
            else:
                (_, fire, reload, raise_, burst, aim, unlimited, self.held, self.mutation,
                 camera, self.steady, self.crt, seed) = values
                for address, word in zip([G['fire_timer'], G['reload_timer'], G['raise'],
                                          ACTOR_DEF + 0x6C, G['aim_block'], G['camera_mode'], RNG],
                                         [fire, reload, raise_, burst, aim, camera, seed]):
                    self.poke(address, word)
                self.set_unlimited(unlimited)
                self.call(0x482B00)
            state = self.snapshot_full()
        return {'state': state, 'calls': self.calls}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('game', type=Path)
    p.add_argument('probe', type=Path)
    p.add_argument('--report', type=Path)
    a = p.parse_args()
    rig = Rig(a.game)
    cases = []
    rng = random.Random(0x482B00484C20)
    for i in range(600):
        cases.append(('aim', [rng.choice([0, 0, 1, 7, 0xFFFFFFFF]), rng.getrandbits(32),
                              rng.getrandbits(32), rng.getrandbits(32), i % 2]))
    for weapon in range(39):
        for burst in [0, 1, 2, 0x80000000, 0xFFFFFFFF]:
            for mutation in range(5):
                cases.append(('shot', [weapon, burst, 27, 0, rng.getrandbits(32),
                                       rng.getrandbits(32), rng.getrandbits(32), mutation]))
    for weapon in range(39):
        for camera in [0, 1]:
            for steady in [0, 1]:
                for mutation in range(10):
                    cases.append(('view', [weapon, camera, steady, rng.getrandbits(32),
                                           mutation, 0, rng.getrandbits(32)]))
    edge = [0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
    for i in range(1500):
        cases.append(('prefix', [rng.randrange(39), rng.choice(edge), rng.choice(edge),
                                  rng.choice(edge), rng.choice([0, 0, 1, 2]), rng.randrange(2),
                                  rng.randrange(2), rng.randrange(2), rng.randrange(10),
                                  rng.randrange(2), rng.randrange(2), rng.getrandbits(32), rng.getrandbits(32)]))
    lines = [mode + '|' + ','.join(map(str, values)) for mode, values in cases]
    native = subprocess.run([str(a.probe.resolve())], input='\n'.join(lines) + '\n', text=True,
                            capture_output=True, check=True).stdout.splitlines()
    if len(native) != len(cases):
        raise AssertionError(f'Native row count {len(native)} != {len(cases)}')
    counts = {}
    for index, ((mode, values), row) in enumerate(zip(cases, native)):
        expected = rig.run(mode, values)
        actual = json.loads(row)
        if actual != expected:
            raise AssertionError(f'Case {index}: {lines[index]}\noriginal={expected}\nnative={actual}')
        counts[mode] = counts.get(mode, 0) + 1
    report = {'source_sha256': 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff',
              'cases': counts, 'total': len(cases), 'status': 'passed',
              'shared_rng': 'real 477860', 'sincos': 'real 477510 table and4776C0',
              'foreign_bodies': 'declared input/CRT/muzzle/animated/spawn/melee/sound/message fixtures'}
    if a.report:
        a.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
