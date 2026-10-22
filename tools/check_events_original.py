#!/usr/bin/env python3
"""Compare the event stage with the original DF1 x86 event code.

Runs tools/events_probe (dfre::EventStage) and the original functions from
Df.exe under unicorn on the same fixtures:

  0x42ec90 load + 0x42eea0 / 0x42f020 post-load binders
  0x42eaf0 tick, 0x42eb60 bit 1 pass, 0x42eba0 bit 2 pass
  0x443c8c-0x443cab frame driver counter
  0x42e9e0 / 0x42e930 / 0x42e620 / 0x42e810 runtime

Only event subsystem code runs for real. Unknown callees are stubbed and
every stub call is compared through the probe's call log. Marker records for
the binders are synthesized from the fixture fields the binder reads
(type 0x177d, raw byte 0x4a, position x/y) because the 96 -> 0x114 byte
object converter (0x449710) is outside this scope.

Real BMS cases use the checked mission decoder and local game archives.
Per-identifier counts come from decoded actor groups;7936A4 is a fixture.
The original allocator runs against defined fresh and occupied heap layouts.
Foreign effects are ordered fixtures with explicit mutations.
"""

import argparse
import hashlib
import json
import random
import struct
import subprocess
import sys
from pathlib import Path

import pefile
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_MEM_READ
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EBX,
                               UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_EDX,
                               UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_ESP)

SOURCE_HASH = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
IMAGE_BASE = 0x400000
IMAGE_SIZE = 0x1000000
STACK = 0x2000000
DONE = 0x2100000
HEAP = 0x2200000
HEAP_SIZE = 0x20000
STREAM = 0x2300000
STREAM_SIZE = 0x20000
TYPEINFO = 0x2400000

FUNCTION_RESET = 0x42ebe0
FUNCTION_LOAD = 0x42ec90
FUNCTION_TICK = 0x42eaf0
FUNCTION_BIT1 = 0x42eb60
FUNCTION_BIT2 = 0x42eba0
FRAME_COUNTER_BEGIN = 0x443c8c
FRAME_COUNTER_END = 0x443cab
FUNCTION_ALLOC = 0x497f60
FUNCTION_FREE = 0x4980d0
FUNCTION_READ = 0x499280
FUNCTION_MEMSET = 0x49a620

GLOBAL_EVENT_COUNT = 0x4f2bc8
GLOBAL_EVENTS = 0x4f2bcc
GLOBAL_TRIGGER_COUNT = 0x4f2bd0
GLOBAL_TRIGGERS = 0x4f2bd4
GLOBAL_ACTION_COUNT = 0x4f2bd8
GLOBAL_ACTIONS = 0x4f2bdc
GLOBAL_LOADER_FLAG = 0x4f2be0
GLOBAL_CURSOR = 0x4f2be4
GLOBAL_MARKERS = 0x8ace80
GLOBAL_MARKER_COUNT = 0x8cf900
GLOBAL_FRAME_COUNTER = 0x6e4734
GLOBAL_WORLD_MODE = 0x67f520
GLOBAL_WORLD_FLAG = 0x67f549
FLAG_2D_A = 0x6e0050
FLAG_2D_B = 0x6d7e50
FLAG_3D = 0x6d8050
FLAG_6E0250 = 0x6e0250
FLAG_6E0254 = 0x6e0254
FLAG_67DBEC = 0x67dbec
UNIT_RECORD_BASE = 0x793640
UNIT_RECORD_SIZE = 0x1040

# Stub targets. kind 'probe' returns a scripted answer and logs; 'call' only
# logs. Pointer arguments are converted back to table indexes for the log.
STUBS = {
    0x460330: ('p330', 1, 'trigger'),
    0x460560: ('p560', 2, 'dwords'),
    0x45f670: ('d45f670', 2, 'dwords'),
    0x45f4c0: ('d45f4c0', 1, 'dwords'),
    0x4601a0: ('d4601a0', 1, 'action'),
    0x45f5b0: ('d45f5b0', 1, 'dwords'),
    0x404dd0: ('d404dd0', 1, 'dwords'),
    0x43f500: ('d43f500', 1, 'dwords'),
    0x444ca0: ('d444ca0', 1, 'dwords'),
    0x460640: ('d460640', 2, 'dwords'),
    0x460280: ('d460280', 1, 'action'),
}


class Case:
    """One fixture: tables, marker group, host state, flag sets, steps."""

    def __init__(self, events, triggers, actions=(), markers=(), dwords=(), world=(1, 0),
                 loader=0, cursor=0, sets=(), answers_330=(), answers_560=(), steps=(), mutations=(), counter=0):
        self.events = [list(event) for event in events]
        self.triggers = [list(trigger) for trigger in triggers]
        self.actions = [list(action) for action in actions]
        self.markers = [list(marker) for marker in markers]
        self.dwords = [list(dword) for dword in dwords]
        self.world = tuple(world)
        self.loader = loader
        self.cursor = cursor
        self.counter=counter
        self.sets = [list(entry) for entry in sets]
        self.answers_330 = list(answers_330)
        self.answers_560 = list(answers_560)
        self.steps = list(steps)
        self.mutations=list(mutations)

    def feed(self):
        tokens = [str(len(self.events)), str(len(self.triggers)), str(len(self.actions))]
        tokens += [str(word) for event in self.events for word in event]
        tokens += [str(word) for trigger in self.triggers for word in trigger]
        tokens += [str(word) for action in self.actions for word in action]
        tokens.append(str(len(self.markers)))
        tokens += [str(field) for marker in self.markers for field in marker]
        tokens.append(str(len(self.dwords)))
        tokens += [str(field) for dword in self.dwords for field in dword]
        tokens += [str(self.world[0]), str(self.world[1]), str(self.loader), str(self.cursor),str(self.counter)]
        tokens.append(str(len(self.sets)))
        tokens += [str(field) for entry in self.sets for field in (list(entry) + [0] * (4 - len(entry)))]
        tokens.append(str(len(self.answers_330)))
        tokens += [str(value) for value in self.answers_330]
        tokens.append(str(len(self.answers_560)))
        tokens += [str(value) for value in self.answers_560]
        tokens.append(str(len(self.steps)))
        tokens += [str(step) for step in self.steps]
        tokens.append(str(len(self.mutations)))
        tokens += [str(n) for mutation in self.mutations for n in mutation]
        return ' '.join(tokens)


class Original:
    def __init__(self, data):
        pe = pefile.PE(data=data)
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.uc.mem_map(IMAGE_BASE, IMAGE_SIZE)
        self.uc.mem_write(IMAGE_BASE, pe.get_memory_mapped_image())
        pe.close()
        self.uc.mem_map(STACK, 0x10000)
        self.uc.mem_map(DONE, 0x1000)
        self.uc.mem_map(HEAP, HEAP_SIZE)
        self.uc.mem_map(STREAM, STREAM_SIZE)
        self.uc.mem_map(TYPEINFO, 0x10000)
        self.log = []
        self.answers_330 = []
        self.answers_560 = []
        self.at_330 = 0
        self.at_560 = 0
        self.stream = b''
        self.stream_at = 0
        self.heap_at = 0
        self.triggers_base = 0
        self.actions_base = 0
        for address in (FUNCTION_FREE, FUNCTION_READ, FUNCTION_MEMSET, *STUBS):
            self.uc.hook_add(unicorn.UC_HOOK_CODE, self.callback, begin=address, end=address + 1)

        self.uc.hook_add(UC_HOOK_MEM_READ,self.external_read,begin=HEAP,end=HEAP+HEAP_SIZE-1)

    def external_read(self,uc,access,address,size,value,user):
        if uc.reg_read(UC_X86_REG_EIP)!=0x42e77f:return
        offset=address-self.u32(GLOBAL_EVENTS)-20
        if offset>=0 and offset%24==0 and offset//24>=self.u32(GLOBAL_EVENT_COUNT):
            self.log.append(f'external:{offset//24}:{self.uc.mem_read(address,1)[0]}')

    # --- guest memory helpers ------------------------------------------------
    def write(self, address, data):
        self.uc.mem_write(address, data)

    def u32(self, address):
        return struct.unpack('<I', self.uc.mem_read(address, 4))[0]

    def put_u32(self, address, value):
        self.write(address, struct.pack('<I', value & 0xffffffff))

    def put_u16(self, address, value):
        self.write(address, struct.pack('<H', value & 0xffff))

    def put_u8(self, address, value):
        self.write(address, struct.pack('<B', value & 0xff))

    # --- stubs ---------------------------------------------------------------
    def callback(self, uc, address, size, user):
        if address == DONE:
            return
        if address in (FUNCTION_FREE,):
            self.return_value(0, 4)
            return
        if address == FUNCTION_READ:
            dest, size = self.args(2)
            chunk = self.stream[self.stream_at:self.stream_at + size]
            if len(chunk) != size:
                raise ValueError('Stream exhausted')
            self.stream_at += size
            self.write(dest, chunk)
            self.return_value(0, 8)
            return
        if address == FUNCTION_MEMSET:
            dest, value, size = self.args(3)
            self.write(dest, bytes([value & 0xff]) * size)
            self.return_value(dest, 12)
            return
        if address in STUBS:
            name, count, kind = STUBS[address]
            values = self.args(count)
            if kind == 'trigger':
                index = (values[0] - self.triggers_base) // 0x20
                self.log.append(f'p330:{index}')
                value = self.answers_330[self.at_330 % len(self.answers_330)] if self.answers_330 else 0
                self.at_330 += 1
                self.mutate(address)
                self.return_value(value, count * 4)
            elif kind == 'action':
                index = (values[0] - self.actions_base) // 0x20
                self.log.append(f'{name}:{index}')
                self.mutate(address)
                self.return_value(0, count * 4)
            elif name == 'p560':
                self.log.append(f'p560:{values[0]}:{values[1]}')
                value = self.answers_560[self.at_560 % len(self.answers_560)] if self.answers_560 else 0
                self.at_560 += 1
                self.mutate(address)
                self.return_value(value, count * 4)
            else:
                self.log.append(f'{name}:' + ':'.join(str(value) for value in values))
                self.mutate(address)
                self.return_value(0, count * 4)
            return
        raise ValueError(f'Unexpected call to {address:#x}')

    def mutate(self,site):
        occurrence=self.calls.get(site,0)+1;self.calls[site]=occurrence
        for at,nth,table,index,word,value in self.mutations:
            if at!=site or nth!=occurrence:continue
            bases=[self.u32(GLOBAL_EVENTS),self.triggers_base,self.actions_base]
            if table==0 and word in (1,2):value=bases[word]+value*32
            self.put_u32(bases[table]+index*(24 if table==0 else 32)+word*4,value)

    def args(self, count):
        sp = self.uc.reg_read(UC_X86_REG_ESP)
        return [struct.unpack('<I', self.uc.mem_read(sp + 4 + 4 * index, 4))[0] for index in range(count)]

    def return_value(self, value, arg_bytes=0):
        # Pop only the return address: the original callers run their own
        # `add esp, ...` cleanup after every call.
        sp = self.uc.reg_read(UC_X86_REG_ESP)
        return_address = struct.unpack('<I', self.uc.mem_read(sp, 4))[0]
        self.uc.reg_write(UC_X86_REG_EIP, return_address)
        self.uc.reg_write(UC_X86_REG_ESP, sp + 4)
        self.uc.reg_write(UC_X86_REG_EAX, value & 0xffffffff)

    def run(self, entry, until=None):
        sp = STACK + 0x8000
        if until is None:
            self.put_u32(sp, DONE)
            self.uc.reg_write(UC_X86_REG_ESP, sp)
            self.uc.emu_start(entry, DONE, timeout=0, count=20000000)
            if self.uc.reg_read(UC_X86_REG_EIP) != DONE:
                raise ValueError(f'{entry:#x} did not return')
        else:
            self.uc.reg_write(UC_X86_REG_ESP, sp)
            self.uc.emu_start(entry, until, timeout=0, count=20000000)
            if self.uc.reg_read(UC_X86_REG_EIP) != until:
                raise ValueError(f'{entry:#x} did not reach {until:#x}')

    # --- case setup ----------------------------------------------------------
    def prepare(self, case):
        self.log = []
        self.mutations=case.mutations;self.calls={}
        self.answers_330 = list(case.answers_330)
        self.answers_560 = list(case.answers_560)
        self.at_330 = 0
        self.at_560 = 0
        self.heap_at = 0
        # Execute497F60 with its real header/alignment against a defined fresh heap.
        self.write(HEAP,bytes(HEAP_SIZE));context=TYPEINFO+0xf000
        self.put_u32(0x5013a0,context)
        for offset,value in ((0,HEAP),(4,HEAP_SIZE),(8,HEAP),(12,0),(16,0)):self.put_u32(context+offset,value)
        self.stream_at = 0
        counts = struct.pack('<III', len(case.events), len(case.triggers), len(case.actions))
        events = b''.join(struct.pack('<I', word) for event in case.events for word in event)
        tables = b''.join(struct.pack('<I', word) for record in case.triggers + case.actions for word in record)
        self.stream = counts + events + tables
        self.write(GLOBAL_WORLD_MODE, struct.pack('<I', case.world[0]))
        self.put_u8(GLOBAL_WORLD_FLAG, case.world[1])
        self.put_u8(GLOBAL_LOADER_FLAG, case.loader)
        self.put_u32(GLOBAL_CURSOR, case.cursor)
        self.put_u32(GLOBAL_FRAME_COUNTER, case.counter)
        self.put_u32(FLAG_67DBEC, 0)
        self.write(UNIT_RECORD_BASE, b'\0' * UNIT_RECORD_SIZE)
        for address, index, value in case.dwords:
            self.put_u32(address + index * 0x40, value)
        # Markers: the binder reads [record+4] (type pointer, +0x20 = type id),
        # [record+0x7c] (raw byte 0x4a) and [record+0x24]/[record+0x28] (x/y).
        self.write(GLOBAL_MARKERS, b'\0' * (max(64,len(case.markers)) * 0x114))
        self.put_u32(GLOBAL_MARKER_COUNT, len(case.markers))
        for at, marker in enumerate(case.markers):
            type_id, raw_4a, x, y = marker
            base = GLOBAL_MARKERS + at * 0x114
            if type_id == 0x177d:
                info = TYPEINFO + at * 0x40
                self.put_u32(info + 0x20, 0x177d)
                self.put_u32(base + 4, info)
            self.put_u32(base + 0x7c, raw_4a & 0xff)
            self.put_u32(base + 0x24, x & 0xffffffff)
            self.put_u32(base + 0x28, y & 0xffffffff)

    def load(self, case):
        self.run(FUNCTION_LOAD)
        self.triggers_base = self.u32(GLOBAL_TRIGGERS)
        self.actions_base = self.u32(GLOBAL_ACTIONS)
        for entry in case.sets:
            which = entry[0]
            if which == 0:
                self.set_2d(FLAG_2D_A, entry[1], entry[2])
            elif which == 1:
                self.set_2d(FLAG_2D_B, entry[1], entry[2])
            elif which == 2:
                word = ((entry[1] << 7) + entry[2]) & 0xffffffff
                self.set_word(FLAG_3D, word, entry[3])
            elif which == 3:
                self.set_word(FLAG_6E0250, 0, entry[1])
            elif which == 4:
                self.set_word(FLAG_6E0254, 0, entry[1])
            elif which == 5:
                self.set_word(FLAG_67DBEC, 0, entry[1])
            else:
                raise ValueError('Bad flag set kind')

    def set_word(self, base, word, bit):
        address = base + (word & 0xffffffff) * 4
        current = self.u32(address)
        self.put_u32(address, current | (1 << (bit & 31)))

    def set_2d(self, base, a, b):
        word = ((struct.unpack('<i', struct.pack('<I', b))[0] >> 5) + a * 2) & 0xffffffff
        self.set_word(base, word, b)

    def steps(self, case):
        for step in case.steps:
            if step == 't':
                self.run(FUNCTION_TICK)
            elif step == 'b':
                self.run(FUNCTION_BIT1)
            elif step == 'c':
                self.run(FUNCTION_BIT2)
            elif step == 'f':
                self.run(FRAME_COUNTER_BEGIN, until=FRAME_COUNTER_END)
            elif step == 'p':
                raise ValueError('process step is not emulated directly')
            else:
                raise ValueError('Bad step')

    # --- state read-back -----------------------------------------------------
    def state_line(self):
        count = self.u32(GLOBAL_EVENT_COUNT)
        events = self.u32(GLOBAL_EVENTS)
        triggers = self.u32(GLOBAL_TRIGGERS)
        trigger_count = self.u32(GLOBAL_TRIGGER_COUNT)
        actions = self.u32(GLOBAL_ACTIONS)
        action_count = self.u32(GLOBAL_ACTION_COUNT)
        words = []
        for index in range(count):
            record = list(struct.unpack('<6I', self.uc.mem_read(events + index * 24, 24)))
            record[1] = self.pointer_index(record[1], triggers, trigger_count)
            record[2] = self.pointer_index(record[2], actions, action_count)
            words.extend(record)
        line = 'E:' + ''.join(f'{word:08x}' for word in words)
        for label,base,count in [('T',triggers,trigger_count),('A',actions,action_count)]:
            line+='|'+label+':'+''.join(f'{self.u32(base+i*4):08x}' for i in range(count*8))
        line+=f'|C:{self.u32(GLOBAL_CURSOR):08x}:{self.u32(GLOBAL_FRAME_COUNTER):08x}|F:'
        for name, base, length in (('6e0050', FLAG_2D_A, 128), ('6d7e50', FLAG_2D_B, 128),
                                   ('6d8050', FLAG_3D, 8192)):
            for index in range(length):
                value = self.u32(base + index * 4)
                if value:
                    line += f'{name}={index}:{value:08x},'
        line += f'6e0250:{self.u32(FLAG_6E0250):08x},6e0254:{self.u32(FLAG_6E0254):08x},'
        line += f'67dbec:{self.u32(FLAG_67DBEC):08x},4f2be0:{self.u32(GLOBAL_LOADER_FLAG) & 0xff:08x}'
        line += '|L:' + ';'.join(self.log)
        return line

    @staticmethod
    def pointer_index(value, base, count):
        if value == 0:
            return 0
        if base and value >= base and (value - base) % 32 == 0:
            index = (value - base) // 32
            if index <= count:
                return index
        raise ValueError(f'Event word {value:#x} is not a {count} entry table pointer')


def run_case(original, case):
    original.prepare(case)
    original.load(case)
    original.steps(case)
    return original.state_line()


# --- fixture builders --------------------------------------------------------

def trigger(kind, subject, a=0, b=0, c=0, d=0):
    return [0, kind, subject, a, b, c, d, 0]


def event(w0, trigger_index, action_index, w3, w4, trigger_count, action_count, fired=0):
    return [w0, trigger_index, action_index, w3, w4,
            (fired & 0xff) | ((trigger_count & 0xff) << 8) | ((action_count & 0xff) << 16)]


def synthetic_cases():
    cases = []
    # Foreign calls can change fields which the next instruction reads.
    actions=[[0,1,0,3,4,0,0,0],[0,14,0,1,0,0,0,0],[0,15,0,2,0,0,0,0]]
    cases.append(Case([event(0,0,0,0,0,0,1)],[],actions,steps=['t'],
        mutations=[(0x45f670,1,0,0,0,1),(0x45f670,1,0,0,3,0x04000000)]))
    cases.append(Case([event(0,0,0,0x06000200,128,0,1)],[],actions,steps=['t'],
        mutations=[(0x45f670,1,0,0,3,0x06000400)]))
    cases.append(Case([event(0,0,0,0,0,0,3)],[],actions,steps=['t'],
        mutations=[(0x45f670,1,0,0,5,0x00010001)]))
    cases.append(Case([event(0,0,0,0,0,0,2)],[],actions,steps=['t'],
        mutations=[(0x45f670,1,0,0,2,1)]))
    negated=trigger(0,0);negated[0]=1
    cases.append(Case([event(0,0,0,0,0,1,1)],[trigger(1,11,10,20),negated],actions,
        steps=['t'],answers_560=[1],mutations=[(0x460560,1,0,0,1,1)]))
    cases.append(Case([event(0,0,0,0,0,3,1)],[trigger(1,11),trigger(0,0),trigger(0,0)],actions,
        steps=['t'],answers_560=[1],mutations=[(0x460560,1,0,0,5,0x00010100)]))
    cases.append(Case([event(0,0,0,0,0,2,1)],[trigger(0,0),trigger(1,11)],actions,
        steps=['t'],answers_560=[1],mutations=[(0x460560,1,1,0,0,2)]))
    cases.append(Case([event(0,0,0,0,0,2,1)],[trigger(1,11,10),trigger(1,11,20),trigger(1,11,30)],actions,
        steps=['t'],answers_560=[1,0],mutations=[(0x460560,1,0,0,1,1)]))
    cases.append(Case([event(0,1,2,0,0,1,1)],[trigger(0,0),trigger(4,0)],
        [[0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0],[0,12,0,77,8,0,0,0]],
        markers=[[0x177d,77,-10,20],[0x177d,77,30,-40]],steps=['t']))
    cases.append(Case([event(0,0,0,0,0,1,1)],[trigger(1,6,1,0)],actions,
        dwords=[(0x7936a8,1,0x80000000),(0x7936ac,1,1)],steps=['t']))
    cases.append(Case([event(1,0,0,0,0,0,1)],[],actions,steps=['t'],
        mutations=[(0x45f670,1,0,0,3,123)]))
    for counter in (0x7fffffff,0x80000000,0xffffffff,15,16):
        cases.append(Case([event(0,0,0,0,0,0,1)],[],[[0,5,0,1,0,0,0,0]],counter=counter,steps=['f']))
    # Load and binding: two markers share an id, one marker has another type.
    cases.append(Case(
        events=[event(0, 0, 0, 0, 0, 1, 1), event(0, 1, 1, 0, 0, 2, 1)],
        triggers=[trigger(1, 10, 0, 7), trigger(1, 10, 0, 9), trigger(1, 3, 1)],
        actions=[[0, 0xc, 0, 7, 0, 0, 0, 0], [0, 5, 0, 0, 0, 0, 0, 0]],
        markers=[[0x177d, 7, -5, 9], [0x177d, 7, 3, 2], [0x177d, 9, 4, 4],
                 [6014, 7, 100, 100], [0x177d, 9, -1, -2]],
        steps=['t']))
    # Unmatched trigger and action clear their kind words.
    cases.append(Case(
        events=[event(0, 0, 0, 0, 0, 1, 1)],
        triggers=[trigger(1, 10, 0, 42, 42, 42)],
        actions=[[0, 0xd, 0, 42, 42, 42, 42, 42]],
        markers=[[0x177d, 5, 1, 1]],
        steps=['t', 't']))
    # Every trigger kind with live state. Unit records: index 2 live 7 start 5,
    # index 4 live -3 start -2.
    kinds = [trigger(1, 1, 1, 3), trigger(1, 2, 1, 4), trigger(1, 3, 2), trigger(1, 3, 5),
             trigger(1, 4, 2), trigger(1, 4, 4), trigger(1, 5, 2), trigger(1, 5, 4),
             trigger(1, 6, 2, 2), trigger(1, 6, 2, 3), trigger(1, 7, 2, 0, 5), trigger(1, 9, 2),
             trigger(1, 9, 5), trigger(1, 10, 2), trigger(1, 11, 2, 3), trigger(1, 12, 2, 5),
             trigger(1, 12, 2, 6), trigger(2, 0, 1), trigger(3, 0, 6), trigger(4, 0),
             trigger(5, 0), trigger(0, 0), trigger(6, 0), trigger(1, 8, 0)]
    cases.append(Case(
        events=[event(0, 0, 0, 0, 0, len(kinds), 0, 0), event(0, 0, 0, 0, 0, 0, 0, 1)],
        triggers=kinds, actions=[],
        markers=[[0x177d, 2, 5, 6]],
        dwords=[(0x7936a4, 2, 2), (0x7936a8, 2, 7), (0x7936ac, 2, 5),
                (0x7936a8, 4, 0xfffffffd), (0x7936ac, 4, 0xfffffffe)],
        sets=[[0, 1, 3], [1, 1, 4], [2, 2, 0, 5], [5, 6]],
        answers_330=[1], answers_560=[0],
        steps=['t']))
    # Kind 5 world flags and kind 4 loader flag.
    cases.append(Case(
        events=[event(0, 0, 0, 0, 0, 3, 0)],
        triggers=[trigger(5, 0), trigger(4, 0), trigger(2, 0, 0)],
        world=(0, 0), loader=0, steps=['t', 't']))
    cases.append(Case(
        events=[event(0, 0, 0, 0, 0, 2, 0)],
        triggers=[trigger(5, 0), trigger(4, 0)],
        world=(0, 2), loader=1, steps=['t']))
    cases.append(Case(
        events=[event(0, 0, 0, 0, 0, 1, 0)],
        triggers=[trigger(5, 0)],
        world=(7, 0), loader=0, steps=['t']))
    # Trigger list combination: and, or, xor, negation.
    cases.append(Case(
        events=[event(0, 0, 0, 0, 0, 4, 1)],
        triggers=[trigger(1, 5, 2), trigger(1, 5, 2), trigger(1, 4, 2), trigger(1, 5, 2)],
        actions=[[0, 1, 0, 3, 4, 0, 0, 0]],
        dwords=[(0x7936ac, 2, 1)],
        steps=['t']))
    cases.append(Case(
        events=[event(0, 0, 0, 0, 0, 2, 1)],
        triggers=[[2, 1, 5, 2, 0, 0, 0, 0], [4, 1, 5, 2, 0, 0, 0, 0]],
        actions=[[0, 1, 0, 3, 4, 0, 0, 0]],
        dwords=[(0x7936ac, 2, 1)], steps=['t']))
    # All action kinds, with a repeat timer so the run happens twice.
    actions = [[0, kind, 0, 9, 11, 0, 0, 0] for kind in range(16)]
    actions.append([0, 16, 0, 0, 0, 0, 0, 0])
    cases.append(Case(
        events=[event(1, 0, 0, 0x00020000, 0, 0, len(actions))],
        triggers=[trigger(5, 0)], actions=actions, steps=['t', 't', 't', 't', 't']))
    # Timer paths: delay reload, small delay, repeat expiry, fired gate.
    cases.append(Case(
        events=[event(0, 0, 0, 0, 0x00020100, 1, 1)],
        triggers=[trigger(5, 0)], actions=[[0, 1, 0, 3, 4, 0, 0, 0]],
        steps=['t', 't', 't', 't']))
    cases.append(Case(
        events=[event(1, 0, 0, 0x00020000, 0x00000100, 1, 1)],
        triggers=[trigger(5, 0)], actions=[[0, 1, 0, 3, 4, 0, 0, 0]],
        steps=['t', 't', 't', 't', 't', 't']))
    cases.append(Case(
        events=[event(1, 0, 0, 0x00020000, 0x00000100, 1, 1)],
        triggers=[trigger(1, 4, 9, 0)], actions=[[0, 1, 0, 3, 4, 0, 0, 0]],
        dwords=[(0x7936ac, 9, 1)], steps=['t', 't', 't']))
    # Scheduling: bits 1 and 2, cursor buckets, frame driver.
    cases.append(Case(
        events=[event(0, 0, 0, 0, 0, 1, 1), event(2, 0, 0, 0, 0, 1, 1), event(4, 0, 0, 0, 0, 1, 1)],
        triggers=[trigger(5, 0)], actions=[[0, 1, 0, 0, 0, 0, 0, 0]],
        steps=['t', 't', 't', 'b', 'c', 'b', 'c']))
    cases.append(Case(
        events=[event(0, 0, 0, 0, 0, 1, 1) for _ in range(9)],
        triggers=[trigger(5, 0)], actions=[[0, 1, 0, 0, 0, 0, 0, 0]],
        cursor=3, steps=['t'] + ['t'] * 7 + ['t'] * 8))
    cases.append(Case(
        events=[event(0, 0, 0, 0, 0, 1, 1)],
        triggers=[trigger(5, 0)], actions=[[0, 1, 0, 0, 0, 0, 0, 0]],
        steps=['f'] * 16 + ['f'] * 16 + ['f'] * 16))
    # Event words[0] with both scheduling bits set.
    cases.append(Case(
        events=[event(6, 0, 0, 0, 0, 1, 1)],
        triggers=[trigger(5, 0)], actions=[[0, 1, 0, 0, 0, 0, 0, 0]],
        steps=['t', 'b', 'c', 't']))
    # Fuzz: deterministic random fixtures over the whole surface.
    rng = random.Random(0xdf1461)
    for _ in range(40):
        n_triggers = rng.randrange(1, 5)
        n_actions = rng.randrange(0, 4)
        n_events = rng.randrange(1, 4)
        triggers = []
        for _ in range(n_triggers):
            kind = rng.choice([0, 1, 1, 1, 2, 3, 4, 5, 6])
            subject = rng.choice([0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13])
            if kind == 1 and subject not in (1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12):
                subject = rng.choice([1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12])
            triggers.append([rng.choice([0, 1, 2, 4, 6]), kind, subject,
                             rng.randrange(0, 8), rng.randrange(0, 8), rng.randrange(0, 8), 0, 0])
        actions = []
        for _ in range(n_actions):
            actions.append([0, rng.randrange(0, 17), 0, rng.randrange(0, 40), rng.randrange(0, 40), 0, 0, 0])
        events = []
        for _ in range(n_events):
            trigger_index = rng.randrange(0, n_triggers)
            count = rng.randrange(1, n_triggers - trigger_index + 1) if n_triggers else 0
            action_index = rng.randrange(0, max(1, n_actions)) if n_actions else 0
            action_count = rng.randrange(0, n_actions - action_index + 1) if n_actions else 0
            w0 = rng.choice([0, 1, 2, 4, 6])
            w3 = rng.choice([0, 0x00020000, 0x00000100, 0x00030000])
            w4 = rng.choice([0, 0x00000100, 0x00020100, 0x40])
            events.append(event(w0, trigger_index, action_index, w3, w4, count, action_count,
                                rng.choice([0, 1])))
        # Kind 2 reads events[words[3]]. The original has no range check, so
        # keep the fuzz fixtures inside the table to stay deterministic.
        for entry in triggers:
            if entry[1] == 2:
                entry[3] = rng.randrange(0, n_events)
        dwords = [(0x7936a4, index, rng.choice([0, 1, 2, 3]))
                  for index in range(8)]
        dwords += [(0x7936a8, index, rng.choice([0, 1, 5, 7, 0xfffffffd])) for index in range(8)]
        dwords += [(0x7936ac, index, rng.choice([0, 1, 5, 7, 0xfffffffe])) for index in range(8)]
        sets = [[0, rng.randrange(0, 4), rng.randrange(0, 8)] for _ in range(rng.randrange(0, 3))]
        sets += [[2, rng.randrange(0, 3), rng.randrange(0, 4), rng.randrange(0, 8)]
                 for _ in range(rng.randrange(0, 2))]
        sets += [[5, rng.randrange(0, 8)] for _ in range(rng.randrange(0, 2))]
        cases.append(Case(
            events=events, triggers=triggers, actions=actions,
            dwords=dwords, sets=sets,
            world=(rng.choice([0, 1]), rng.choice([0, 2])),
            loader=rng.choice([0, 1]), cursor=rng.randrange(0, 8),
            answers_330=[rng.choice([0, 1]) for _ in range(4)],
            answers_560=[rng.choice([0, 1]) for _ in range(4)],
            steps=[rng.choice(['t', 't', 'b', 'c', 'f']) for _ in range(rng.randrange(1, 12))]))
    return cases


def real_cases(game_folder, limit):
    sys.path.insert(0, str(Path(__file__).parent))
    from mission import Resolver
    from mission_report import parse

    resolver = Resolver(game_folder)
    names = sorted(set(entry.name for archive in resolver.archives for entry in archive.entries
                       if not entry.deleted and entry.name.lower().endswith('.bms')))
    cases = []
    used = []
    for name in names:
        if len(cases) >= limit:
            break
        data, _source = resolver.read(*resolver.find(name))
        parsed = parse(data)
        markers = [[object_row['type_id'], (object_row['words'][18] >> 16) & 0xff,
                    object_row['position_raw'][0], object_row['position_raw'][1]]
                   for object_row in parsed['object_groups'][2]]
        # 0x44a110 counts objects of groups 1/2/4 per ident into 0x7936a8 and
        # copies them into 0x7936ac (index 0 is zeroed before the copy). The
        # converter 0x449710 writes raw byte 0x48 into internal +0x72, so
        # [obj+0x70] >> 16 is raw byte 0x48; group 3 (markers) is not counted.
        counts = [0] * 64
        for group_index in (0, 1, 3):
            for object_row in parsed['object_groups'][group_index]:
                ident = object_row['byte_72']
                if ident < len(counts):
                    counts[ident] += 1
        counts[0] = 0
        dwords = []
        for index in range(64):
            dwords.append((0x7936a4, index, 0))  # 0x44a3e9 producer not emulated
            dwords.append((0x7936a8, index, counts[index]))
            dwords.append((0x7936ac, index, counts[index]))
        rng = random.Random(len(cases) * 7919 + 13)
        cases.append(Case(
            events=parsed['events'], triggers=parsed['triggers'], actions=parsed['actions'],
            markers=markers, dwords=dwords, sets=[[5, index] for index in range(4)],
            world=(1, 0), loader=0, cursor=len(cases) % 8,
            answers_330=[rng.choice([0, 1]) for _ in range(64)],
            answers_560=[rng.choice([0, 1]) for _ in range(64)],
            steps=['t'] * 16 + ['b', 'c'] + ['f'] * 24 + ['t'] * 8))
        used.append(name)
    return cases, used


def legacy_heap_case(data,case):
    proof={'mission':'C05M07.BMS','event_count':len(case.events),'index':74,'field_offset':20,
           'requested_event_bytes':len(case.events)*24,'alignment':16,'header_bytes':16,'layouts':[]}
    for occupied in (False,True):
        original=Original(data);original.prepare(case)
        existing=HEAP+0x710
        if occupied:
            original.put_u32(HEAP+12,existing)
            for at,value in ((0,0x100),(4,0),(8,0),(12,0)):original.put_u32(existing+at,value)
            original.put_u32(TYPEINFO+0xf000+12,1)
        original.load(case)
        events=original.u32(GLOBAL_EVENTS);triggers=original.u32(GLOBAL_TRIGGERS)
        address=events+74*24+20;value=original.uc.mem_read(address,1)[0]
        original.steps(case)
        proof['layouts'].append({'kind':'existing_zeroed_live_block' if occupied else 'fresh_heap',
            'events':hex(events),'triggers':hex(triggers),'event_end':hex(events+74*24),
            'read_address':hex(address),'byte':value,
            'owner':'existing live allocation payload+4' if occupied else 'trigger[0].kind low byte',
            'executed_reads':[call for call in original.log if call.startswith('external:')]})
    return proof


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--game-folder', type=Path,
                        default=Path('/Users/khanh/Desktop/workspace/Delta Force'))
    parser.add_argument('--real-limit', type=int, default=62)
    args = parser.parse_args()
    try:
        data = args.executable.read_bytes()
        if hashlib.sha256(data).hexdigest() != SOURCE_HASH:
            raise ValueError('Expected the documented Df.exe SHA-256')
        cases = synthetic_cases()
        real_names = []
        if args.game_folder.is_dir():
            real, real_names = real_cases(args.game_folder, args.real_limit)
            cases += real
        result = subprocess.run([str(args.probe.resolve())], check=True, capture_output=True,
                                text=True,
                                input=str(len(cases)) + '\n' +
                                '\n'.join(case.feed() for case in cases) + '\n')
        lines = result.stdout.splitlines()
        if len(lines) != len(cases):
            raise ValueError(f'Probe printed {len(lines)} lines for {len(cases)} cases')
        original = Original(data)
        mismatches = []
        for index, case in enumerate(cases):
            expected = run_case(original, case)
            if expected != lines[index]:
                mismatches.append({'case': index,
                                   'original': expected[:400],
                                   'probe': lines[index][:400]})
        heap_proof=None
        if 'C05M07.BMS' in real_names:
            heap_proof=legacy_heap_case(data,cases[len(cases)-len(real_names)+real_names.index('C05M07.BMS')])
        print(json.dumps({
            'source_sha256': SOURCE_HASH,
            'procedures': {'load': hex(FUNCTION_LOAD), 'reset': hex(FUNCTION_RESET),
                           'tick': hex(FUNCTION_TICK), 'bit1': hex(FUNCTION_BIT1),
                           'bit2': hex(FUNCTION_BIT2), 'frame_counter': f'{FRAME_COUNTER_BEGIN:#x}-{FRAME_COUNTER_END:#x}'},
            'unicorn_version': unicorn.__version__,
            'cases': len(cases), 'synthetic_cases': len(cases) - len(real_names),
            'legacy_external_fired_byte':heap_proof,
            'allocator_fixture':'Original497F60/498A00 with a defined fresh heap;16-byte headers and alignment',
            'real_missions': real_names,
            'mismatch_count': len(mismatches), 'mismatches': mismatches[:4],
            'scope': 'Load, index conversion, both binders, trigger evaluation, action routing, '
                     'timer state machine, cursor scheduling and signed frame counter. Host effects '
                     'are fixtures compared by call log; marker records are synthesized from the '
                     'fields the binder reads; counts cover64identifiers;7936A4 stays a fixture.',
        }, indent=2))
        return int(bool(mismatches))
    except (OSError, ValueError, pefile.PEFormatError, unicorn.UcError,
            subprocess.CalledProcessError) as error:
        print(f'check_events_original: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
