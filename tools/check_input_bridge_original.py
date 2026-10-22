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
        if self.mutation&1:
            self._write_u32(INPUT_MODE, 1)
            self._write_u32(DIGIT_FLAG, 0)
        return 0

    def __init__(self, source, bindings):
        self.split_enabled=False
        super().__init__(source)
        self.bindings = bindings
        self._return_from(0x479A80, self.chat)
        def pulse_hook(uc,address,size,user):
            if self.split_enabled:
                event=self._read_u32(uc.reg_read(UC_X86_REG_ESP)+4)
                self.order.append(f'P,{event}')
        self.u.hook_add(UC_HOOK_CODE,pulse_hook,begin=PULSE,end=PULSE)

    def _install_stubs(self):
        # Explicit source helper fixtures; DISPATCH can run the real PULSE body.
        self._return_from(STRING_GET, SCRATCH + 0xf000)
        self._return_from(RESET_MOUSE, 0)
        self._return_from(READ_AXES, 0)
        self._return_from(STEADY, lambda: self.steady_value)
        self._log_from(DIGIT_HANDLER, 'digit', 1)
        self._log_from(FLIGHT_PAGE, 'flight_page', 1)
        self._log_from(MENU_PAGE, 'menu_page', 1)
        def dispatch_hook(uc,address,size,user):
            sp=uc.reg_read(UC_X86_REG_ESP);ret=self._read_u32(sp)
            values=list(struct.unpack('<3I',self._read(sp+4,12)));self.dispatch_log.append(tuple(values))
            if self.split_enabled:self.order.append('D,'+','.join(map(str,values)))
            if self.split_enabled and self.phase=='consume' and self.budget:
                self.budget-=1
                if self.mutation&2:
                    self._write_u32(MENU_FLAG,self._read_u32(MENU_FLAG)^1)
                    actor=self._read_u32(ACTOR_PTR);self._write_u32(actor+0x6c,self._read_u32(actor+0x6c)^4)
                # Execute original47A150 in the same machine and call stack.
                # Its return trampoline removes its argument before the original
                # 479450 caller resumes with the three dispatch arguments intact.
                trampoline=SCRATCH+0xe000
                self.u.mem_write(trampoline,b'\x83\xc4\x04\x31\xc0\x68'+struct.pack('<I',ret)+b'\xc3')
                self._write_u32(sp-4,trampoline);self._write_u32(sp,self.during)
                uc.reg_write(UC_X86_REG_ESP,sp-4);uc.reg_write(UC_X86_REG_EIP,PULSE)
            else:
                uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EAX,0);uc.reg_write(UC_X86_REG_EIP,ret)
        self.u.hook_add(UC_HOOK_CODE,dispatch_hook,begin=DISPATCH,end=DISPATCH)

    def chat(self):
        if self._read_u32(INPUT_MODE) != 1:
            return 0
        sp = self.u.reg_read(UC_X86_REG_ESP) - 4  # _return_from has popped the return address
        character, key = struct.unpack('<2I', self._read(sp + 4, 8))
        self.chats.append([character, key])
        if self.mutation&1:
            self._write_u32(INPUT_MODE, 0)
            # Declared chat fixture appends the KEY_PRESS(34,0) producer state.
            self.u.mem_write(KEY_STATE + 34, b'\x01')
            write = self._read_u32(0xD096B4)
            slot = 0xD094B0 + write * 4
            self._write_u32(slot, 34)
            self._write_u32(0xD096B4, (write + 1) & 127)
            self._write_u32(0xD096B8, slot)
        return 0

    def run(self, head, events, split=None):
        self.split_enabled=False
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
                               (MENU_FLAG, menu), (DIGIT_FLAG, 1 if self.mutation&1 else 0)]:
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
        if split:
            consume_tick,between,self.during,self.budget=split
            self.split_enabled=True;self.phase='begin';self.order=['B']
            self._call(FRAME_PUSH,[tick])
            def queued():return [list(struct.unpack('<4I',self._read(QUEUE+i*0x20,16))) for i in range(self._read_u32(QUEUE_COUNT))]
            begin=queued();self.phase='between'
            if between:self._call(PULSE,[between])
            self.order.append('C');self.phase='consume';self._call(FRAME_CONSUME,[consume_tick]);self.split_enabled=False
            return {'begin':begin,'calls':[list(call) for call in self.dispatch_log],'queue':queued(),'order':self.order,
                    'state':[self.dx,self.dy,self._read_u32(WINDOW_ACTIVE),self._read_u32(MOUSE_BUTTONS),self._read_u32(MENU_FLAG),self._read_u32(actor+0x6c)]}
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
    split_cases=[]
    for budget in (0,1,2,31,32,40):
        for between in (0,2,8,0x20,0x100,0x200):
            for consume_tick in (0,9,10):
                split_cases.append(([10,0,1,0,1,1,0,0,0,0,2],[(0,17,0),(4,3,-4)],(consume_tick,between,8,budget)))
    for tick in (0x7fffffff,0x80000000,0xffffffff):
        for consume_tick in (0,tick,(tick-1)&0xffffffff):
            split_cases.append(([tick,0,1,0,1,1,0,0,0,0,0],[(2,1,0)],(consume_tick,8,0x200,2)))
    for count in (0,1,30,31,32,33,64):
        split_cases.append(([100,0,1,0,1,1,0,0,0,0,0],[(2,1,0)]*count,(99,8,0x200,10)))
    for i in range(1200):
        tick=rng.choice([0,1,10,0x7fffffff,0x80000000,0xffffffff])
        head=[tick,rng.randrange(3),rng.randrange(2),rng.randrange(2),rng.randrange(2),rng.choice([0,1,128,-1]),rng.randrange(2),rng.randrange(2),rng.randrange(2),rng.choice([0,4]),rng.choice([0,2])]
        events=[]
        for _ in range(rng.randrange(36)):
            kind=rng.randrange(6)
            if kind in (0,1):events.append((kind,rng.choice(keys),rng.randrange(2)))
            elif kind in (2,3):events.append((kind,rng.randrange(4),0))
            elif kind==4:events.append((kind,rng.randrange(-100,101),rng.randrange(-100,101)))
            else:events.append((kind,0,0))
        split=(rng.choice([0,tick,(tick-1)&0xffffffff]),rng.choice([0,2,8,0x20,0x100,0x200]),rng.choice([2,8,0x20,0x100,0x200]),rng.choice([0,1,2,8,40]))
        split_cases.append((head,events,split))
    split_lines=[' '.join(map(str,head+list(split)+[len(events)]+[n for event in events for n in event])) for head,events,split in split_cases]
    process=subprocess.run([str(args.probe.resolve()),'split-oracle'],input='\n'.join(split_lines)+'\n',text=True,capture_output=True)
    if process.returncode:raise RuntimeError(process.stderr)
    split_rows=process.stdout.splitlines();assert len(split_rows)==len(split_cases)
    injected=remaining=0
    for i,((head,events,split),row) in enumerate(zip(split_cases,split_rows)):
        expected,actual=rig.run(head,events,split),json.loads(row)
        if expected!=actual:
            if args.report:args.report.with_suffix('.failure.json').write_text(json.dumps({'case':i,'input':split_lines[i],'expected':expected,'actual':actual},indent=2)+'\n')
            raise AssertionError(f'split case{i}: {split_lines[i]}\noriginal={expected}\nnative={actual}')
        injected+=sum(entry.startswith('P,') for entry in expected['order'][expected['order'].index('C')+1:]);remaining+=bool(expected['queue'])
    report = {'status': 'passed', 'cases': len(cases), 'split_cases':len(split_cases),'during_dispatch_real_pulses':injected,'split_cases_with_remaining_queue':remaining,'source_sha256': SOURCE_HASH,
              'real_bodies': '41CE20,47A300,498AE0,498A60,498A70,498B00,498B20,498B80,47A150,479FA0,47A000,47A070,479750,479450,47A290',
              'providers': 'axes, steady, dispatch, digit and chat bodies; callback source mutation; real47A150 executed between479750/479450 and during477D40 fixtures','file_replay_fixture':'BD4840=1, no remaining file records','dispatch_fixture':'477D40 capture plus an optional real47A150 call on the original stack through a balanced return trampoline'}
    if args.report: args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__': main()
