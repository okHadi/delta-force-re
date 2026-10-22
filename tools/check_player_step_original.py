#!/usr/bin/env python3
"""Compare original 0x482B00 recoil, eye height and waypoint instructions.

The required weapon prefix (482B11..482B78, including its branch targets)
uses a declared state-change fixture on both sides. The other instructions
execute from the supplied original executable.
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
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP

SHA = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
GLOBALS = [0xC23820, 0xC239E8, 0xC239EC, 0xC239F0, 0xC23A18, 0xC23828,
           0x67F53C, 0xC23B7C, 0xC23B80]
SENTINEL = 0x1000000
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('game', type=Path)
p.add_argument('probe', type=Path)
p.add_argument('--report', type=Path)
a = p.parse_args()
raw = (a.game / 'Df.exe').read_bytes()
if hashlib.sha256(raw).hexdigest() != SHA:
    raise SystemExit('Unsupported original executable hash')
pe = pefile.PE(data=raw)
u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
for address, size in [(0x400000, 0xA00000), (0x1000000, 0x200000), (0x2000000, 0x3000)]:
    u.mem_map(address, size)
for section in pe.sections:
    if section.PointerToRawData and not section.Characteristics & 0x80:
        u.mem_write(0x400000 + section.VirtualAddress,
                    raw[section.PointerToRawData:section.PointerToRawData + section.SizeOfRawData])


def put(address, value):
    u.mem_write(address, struct.pack('<I', value & 0xFFFFFFFF))


def get(address):
    return struct.unpack('<I', u.mem_read(address, 4))[0]


context = {}


def sync(values):
    for address, value in zip(GLOBALS, values):
        put(address, value)
    for local in [0x2000000, 0x2001000]:
        u.mem_write(local + 0x6E, bytes([values[9] & 255]))
        put(local + 0x24, values[10])
        put(local + 0x28, values[11])
    put(0x2002000 + 0x24, values[12])
    put(0x2002000 + 0x28, values[13])
    put(0x2002000 + 0x7C, values[14])
    put(0xC23B84, 0x2002000)
    put(0xC23B88, 0x2002000)


def hook(uc, address, size, data):
    if address == SENTINEL:
        context['returned'] = True
        uc.emu_stop()
    elif address == 0x482B11:
        context['prefix_calls'] += 1
        for field, value in context['mutations']:
            context['values'][field] = value
        sync(context['values'])
        uc.reg_write(UC_X86_REG_EIP, 0x482B79)


u.hook_add(unicorn.UC_HOOK_CODE, hook)


def original(values, mutations):
    context.update(values=values.copy(), mutations=mutations, returned=False, prefix_calls=0)
    sync(values)
    put(0x11FF000, SENTINEL)
    u.reg_write(UC_X86_REG_ESP, 0x11FF000)
    u.emu_start(0x482B00, SENTINEL + 1, count=10000)
    if not context['returned']:
        raise AssertionError('Original player step did not return')
    output = [get(address) for address in GLOBALS] + context['values'][9:]
    return ','.join(map(str, output + [context['prefix_calls']]))


base = [0x2000000, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0x10000]
cases = []
edge = [0, 1, 127, 128, 129, 0x7FFFFFFF, 0x80000000, 0xFFFFFF7F,
        0xFFFFFF80, 0xFFFFFF81, 0xFFFFFFFF]
for recoil, eye, prone, flags in itertools.product(edge, [0, 0x4CCC, 0x6666, 0xB333, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF], [0, 1], [0, 2]):
    values = base.copy()
    values[1:4] = [recoil] * 3
    values[4], values[5], values[9] = eye, prone, flags
    cases.append((values, []))
for dx, dy, radius, index in itertools.product(edge, edge, [0, 1, 128, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF], [0, 1]):
    values = base.copy()
    values[10], values[11], values[14], values[8] = dx, dy, radius, index
    cases.append((values, []))
for field in range(15):
    for value in ([0x2001000] if field == 0 else [0, 1] if field == 8 else [0, 1, 2, 255] if field == 9 else edge):
        cases.append((base.copy(), [(field, value)]))
rng = random.Random(0x482B00)
for _ in range(1000):
    values = [rng.getrandbits(32) for _ in base]
    values[0] = rng.choice([0, 0x2000000, 0x2001000])
    values[6] = rng.choice([0, 0, 1])
    values[7] = rng.choice([0, 1, 2, 0x7FFFFFFF, 0xFFFFFFFF])
    values[8] = rng.randrange(2)
    values[9] = rng.randrange(256)
    cases.append((values, []))
lines = [','.join(map(str, values)) + '|' + (';'.join(','.join(map(str, m)) for m in mutations) or '-')
         for values, mutations in cases]
run = subprocess.run([str(a.probe.resolve())], input='\n'.join(lines) + '\n', text=True,
                     capture_output=True, check=True)
rows = run.stdout.splitlines()
if len(rows) != len(cases):
    raise AssertionError(f'Probe returned {len(rows)} rows for {len(cases)} cases')
for index, (case, rebuilt) in enumerate(zip(cases, rows)):
    expected = original(*case)
    if expected != rebuilt:
        raise AssertionError(f'Case {index}: {lines[index]}\noriginal={expected}\nrebuilt={rebuilt}')
report = {'source_sha256': SHA, 'function': '0x482B00', 'cases': len(cases),
          'compared': ['recoil', 'eye height', 'waypoint index', 'prefix mutations', 'null actor gate'],
          'weapon_prefix': 'required synchronous fixture at 0x482B11; resumes original at 0x482B79',
          'status': 'passed'}
if a.report:
    a.report.parent.mkdir(parents=True, exist_ok=True)
    a.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
