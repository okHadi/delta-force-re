#!/usr/bin/env python3
"""Replay platform events through original key ring and input-frame bodies."""
import argparse
import hashlib
import json
import random
import struct
import subprocess
from pathlib import Path
from check_input_state_original import *
from check_input_config_original import Original as ConfigOriginal
from pff import Archive


class Rig(Original):
    def _return_from(self, address, value):
        if address == READ_AXES:
            value = self.read_axes
        super()._return_from(address, value)

    def read_axes(self):
        self._write_u32(AXIS_X, self.dx)
        self._write_u32(AXIS_Y, self.dy)
        self.dx = self.dy = 0
        return 0

    def _log_from(self, address, name, args=0):
        if address != DIGIT_HANDLER:
            return super()._log_from(address, name, args)
        self._return_from(address, self.digit)

    def digit(self):
        if self.mutation:
            self._write_u32(INPUT_MODE, 1)
            self._write_u32(DIGIT_FLAG, 0)
        return 0

    def __init__(self, source, bindings):
        super().__init__(source)
        self.bindings = bindings
        self._return_from(0x479A80, self.chat)

    def chat(self):
        if self._read_u32(INPUT_MODE) != 1:
            return 0
        sp = self.u.reg_read(UC_X86_REG_ESP) - 4  # _return_from has popped the return address
        character, key = struct.unpack('<2I', self._read(sp + 4, 8))
        self.chats.append([character, key])
        if self.mutation:
            self._write_u32(INPUT_MODE, 0)
            # Declared chat fixture appends the KEY_PRESS(34,0) producer state.
            self.u.mem_write(KEY_STATE + 34, b'\x01')
            write = self._read_u32(0xD096B4)
            slot = 0xD094B0 + write * 4
            self._write_u32(slot, 34)
            self._write_u32(0xD096B4, (write + 1) & 127)
            self._write_u32(0xD096B8, slot)
        return 0

    def run(self, head, events):
        self._zero_state()
        self.restore_table()
        base = SCRATCH + 0x40000
        for i, binding in enumerate(self.bindings):
            for j, value in enumerate(binding):
                self._write_u32(base + 0x6A0 + i * 12 + j * 4, value)
        self._write_u32(PROFILE_PTR, base)
        self._call(APPLY_BINDINGS, [])
        self._write_u32(WINDOWED, 1)
        self._call(INPUT_INIT, [])
        tick, mode, live, windowed, active, scale, flip, steady, menu, actor_flags, self.mutation = head
        for address, value in [(INPUT_MODE, mode), (RUN_FLAG, 0 if live else 1), (WINDOWED, windowed),
                               (WINDOW_ACTIVE, active), (MOUSE_SCALE, scale), (MOUSE_FLIP, flip),
                               (MENU_FLAG, menu), (DIGIT_FLAG, 1 if self.mutation else 0)]:
            self._write_u32(address, value)
        actor = SCRATCH + 0x50000
        self._write_u32(ACTOR_PTR, actor)
        self._write_u32(actor + 0x6C, actor_flags)
        self._write_u32(0xD096BC, 0)
        self._write_u32(0xBD4840, 1)  # Declared replay fixture has no remaining file records.
        self.steady_value = steady
        self.dx = self.dy = 0
        self.chats = []
        masks = {0: 1, 1: 1, 2: 2, 3: 0x10}
        pulses = {0: 2, 1: 2, 2: 8, 3: 0x20}
        for kind, a, b in events:
            if kind in [0, 1]:
                self._call(KEY_PRESS if kind == 0 else 0x498A60, [a, 0x01000000 if b else 0])
            elif kind in [2, 3]:
                old = self._read_u32(MOUSE_BUTTONS)
                self._write_u32(MOUSE_BUTTONS, old | masks.get(a, 0) if kind == 2 else old & ~masks.get(a, 0))
                if kind == 2 and a in pulses:
                    self._call(PULSE, [pulses[a]])
            elif kind == 4:
                self.dx = (self.dx + a) & 0xFFFFFFFF
                self.dy = (self.dy + b) & 0xFFFFFFFF
            elif kind == 5:
                self._call(0x498B80, [])
                self._write_u32(WINDOW_ACTIVE, 0)
            elif kind == 6:
                self._call(0x498B00, [a])
            elif kind == 7:
                self._write_u32(0xD096BC, a)
        self._call(FRAME_PUSH, [tick])
        self._call(FRAME_CONSUME, [tick])
        # The source executable has zero query fields at row140, separate from
        # command ID140's profile row. Execute the actual47A290 body.
        query = self._call(0x47A290, [0x8C])
        return {'calls': [list(call) for call in self.dispatch_log], 'chat': self.chats,
                'state': [self.dx, self.dy, self._read_u32(WINDOW_ACTIVE), self._read_u32(MOUSE_BUTTONS), query]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    source = (args.game / 'Df.exe').read_bytes()
    assert hashlib.sha256(source).hexdigest() == SOURCE_HASH
    for name in ['Dfbase.pff', 'Df.pff']:
        archive = Archive(args.game / name)
        entries = [e for e in archive.entries if not e.deleted and e.name.lower() == 'dfdk.key']
        if entries:
            with archive.path.open('rb') as file:
                file.seek(entries[0].offset)
                bindings = ConfigOriginal(source).load_keys(file.read(entries[0].size))
            break
    rig, rng = Rig(source, bindings), random.Random(0x498AE0479750)
    cases = []
    for count in [0, 1, 32, 127, 128, 129, 255, 256, 257]:
        cases.append(([1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0], [(0, 17, 0)] * count))
    cases += [([10, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1], [(0, 49, 0), (0, 50, 0)]),
              ([11, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0], [(0, 65, 0), (6, 97, 0)]),
              ([12, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0], [(0, 17, 0), (4, 3, 4), (2, 1, 0), (5, 0, 0)])]
    keys = [16, 17, 34, 38, 40, 49, 50, 65, 80, 83, 87]
    for i in range(1200):
        head = [i + 20, rng.randrange(3), rng.randrange(2), rng.randrange(2), rng.randrange(2),
                rng.choice([0, 1, 128, -1]), rng.randrange(2), rng.randrange(2), rng.randrange(2),
                rng.choice([0, 4]), 0]
        events = []
        for _ in range(rng.randrange(30)):
            kind = rng.randrange(7)
            if kind in [0, 1]: events.append((kind, rng.choice(keys), rng.randrange(2)))
            if kind in [2, 3]: events.append((kind, rng.randrange(4), 0))
            if kind == 4: events.append((kind, rng.choice([-2147483648, -1, 0, 1, 2147483647]), rng.choice([-2147483648, -1, 0, 1, 2147483647])))
            if kind == 5: events.append((kind, 0, 0))
            if kind == 6: events.append((kind, rng.randrange(128), 0))
        cases.append((head, events))
    cases.append(([13, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0], [(7, 1, 0), (0, 34, 0), (0, 38, 0)]))
    lines = [' '.join(map(str, head + [len(events)] + [n for event in events for n in event])) for head, events in cases]
    process = subprocess.run([str(args.probe.resolve()), 'oracle'], input='\n'.join(lines) + '\n', text=True, capture_output=True)
    if process.returncode: raise RuntimeError(process.stderr)
    rows = process.stdout.splitlines()
    assert len(rows) == len(cases)
    for i, ((head, events), row) in enumerate(zip(cases, rows)):
        expected, actual = rig.run(head, events), json.loads(row)
        if actual != expected:
            raise AssertionError(f'case{i}: {lines[i]}\noriginal={expected}\nnative={actual}')
    report = {'status': 'passed', 'cases': len(cases), 'source_sha256': SOURCE_HASH,
              'real_bodies': '41CE20,47A300,498AE0,498A60,498A70,498B00,498B20,498B80,479750,479450,47A290',
              'providers': 'axes, steady, dispatch, digit and chat bodies; callback source mutation'}
    if args.report: args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__': main()
