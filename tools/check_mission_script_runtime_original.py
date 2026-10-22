#!/usr/bin/env python3
"""Compare script playback and queue bodies with local original instructions."""
import argparse
import json
import random
import struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX
from check_weapons_original import OriginalRig, SOURCE_SHA256
from check_projectile_hits_original import match

RESOURCE, NAMES, CACHE = 0x1100000, 0x1110000, 0x1120000
CALLBACK, SHOW, TEXT = 0x120F000, 0x120F010, 0x120F020
COUNT, HANDLES, VOICE_NAMES, QUEUE = 0x8E3D50, 0x8E3550, 0x8E3950, 0x8E3D74
TEXT_NAMES = ['radio_123', 'root', 'number_+42', 'radio_-7', 'radio_1x', '', '_', 'long_last_42']


def run(args):
    rig = OriginalRig(args.game)
    rng = random.Random(0x456110)
    counts = {}

    def read(at): return rig.peek(at) & 0xFFFFFFFF
    def call_args(n):
        return struct.unpack('<%dI' % n, rig.u.mem_read(rig.u.reg_read(UC_X86_REG_ESP) + 4, n * 4))
    def string(at, value): rig.u.mem_write(at, value.encode('ascii') + b'\0')
    def log(site, *values): trace.append(site + ',' + ','.join(map(str, values)) + ';')

    for mode in ['step', 'announce', 'remove', 'cleanup']:
        rows, expected = [], []
        for case in range(args.cases):
            rig.reset()
            rig.u.mem_write(HANDLES, bytes(0x800))
            rig.u.mem_write(QUEUE, bytes(0x100))
            rig.poke(COUNT, 0)
            trace = []
            if mode == 'step':
                index, sample, name = rng.randrange(4), rng.choice([0, 0, 17]), case % 8
                dictionary, found, accept, mutation = rng.randrange(2), rng.randrange(2), rng.randrange(2), rng.randrange(8)
                mode_byte, text_value = rng.randrange(256), rng.choice([0, 77, 0xFFFFFFFF])
                position, parameters = [rng.getrandbits(32) for _ in range(3)], [rng.getrandbits(32) for _ in range(3)]
                rows.append([index, sample, name, dictionary, found, accept, mutation, mode_byte, text_value, *position, *parameters])
                base = RESOURCE + 0x34 + index * 0x44
                string(base + 4, 'voice.wav'); string(base + 0x1C, TEXT_NAMES[name])
                rig.u.mem_write(base + 0x34, bytes([mode_byte]))
                for i, value in enumerate(position): rig.poke(base + 0x38 + i * 4, value)
                rig.poke(0x8E3F3C, dictionary); rig.poke(0x8E3D68, CALLBACK); rig.poke(0x8E3D6C, SHOW)
                rig.poke(TEXT, text_value)
                def load(): log('L', call_args(1)[0]); rig.finish(sample)
                def missing(): log('M', call_args(3)[2]); rig.finish()
                def parse(): log('I', call_args(1)[0])  # Execute the actual 49DFF0 body.
                def lookup(): log('T', *call_args(2)); rig.finish(TEXT if found else 0)
                def callback():
                    (at,) = call_args(1)
                    log('C', read(at), read(at + 4), rig.u.mem_read(at + 8, 1)[0], *[read(at + 12 + i * 4) for i in range(3)])
                    if mutation & 1: rig.poke(at + 4, read(at + 4) + 3)
                    if mutation & 2: rig.poke(at, read(at) + 1)
                    for i, value in enumerate(parameters): rig.poke(at + 24 + i * 4, value)
                    rig.finish(accept)
                def play():
                    log('P', *call_args(4))
                    if mutation & 4: rig.poke(COUNT, 2)
                    rig.finish(91)
                def show(): log('S', call_args(1)[0], read(COUNT)); rig.finish()
                rig.stubs.update({0x49D400: load, 0x493A80: missing, 0x49DFF0: parse, 0x49A076: lookup,
                                  CALLBACK: callback, 0x494DD0: play, SHOW: show})
                rig.call(0x456110, RESOURCE, index)
                values = [read(COUNT)]
                for i in range(3): values += [read(HANDLES + i * 4), read(VOICE_NAMES + i * 4)]
            elif mode == 'announce':
                host, index, name = rng.randrange(2), rng.getrandbits(32), case % 8
                rows.append([host, index, name]); rig.poke(0x67F528, host); string(NAMES, TEXT_NAMES[name])
                def message():
                    message_id, at, length = call_args(3)
                    trace.append(','.join(map(str, [message_id, *rig.u.mem_read(at, length)])) + ';'); rig.finish()
                rig.stubs[0x442630] = message
                rig.call(0x447C60, NAMES, index)
                values = [0]
            elif mode == 'remove':
                count, wanted = case % 17, rng.randrange(17)
                row = [count, wanted]; rig.poke(0x8E3D70, count)
                for i in range(17): string(NAMES + i * 32, 'name' + str(i % 5))
                for i in range(count):
                    name = rng.randrange(16); fields = [name, *[rng.getrandbits(32) for _ in range(3)]]; row += fields
                    for j, value in enumerate([NAMES + name * 32 - 4, *fields[1:]]): rig.poke(QUEUE + i * 16 + j * 4, value)
                rows.append(row)
                rig.call(0x456000, NAMES + wanted * 32)
                result = rig.u.reg_read(UC_X86_REG_EAX)
                values = [result, read(0x8E3D70)]
                for i in range(values[1]): values += [read(QUEUE + i * 16 + j * 4) for j in range(4)]
            else:
                count, mutation = case % 9, rng.randrange(4); row = [count, mutation]; rig.poke(COUNT, count)
                for i in range(16): string(NAMES + i * 32, 'name' + str(i % 4))
                for i in range(count):
                    handle, name = rng.randrange(10), rng.randrange(16); row += [handle, name]
                    rig.poke(HANDLES + i * 4, handle); rig.poke(VOICE_NAMES + i * 4, NAMES + name * 32)
                for i in range(4):
                    loaded, allocation = rng.randrange(2), rng.choice([0, i + 20]); row += [loaded, allocation]
                    rig.u.mem_write(CACHE + i * 64, bytes(64)); rig.u.mem_write(CACHE + i * 64 + 0x23, bytes([loaded])); rig.poke(CACHE + i * 64 + 0x24, allocation)
                rows.append(row); changed = False
                def playing():
                    nonlocal changed
                    (handle,) = call_args(1); log('Q', handle)
                    if mutation & 1 and not changed: rig.poke(VOICE_NAMES, NAMES + 32); changed = True
                    rig.finish(handle & 1)
                def cache():
                    (at,) = call_args(1); log('F', at); rig.finish(CACHE + (((at - NAMES) // 32) % 4) * 64)
                def free():
                    log('R', call_args(1)[0])
                    if mutation & 2:
                        for i in range(4):
                            if not rig.u.mem_read(CACHE + i * 64 + 0x23, 1)[0]: rig.poke(CACHE + i * 64 + 0x24, 99)
                    rig.finish()
                rig.stubs.update({0x494FF0: playing, 0x49CFD0: cache, 0x4980D0: free})
                rig.call(0x455B40)
                values = [read(COUNT)]
                for i in range(8): values += [read(HANDLES + i * 4), read(VOICE_NAMES + i * 4)]
                for i in range(4): values += [rig.u.mem_read(CACHE + i * 64 + 0x23, 1)[0], read(CACHE + i * 64 + 0x24)]
            expected.append(' '.join(map(str, values)) + ' ' + ''.join(trace))
        match(args.probe, mode, rows, expected); counts[mode] = len(rows)
    report = {'source_sha256': SOURCE_SHA256, 'cases': counts,
              'real_bodies': ['456110', '447C60', '456000', '455FA0', '455B40', '49AD30', '49DFF0'],
              'foreign_fixtures': ['audio load/diagnostic', 'text dictionary', 'step callback', 'muted playback/show', 'voice state/cache/free', '442630 message'],
              'comparison': 'current callback arguments, voice publication, queue ordering, cleanup writes, packets, and ordered mutable effects'}
    if args.report: args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(); parser.add_argument('game', type=Path); parser.add_argument('probe', type=Path)
    parser.add_argument('--cases', type=int, default=500); parser.add_argument('--report', type=Path); run(parser.parse_args())
