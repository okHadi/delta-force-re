#!/usr/bin/env python3
"""Compare 0x404340 and its real 0x43F560 timer body with the native frame.

Other direct callees use declared return/mutation fixtures on both sides.
No fixture proves the body of the callee. The checker verifies the frame's
state, return value, arguments, and call order after each fixture.
"""
import argparse
import hashlib
import itertools
import json
import random
import struct
import subprocess
from pathlib import Path

import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX

SHA256 = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
FIELDS = [0x67F528, 0x67F520, 0x6E4714, 0x6E4718, 0x67DBF4, 0x6E471C,
          0x6E4720, 0x6E4724, 0x67DBE8, 0x4CED10, 0x67DBF0, 0x67DBCC,
          0x67DBD0, 0x67DBD4, 0x67DBD8, 0x67DBDC, 0x491014, 0x67F53C,
          0x503424, 0x67DBBC]
ARGC = {0x461350: 0, 0x4562A0: 1, 0x43F560: 0, 0x456E30: 1,
        0x490880: 0, 0x479750: 1, 0x479ED0: 0, 0x43F0B0: 0,
        0x479450: 1, 0x46F5C0: 0, 0x487EA0: 1, 0x443C50: 0,
        0x42AB70: 0, 0x4702A0: 1, 0x48FBD0: 0, 0x489220: 0,
        0x467260: 0, 0x404E30: 0, 0x461340: 1}
SENTINEL = 0x1000000
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('game', type=Path)
p.add_argument('probe', type=Path)
p.add_argument('--report', type=Path)
a = p.parse_args()
raw = (a.game / 'Df.exe').read_bytes()
if hashlib.sha256(raw).hexdigest() != SHA256:
    raise SystemExit('Unsupported original executable hash')
pe = pefile.PE(data=raw)
u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
u.mem_map(0x400000, 0xA00000)
u.mem_map(0x1000000, 0x200000)
for section in pe.sections:
    if section.PointerToRawData and not section.Characteristics & 0x80:
        u.mem_write(0x400000 + section.VirtualAddress,
                    raw[section.PointerToRawData:section.PointerToRawData + section.SizeOfRawData])


def put(address, value):
    u.mem_write(address, struct.pack('<I', value & 0xFFFFFFFF))


def get(address):
    return struct.unpack('<I', u.mem_read(address, 4))[0]


context = {}


def hook(uc, address, size, data):
    if address == SENTINEL:
        context['returned'] = True
        uc.emu_stop()
        return
    if address not in ARGC:
        return
    esp = uc.reg_read(UC_X86_REG_ESP)
    args = [get(esp + 4 + 4 * index) for index in range(ARGC[address])]
    context['calls'].append([address, *args])
    if address == 0x43F560:
        return  # Execute the original timer instructions, including all RETs.
    for stage, field, value in context['mutations']:
        if stage == address:
            put(FIELDS[field], value)
    uc.reg_write(UC_X86_REG_EAX, context['stop'] if address == 0x461350 else 0)
    uc.reg_write(UC_X86_REG_EIP, get(esp))
    uc.reg_write(UC_X86_REG_ESP, esp + 4)


u.hook_add(unicorn.UC_HOOK_CODE, hook)


def original(values, stop, mutations):
    for address, value in zip(FIELDS, values):
        put(address, value)
    context.update(calls=[], mutations=mutations, stop=stop, returned=False)
    put(0x11FF000, SENTINEL)
    u.reg_write(UC_X86_REG_ESP, 0x11FF000)
    u.reg_write(UC_X86_REG_EAX, 0xCCCCCCCC)
    u.emu_start(0x404340, SENTINEL + 1, count=10000)
    if not context['returned']:
        raise AssertionError('Original frame did not return')
    result = u.reg_read(UC_X86_REG_EAX)
    calls = context['calls']
    if calls[0][0] != 0x461350:
        raise AssertionError('Missing early-stop query')
    reason = 0
    if stop:
        reason = 1
    elif result:
        token = calls[-1][1]
        reason = {0x4CECE0: 2, 0: 3, 0x4EB834: 4}[token]
    fields = ','.join(map(str, [*[get(at) for at in FIELDS], result, reason]))
    return fields + '|' + ';'.join(','.join(map(str, call)) for call in calls)


cases = []
base = [1, 0, 0, 0, 100, 200, 300, 400, 10, 0, 1234, 90, 80, 1000, 70, 60, 1200, 0, 0, 0]
for host, multi, pause, demo, menu, game, shutdown, frame in itertools.product(
        [0, 1], [0, 1], [0, 1], [0, 1], [0, 1, 4], [0, 1], [0, 1], [0, 60, 61]):
    v = base.copy()
    for index, value in zip([0, 1, 2, 3, 19, 17, 18, 9],
                            [host, multi, pause, demo, menu, game, shutdown, frame]):
        v[index] = value
    cases.append((v, 0, []))
for stop in [1, 2, 0xFFFFFFFF]:
    cases.append((base.copy(), stop, []))
# Callbacks may alter all later branch inputs, timers, and call arguments.
for stage in [0x461350, 0x4562A0, 0x490880, 0x479450, 0x487EA0,
              0x42AB70, 0x456E30, 0x4702A0, 0x48FBD0, 0x404E30]:
    for field in range(len(FIELDS)):
        for value in [0, 1, 4, 61, 0xFFFFFFFF]:
            v = base.copy()
            v[9] = 61
            v[1] = 1
            cases.append((v, 0, [(stage, field, value)]))
rng = random.Random(0x404340)
for _ in range(800):
    v = [rng.getrandbits(32) for _ in FIELDS]
    for index in [0, 1, 2, 3, 17, 18]:
        v[index] = rng.randrange(2)
    v[19] = rng.choice([0, 1, 4, 0xFFFFFFFF])
    v[9] = rng.choice([0, 60, 61, 62, 124, 0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF])
    v[8] = rng.choice([0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF])
    changes = [(rng.choice(list(ARGC.keys() - {0x43F560})), rng.randrange(len(FIELDS)),
                rng.getrandbits(32)) for _ in range(rng.randrange(4))]
    cases.append((v, 0, changes))
lines = []
for values, stop, mutations in cases:
    lines.append(','.join(map(str, values)) + '|' + str(stop) + '|' +
                 (';'.join(','.join(map(str, item)) for item in mutations) or '-'))
run = subprocess.run([str(a.probe.resolve()), 'frame'], input='\n'.join(lines) + '\n',
                     text=True, capture_output=True, check=True)
actual = run.stdout.splitlines()
if len(actual) != len(cases):
    raise AssertionError(f'Probe returned {len(actual)} rows for {len(cases)} cases')
for index, (case, rebuilt) in enumerate(zip(cases, actual)):
    expected = original(*case)
    if rebuilt != expected:
        raise AssertionError(f'Case {index}: {lines[index]}\noriginal={expected}\nrebuilt={rebuilt}')
report = {'source_sha256': SHA256, 'frame': '0x404340', 'real_timer': '0x43F560',
          'cases': len(cases), 'compared': ['20 globals', 'return status', 'exit reason',
                                         'ordered calls and arguments'],
          'foreign_bodies': 'declared synchronous return and state-mutation fixtures',
          'status': 'passed'}
if a.report:
    a.report.parent.mkdir(parents=True, exist_ok=True)
    a.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
