#!/usr/bin/env python3
"""Check the rebuilt per-tick actor stage against original Df.exe instructions.

Sections:
  tick      original 0x487EA0        one tick over the five actor tables
  rebuild   original 0x487E10        selected-actor target-list rebuild (real 0x487C50)
  acquire   original 0x487C50        one actor's nearby-actor list, with list prefill

Every probe call is length-checked, every emulation must reach a known return
address, and every hook records its argument. Silently skipped cases fail.
"""
import argparse
import hashlib
import json
import random
import struct
import subprocess
from pathlib import Path

import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW

SOURCE_SHA256 = "abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff"
RET_SENTINEL = 0x412345
DEF_TABLE = 0x3000000
DEF_PTR = 0xC23810
DEF_COUNT = 0xC23814
COUNTER = 0xC92A98
LIST_COUNT = 0xC92A90
LIST_ARRAY = 0xC8FF50
PAUSED = 0x6E4714
DEMO = 0x6E4718
LOCAL = 0xC23820
UPDATE_KEY = 0x2004000
UPDATE_KEY_ALT = 0xDEAD0000
SECONDARY_KEY = 0x2004100
OBJECT_BASE = 0x2005000
OBJECT_STRIDE = 0x100
ACTOR_STRIDE = 0x114
TABLES = {0: (0x7DDE80, 0x8CFA1C, 1024), 1: (0x867E80, 0x8CF904, 1024),
          2: (0x8ACE80, 0x8CF900, 512), 3: (0x7CCA80, 0x8CFA18, 256)}
EXTRA = (0x822E80, 0x8CFA14, 1024)
RETS_TICK = (0x48804B, 0x488096, 0x4880B4)
RET_REBUILD = 0x487E73
RET_ACQUIRE = 0x487CE1

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("game", type=Path)
p.add_argument("probe", type=Path)
p.add_argument("--report", type=Path)
p.add_argument("--scale", type=int, default=1, help="multiply generated case counts")
p.add_argument("--mutating-callbacks", action="store_true")
args = p.parse_args()

raw = (args.game / "Df.exe").read_bytes()
sha = hashlib.sha256(raw).hexdigest()
if sha != SOURCE_SHA256:
    raise SystemExit("Unsupported original executable hash")
pe = pefile.PE(data=raw)
u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
for at, size in [(0x400000, 0xA00000), (0x1000000, 0x200000), (0x2000000, 0x100000),
                 (0x3000000, 0x200000), (0x4000000, 0x1000), (0xDEAD0000, 0x1000)]:
    u.mem_map(at, size)
u.mem_write(0xDEAD0000, b"\xC3")  # alternate update callback target returns
for section in pe.sections:
    if section.PointerToRawData and not section.Characteristics & 0x80:
        u.mem_write(0x400000 + section.VirtualAddress,
                    raw[section.PointerToRawData:section.PointerToRawData + section.SizeOfRawData])
PROBE = str(args.probe.resolve())
case_context = [0]


def put(at, value):
    u.mem_write(at, struct.pack("<I", value & 0xFFFFFFFF))


def put8(at, value):
    u.mem_write(at, bytes([value & 0xFF]))


def word(at):
    return struct.unpack("<I", u.mem_read(at, 4))[0]


def probe(mode, rows):
    text = "".join(" ".join(str(field) for field in row) + "\n" for row in rows)
    done = subprocess.run([PROBE, mode], input=text.encode(), capture_output=True)
    if done.returncode != 0:
        raise SystemExit("probe failed: " + done.stderr.decode(errors="replace"))
    lines = [line for line in done.stdout.decode().splitlines() if line]
    if len(lines) != len(rows):
        raise SystemExit(f"probe {mode}: {len(lines)} result lines for {len(rows)} cases")
    return lines


def emu_run_rets(begin, rets, count=500000):
    fired = []

    def hook(machine, at, size, user):
        fired.append(at)
        u.emu_stop()

    handles = [u.hook_add(unicorn.UC_HOOK_CODE, hook, begin=r, end=r) for r in rets]
    try:
        u.emu_start(begin, 0x1000, count=count)
    except unicorn.UcError as error:
        eip = u.reg_read(UC_X86_REG_EIP)
        raise SystemExit(f"case {case_context[0]}: emulation from 0x{begin:x} failed at "
                         f"EIP=0x{eip:x}: {error}")
    finally:
        for handle in handles:
            u.hook_del(handle)
    if len(fired) != 1:
        eip = u.reg_read(UC_X86_REG_EIP)
        raise SystemExit(f"case {case_context[0]}: emulation from 0x{begin:x} hit "
                         f"{len(fired)} return addresses (EIP=0x{eip:x})")
    return fired[0]


def normalize_pointer(pointer):
    for group, (base, _, slots) in TABLES.items():
        if base <= pointer < base + slots * ACTOR_STRIDE:
            return [group, (pointer - base) // ACTOR_STRIDE]
    base, _, slots = EXTRA
    if base <= pointer < base + slots * ACTOR_STRIDE:
        return [4, (pointer - base) // ACTOR_STRIDE]
    raise SystemExit(f"record pointer 0x{pointer:x} is outside the actor tables")


def normalize_list_start(pointer):
    return 0 if pointer == 0 else (pointer - LIST_ARRAY) // 4


def setup_definitions(defs):
    put(DEF_PTR, DEF_TABLE)
    put(DEF_COUNT, len(defs))
    u.mem_write(DEF_TABLE, bytes(0x13C * len(defs)))
    for index, (ident, field_28, field_9c) in enumerate(defs):
        base = DEF_TABLE + index * 0x13C
        put(base + 0x20, ident)
        put(base + 0x28, field_28)
        put(base + 0x9C, field_9c)


def setup_object(handle, radius, height):
    base = OBJECT_BASE + handle * OBJECT_STRIDE
    put(base + 0x64, radius)
    put(base + 0x60, height)


def object_address(handle):
    return OBJECT_BASE + handle * OBJECT_STRIDE if handle else 0


def write_record(group, index, definition, *, byte6c=0, handle=0, position=(0, 0, 0),
                 f110=0, f10c=0, f18=0, f78=0, fc=0):
    base, _, _ = TABLES[group]
    at = base + index * ACTOR_STRIDE
    u.mem_write(at, bytes(ACTOR_STRIDE))
    put(at + 0x00, definition)
    put(at + 0x04, DEF_TABLE + definition * 0x13C)
    put(at + 0x10, object_address(handle))
    put(at + 0x18, f18)
    for axis in range(3):
        put(at + 0x24 + axis * 4, position[axis] & 0xFFFFFFFF)
    put8(at + 0x6C, byte6c)
    put(at + 0x78, f78)
    put(at + 0xFC, fc)
    put(at + 0x10C, f10c)
    put(at + 0x110, f110)
    return at


def parse_list(token):
    if token == "-":
        return []
    return [[int(g), int(i)] for g, i in (pair.split(",") for pair in token.split(";"))]


def parse_indices(token):
    return [] if token == "-" else [int(v) for v in token.split(",")]


def format_list(items):
    if not items:
        return "-"
    return ";".join(f"{g},{i}" for g, i in items)


def format_indices(items):
    if not items:
        return "-"
    return ",".join(str(i) for i in items)


def format_numbers(items):
    if not items:
        return "-"
    return ";".join(str(i) for i in items)


report = {"source_sha256": sha, "probe": PROBE, "checks": {}}
total = 0
random.seed(20261017)

DEFS_SCAN = [(0, 0, 0), (3012, 1, 0), (3013, 3, 0), (3014, 6, 0), (3015, 2, 0), (3016, 0, 0)]
BOUNDS = {5: (100, 200), 6: (0x100000, 0x80000)}
TICK_DEFS = [(0, 0, 0), (3012, 0, 5), (3013, 0, 12)]


def bounds_token(table):
    return ";".join(f"{h}:{r}:{g}" for h, (r, g) in sorted(table.items()))


def defs_token(defs):
    return ";".join(f"{i}:{f28}:{f9c}" for i, f28, f9c in defs)


def record_token(record):
    return ":".join(str(v) for v in record)


# ---------------------------------------------------------------------------
# acquire: 0x487C50, list prefill/cap/bounds
# ---------------------------------------------------------------------------
def acquire_row(defs, bounds, prefill, actor, g1, g0):
    row = [defs_token(defs), bounds_token(bounds), prefill, record_token(actor),
           str(len(g1))] + [record_token(r) for r in g1] + [str(len(g0))] + \
          [record_token(r) for r in g0]
    return row


acquire_cases = []
for index in range(200 * args.scale):
    definition = random.choice([1, 2, 3, 4, 5])
    actor = (definition, 0, 5, random.choice([0, 0x10000, -0x10000, 0x140000, 0x7FFFFFF]), 0, 0)
    g1 = []
    for _ in range(random.randrange(0, 5)):
        g1.append((random.randrange(6), random.choice([0, 2]), random.choice([0, 5, 6]),
                   random.choice([0, 100, 0x140000, 0x140064, 0x140065, -0x140064]), 0, 0))
    g0 = []
    for _ in range(random.randrange(0, 5)):
        g0.append((random.randrange(6), random.choice([0, 2]), random.choice([0, 5, 6]),
                   random.choice([0, 100, 0x140000, 0x140064, 0x140065]), 0, 0))
    prefill = random.choice([0, 0, 0, 0, 1999, 2000])
    acquire_cases.append((DEFS_SCAN, BOUNDS, prefill, actor, g1, g0))
# Directed boundaries: exact limit, limit+1, height boundary, self, flag, handle, def0.
for definition, range_value in ((1, 0x140000), (2, 0xA0000), (3, 0xA0000)):
    radius, height = BOUNDS[5]
    for dx, dy, dz in ((radius + range_value, 0, 0),
                       (radius + range_value + 1, 0, 0),
                       (radius + range_value - 1, 0, 0),
                       (0, radius + range_value + 1, 0),
                       (0, 0, height + range_value),
                       (0, 0, height + range_value + 1)):
        actor = (definition, 0, 5, 0, 0, 0)
        g1 = [(1, 0, 5, dx, dy, dz)]
        acquire_cases.append((DEFS_SCAN, BOUNDS, 0, actor, g1, []))
acquire_cases.append((DEFS_SCAN, BOUNDS, 0, (1, 0, 0, 0, 0, 0), [(1, 0, 5, 0, 0, 0)], []))
acquire_cases.append((DEFS_SCAN, BOUNDS, 0, (1, 0, 5, 0, 0, 0), [(0, 0, 5, 0, 0, 0)], []))
acquire_cases.append((DEFS_SCAN, BOUNDS, 0, (1, 0, 5, 0, 0, 0), [(1, 2, 5, 0, 0, 0)], []))
acquire_cases.append((DEFS_SCAN, BOUNDS, 0, (1, 0, 5, 0, 0, 0), [], [(4, 0, 5, 0, 0, 0)]))
acquire_cases.append((DEFS_SCAN, BOUNDS, 0, (5, 0, 5, 0, 0, 0), [(1, 0, 5, 0, 0, 0)], []))

native = probe("acquire", [acquire_row(*case) for case in acquire_cases])
mismatch = 0
samples = []
for index, (case, line) in enumerate(zip(acquire_cases, native)):
    defs, bounds, prefill, actor, g1, g0 = case
    setup_definitions(defs)
    for handle, (radius, height) in bounds.items():
        setup_object(handle, radius, height)
    put(LIST_COUNT, prefill)
    u.mem_write(LIST_ARRAY, bytes(4 * 2001))
    write_record(3, 0, actor[0], byte6c=actor[1], handle=actor[2],
                 position=(actor[3], actor[4], actor[5]))
    for slot, record in enumerate(g1):
        write_record(1, slot, record[0], byte6c=record[1], handle=record[2],
                     position=(record[3], record[4], record[5]))
    for slot, record in enumerate(g0):
        write_record(0, slot, record[0], byte6c=record[1], handle=record[2],
                     position=(record[3], record[4], record[5]))
    put(TABLES[1][1], len(g1))
    put(TABLES[0][1], len(g0))
    actor_pointer = TABLES[3][0]
    sp = 0x300E000
    u.mem_write(sp, struct.pack("<2I", RET_SENTINEL, actor_pointer))
    u.reg_write(UC_X86_REG_ESP, sp)
    u.reg_write(UC_X86_REG_FPCW, 0x037F)
    case_context[0] = index
    emu_run_rets(0x487C50, [RET_ACQUIRE])
    count = word(LIST_COUNT)
    appended = []
    for slot in range(prefill, count):
        appended.append(normalize_pointer(word(LIST_ARRAY + slot * 4)))
    fields = dict(part.split("=", 1) for part in line.split())
    want = (count, word(actor_pointer + 0x108),
            normalize_list_start(word(actor_pointer + 0x104)), format_list(appended))
    got = (int(fields["count"]), int(fields["f108"]), int(fields["f104"]), fields["appended"])
    if want != got:
        mismatch += 1
        if len(samples) < 5:
            samples.append({"case": index, "want": want, "got": got})
report["checks"]["acquire_0x487C50"] = {"cases": len(acquire_cases), "mismatches": mismatch,
                                        "samples": samples}
total += mismatch

# ---------------------------------------------------------------------------
# rebuild: 0x487E10 over selected group 3 then group 0 actors
# ---------------------------------------------------------------------------
def rebuild_row(defs, bounds, g1, g0, g3):
    row = [defs_token(defs), bounds_token(bounds), str(len(g1))] + \
          [record_token(r) for r in g1] + [str(len(g0))] + [record_token(r) for r in g0] + \
          [str(len(g3))] + [record_token(r) for r in g3]
    return row


rebuild_cases = []
for index in range(150 * args.scale):
    g3 = []
    for _ in range(random.randrange(0, 4)):
        g3.append((random.choice([1, 2, 3, 4, 5]), random.choice([0, 2]), random.choice([0, 5, 6]),
                   random.choice([0, 0x10000, -0x10000]), 0, 0))
    g1 = []
    for _ in range(random.randrange(0, 5)):
        g1.append((random.randrange(6), random.choice([0, 2]), random.choice([0, 5, 6]),
                   random.choice([0, 0x10000, 0x140000, 0x140064, -0x10000]), 0, 0))
    g0 = []
    for _ in range(random.randrange(0, 5)):
        g0.append((random.randrange(6), random.choice([0, 2]), random.choice([0, 5, 6]),
                   random.choice([0, 0x10000, 0x140000, 0x140064, -0x10000]), 0, 0))
    rebuild_cases.append((DEFS_SCAN, BOUNDS, g1, g0, g3))
# Directed: selection order and skips.
rebuild_cases.append((DEFS_SCAN, BOUNDS, [(1, 0, 5, 0, 0, 0)], [],
                      [(1, 0, 5, 0, 0, 0), (1, 2, 5, 0, 0, 0), (0, 0, 5, 0, 0, 0), (3, 0, 5, 0, 0, 0)]))
rebuild_cases.append((DEFS_SCAN, BOUNDS, [], [(1, 0, 5, 0, 0, 0), (2, 0, 6, 0x100000, 0, 0)],
                      [(4, 0, 5, 0, 0, 0)]))

native = probe("rebuild", [rebuild_row(*case) for case in rebuild_cases])
mismatch = 0
samples = []
for index, (case, line) in enumerate(zip(rebuild_cases, native)):
    defs, bounds, g1, g0, g3 = case
    setup_definitions(defs)
    for handle, (radius, height) in bounds.items():
        setup_object(handle, radius, height)
    put(LIST_COUNT, 0)
    u.mem_write(LIST_ARRAY, bytes(4 * 8))
    for slot, record in enumerate(g1):
        write_record(1, slot, record[0], byte6c=record[1], handle=record[2],
                     position=(record[3], record[4], record[5]))
    for slot, record in enumerate(g0):
        write_record(0, slot, record[0], byte6c=record[1], handle=record[2],
                     position=(record[3], record[4], record[5]))
    for slot, record in enumerate(g3):
        write_record(3, slot, record[0], byte6c=record[1], handle=record[2],
                     position=(record[3], record[4], record[5]))
    put(TABLES[1][1], len(g1))
    put(TABLES[0][1], len(g0))
    put(TABLES[3][1], len(g3))
    u.reg_write(UC_X86_REG_ESP, 0x300E000)
    u.reg_write(UC_X86_REG_FPCW, 0x037F)
    case_context[0] = index
    emu_run_rets(0x487E10, [RET_REBUILD])
    list_items = [normalize_pointer(word(LIST_ARRAY + slot * 4))
                  for slot in range(word(LIST_COUNT))]
    starts = []
    counts = []
    selected3 = []
    selected0 = []
    for group, selected in ((3, selected3), (0, selected0)):
        records = g3 if group == 3 else g0
        for slot, record in enumerate(records):
            base = TABLES[group][0] + slot * ACTOR_STRIDE
            if record[0] != 0 and (record[1] & 2) == 0:
                selected.append(slot)
                starts.append(normalize_list_start(word(base + 0x104)))
                counts.append(word(base + 0x108))
    fields = dict(part.split("=", 1) for part in line.split())
    want = (format_list(list_items), format_indices(selected3), format_indices(selected0),
            format_indices(starts), format_indices(counts))
    got = (fields["list"], fields["sel3"], fields["sel0"], fields["f104"], fields["f108"])
    if want != got:
        mismatch += 1
        if len(samples) < 5:
            samples.append({"case": index, "want": want, "got": got})
report["checks"]["rebuild_0x487E10"] = {"cases": len(rebuild_cases), "mismatches": mismatch,
                                        "samples": samples}
total += mismatch

# ---------------------------------------------------------------------------
# tick: 0x487EA0
# ---------------------------------------------------------------------------
def tick_row(defs, groups, extra, counter, paused, demo, local, tick):
    row = [defs_token(defs)]
    for group in groups:
        row.append(str(len(group)))
        for record in group:
            row.append(":".join(str(v) for v in record))
    row.append(str(len(extra)))
    for record in extra:
        row.append(":".join(str(v) for v in record))
    row.extend([counter, paused, demo, local, tick])
    return row


tick_cases = []
for index in range(300 * args.scale):
    sizes = [random.randrange(0, 5) for _ in range(4)]
    groups = []
    for group, size in enumerate(sizes):
        records = []
        for _ in range(size):
            records.append((random.randrange(3),
                            random.choice([0, 1, 2, 0x20, 0x21, 0x40, 0xFFFFFFFF, 0x80000000]),
                            random.choice([UPDATE_KEY, UPDATE_KEY, UPDATE_KEY_ALT]),
                            random.choice([0, 0x4563A0, SECONDARY_KEY]),
                            random.choice([0, 5]) if group == 3 else 0,
                            random.choice([0, 1, 7]) if group == 3 else 0))
        groups.append(records)
    extra = []
    for _ in range(random.randrange(0, 5)):
        extra.append((random.randrange(3), random.choice([0, 4, 5, 12, 13, 0xFFFFFFFF]),
                      UPDATE_KEY, random.choice([0, 0x4563A0, SECONDARY_KEY]), 0, 0))
    counter = random.choice([0, 1, 0x3F, 0x40])
    paused = 1 if index % 17 == 3 else 0
    demo = 1 if index % 13 == 5 else 0
    if demo and not sizes[3]:
        demo = 0
    local = random.randrange(sizes[3]) if demo else -1
    tick = random.choice([0, 1, 7, 8, 31, 32, 33, 63, 64, 0xFFFFFFFF])
    tick_cases.append((TICK_DEFS, groups, extra, counter, paused, demo, local, tick))
tick_cases.append((TICK_DEFS,
                   [[(1, 0, UPDATE_KEY, SECONDARY_KEY, 0, 0)],
                    [(1, 5, UPDATE_KEY, 0, 0, 0), (0, 7, UPDATE_KEY, 0, 0, 0)],
                    [(1, 0, UPDATE_KEY, 0, 0, 0)],
                    [(1, 0, UPDATE_KEY, 0, 5, 3)]], [], 0x3F, 0, 0, -1, 1))
tick_cases.append((TICK_DEFS,
                   [[(1, 1, UPDATE_KEY, 0, 0, 0)], [], [], [(1, 1, UPDATE_KEY, 0, 5, 0)]],
                   [], 0x40, 0, 0, -1, 2))
tick_cases.append((TICK_DEFS, [[], [], [], [(1, 3, UPDATE_KEY, 0, 5, 0)]], [], 0x40, 1, 0, -1, 2))
tick_cases.append((TICK_DEFS, [[], [], [], [(1, 0, UPDATE_KEY, 0, 5, 0)]], [], 1, 0, 1, 0, 4))
tick_cases.append((TICK_DEFS,
                   [[(1, 0, UPDATE_KEY, SECONDARY_KEY, 0, 0)],
                    [(1, 0, UPDATE_KEY, 0, 0, 0)], [(1, 0, UPDATE_KEY, 0, 0, 0)], []],
                   [(1, 4, UPDATE_KEY, SECONDARY_KEY, 0, 0)], 5, 0, 0, -1, 63))

native = probe("tick-hooks" if args.mutating_callbacks else "tick", [tick_row(*case) for case in tick_cases])
mismatch = 0
samples = []
for index, (case, line) in enumerate(zip(tick_cases, native)):
    defs, groups, extra, counter, paused, demo, local, tick = case
    setup_definitions(defs)
    put(COUNTER, counter)
    put(PAUSED, 1 if paused else 0)
    put(DEMO, 1 if demo else 0)
    put(LIST_COUNT, 0)
    u.mem_write(LIST_ARRAY, bytes(4 * 8))
    for group, (base, count_at, slots) in TABLES.items():
        for slot, record in enumerate(groups[group]):
            write_record(group, slot, record[0], fc=record[1], f110=record[2], f10c=record[3],
                         f18=record[4], f78=record[5])
        put(count_at, len(groups[group]))
    u.mem_write(EXTRA[0], bytes(EXTRA[2] * ACTOR_STRIDE))
    for slot, record in enumerate(extra):
        base = EXTRA[0] + slot * ACTOR_STRIDE
        put(base + 0x00, record[0])
        put(base + 0x04, DEF_TABLE + record[0] * 0x13C)
        put(base + 0xFC, record[1])
        put(base + 0x10C, record[3])
        put(base + 0x110, record[2])
        put(base + 0x18, record[4])
        put(base + 0x78, record[5])
    put(LOCAL, TABLES[3][0] + local * ACTOR_STRIDE if local >= 0 else 0)

    events = []
    marks = []
    live_trace=[]
    def live_call(kind, group=0, slot=0):
        if not args.mutating_callbacks:return
        live_trace.append(f'c,{kind},{group},{slot}')
        if kind==0:put(TABLES[group][0]+slot*ACTOR_STRIDE+0xfc,(slot+tick)%5)
        if kind==1 and groups[3]:put(TABLES[3][0]+0xfc,0)
        if kind==2 and groups[0]:put(TABLES[0][0]+0xfc,0)
        if kind==3 and (tick&31)<len(groups[1]):put(TABLES[1][0]+(tick&31)*ACTOR_STRIDE+0xfc,0)
        if kind==4 and (tick&63)<len(groups[2]):put(TABLES[2][0]+(tick&63)*ACTOR_STRIDE+0xfc,0)


    def hook_per_actor(machine, at, size, user):
        sp = u.reg_read(UC_X86_REG_ESP)
        events.append(("per_update", normalize_pointer(word(sp + 8)), word(sp + 4), word(sp + 12)))
        live_call(0,*normalize_pointer(word(sp+8)))
        u.reg_write(UC_X86_REG_ESP, sp + 4)
        u.reg_write(UC_X86_REG_EIP, word(sp))

    def hook_action(kind):
        def callback(machine, at, size, user):
            sp = u.reg_read(UC_X86_REG_ESP)
            events.append((kind, normalize_pointer(word(sp + 4)), at))
            if args.mutating_callbacks:
                actor=word(sp+4);group,slot=normalize_pointer(actor)
                action=0 if kind=="action_update" else 1
                live_trace.append(f'a,{action},{group},{slot},{at},{word(actor+0xfc)}')
                if action==0:
                    put(actor+0xfc,11+(slot&3));put(actor+0x78,word(actor+0x78)+3)
                    put(actor+0x24,word(actor+0x24)+0x1000+slot)
                    if group==0:put(actor+0x10c,SECONDARY_KEY if word(actor+0x10c)==0 else 0)
                else:
                    put(actor+0xfc,word(actor+0xfc)+7);put(actor+0x28,word(actor+0x28)^0x80000000)
            u.reg_write(UC_X86_REG_ESP, sp + 4)
            u.reg_write(UC_X86_REG_EIP, word(sp))
        return callback

    def hook_simple(letter):
        def callback(machine, at, size, user):
            events.append((letter,))
            live_call({"l":1,"a":2,"b":3,"c":4}[letter])
            sp = u.reg_read(UC_X86_REG_ESP)
            u.reg_write(UC_X86_REG_ESP, sp + 4)
            u.reg_write(UC_X86_REG_EIP, word(sp))
        return callback

    def hook_mark(machine, at, size, user):
        sp = u.reg_read(UC_X86_REG_ESP)
        marks.append(word(sp + 4))
        if args.mutating_callbacks:live_trace.append("m,"+str(word(sp+4)))
        u.reg_write(UC_X86_REG_ESP, sp + 4)
        u.reg_write(UC_X86_REG_EIP, word(sp))

    handles = []
    handles.append(u.hook_add(unicorn.UC_HOOK_CODE, hook_per_actor, begin=0x487370, end=0x487370))
    handles.append(u.hook_add(unicorn.UC_HOOK_CODE, hook_action("action_update"),
                              begin=UPDATE_KEY, end=UPDATE_KEY))
    handles.append(u.hook_add(unicorn.UC_HOOK_CODE, hook_action("action_update"),
                              begin=UPDATE_KEY_ALT, end=UPDATE_KEY_ALT))
    handles.append(u.hook_add(unicorn.UC_HOOK_CODE, hook_action("action_secondary"),
                              begin=SECONDARY_KEY, end=SECONDARY_KEY))
    for letter, address in (("l", 0x488200), ("a", 0x45E050), ("b", 0x464EB0), ("c", 0x465BD0)):
        handles.append(u.hook_add(unicorn.UC_HOOK_CODE, hook_simple(letter), begin=address, end=address))
    handles.append(u.hook_add(unicorn.UC_HOOK_CODE, hook_mark, begin=0x456E30, end=0x456E30))
    sp = 0x300E000
    u.mem_write(sp, struct.pack("<2I", RET_SENTINEL, tick))
    u.reg_write(UC_X86_REG_ESP, sp)
    u.reg_write(UC_X86_REG_FPCW, 0x037F)
    case_context[0] = index
    emu_run_rets(0x487EA0, RETS_TICK)
    for handle in handles:
        u.hook_del(handle)

    calls = []
    actions = []
    for event in events:
        if event[0] == "per_update":
            group, slot = event[1]
            if event[3] != (tick & 0xFFFFFFFF):
                raise SystemExit(f"tick case {index}: 487370 tick {event[3]} != {tick}")
            if event[2] != slot:
                raise SystemExit(f"tick case {index}: 487370 index {event[2]} != {slot}")
            calls.append(f"u,{group},{slot}")
        elif event[0] in ("l", "a", "b", "c"):
            calls.append(event[0])
        elif event[0] == "action_update":
            group, slot = event[1]
            actions.append(f"u,{group},{slot},{event[2]}")
        elif event[0] == "action_secondary":
            group, slot = event[1]
            actions.append(f"s,{group},{slot},{event[2]}")

    list_items = [normalize_pointer(word(LIST_ARRAY + slot * 4))
                  for slot in range(word(LIST_COUNT))]
    starts = []
    counts = []
    for group in (3, 0):
        records = groups[group]
        for slot, record in enumerate(records):
            base = TABLES[group][0] + slot * ACTOR_STRIDE
            # The tick row token has no +0x6C byte; write_record left it zero.
            if record[0] != 0:
                starts.append(normalize_list_start(word(base + 0x104)))
                counts.append(word(base + 0x108))
    t3 = [(word(TABLES[3][0] + slot * ACTOR_STRIDE + 0xFC),
           word(TABLES[3][0] + slot * ACTOR_STRIDE + 0x78)) for slot in range(len(groups[3]))]
    t0 = [word(TABLES[0][0] + slot * ACTOR_STRIDE + 0xFC) for slot in range(len(groups[0]))]
    t1 = [word(TABLES[1][0] + slot * ACTOR_STRIDE + 0xFC) for slot in range(len(groups[1]))]
    t2 = [word(TABLES[2][0] + slot * ACTOR_STRIDE + 0xFC) for slot in range(len(groups[2]))]
    xt = [(word(EXTRA[0] + slot * ACTOR_STRIDE + 0xFC),
           1 if word(EXTRA[0] + slot * ACTOR_STRIDE) == 0 else 0) for slot in range(len(extra))]

    fields = dict(part.split("=", 1) for part in line.split())
    want_calls = format_indices([]) if not calls else ";".join(calls)
    want_actions = format_indices([]) if not actions else ";".join(actions)
    want_marks = format_indices([]) if not marks else ";".join(str(m) for m in marks)
    want_t3 = format_list(t3)
    want_t0 = format_numbers(t0)
    want_t1 = format_numbers(t1)
    want_t2 = format_numbers(t2)
    want_xt = format_list(xt)
    want_f104 = format_indices(starts)
    want_f108 = format_indices(counts)
    if not calls:
        want_calls = "-"
    if not actions:
        want_actions = "-"
    if not marks:
        want_marks = "-"
    want = (want_calls, want_actions, want_marks, word(COUNTER), 1 if counter >= 0x40 else 0,
            format_list(list_items), want_f104, want_f108, want_t3, want_t0, want_t1, want_t2, want_xt)
    got = (fields["calls"], fields["actions"], fields["marks"], int(fields["counter"]),
           int(fields["rebuilt"]), fields["list"], fields["f104"], fields["f108"],
           fields["t3"], fields["t0"], fields["t1"], fields["t2"], fields["xt"])
    if args.mutating_callbacks:
        images=[]
        for group in range(5):
            base=EXTRA[0] if group==4 else TABLES[group][0]
            count=len(extra) if group==4 else len(groups[group])
            for slot in range(count):
                image=bytearray(u.mem_read(base+slot*ACTOR_STRIDE,ACTOR_STRIDE))
                definition=struct.unpack_from('<I',image,4)[0]
                struct.pack_into('<I',image,4,(definition-DEF_TABLE)//0x13c if definition else 0)
                pointer=struct.unpack_from('<I',image,0x104)[0]
                struct.pack_into('<I',image,0x104,normalize_list_start(pointer))
                images.append(image.hex())
        want+=((';'.join(live_trace) or '-'),(';'.join(images) or '-'))
        got+=(fields['trace'],fields['images'])
    if want != got:
        mismatch += 1
        if len(samples) < 5:
            samples.append({"case": index, "want": want, "got": got,
                            "groups": [[list(r) for r in g] for g in groups],
                            "extra": [list(r) for r in extra]})
report["checks"]["tick_0x487EA0"] = {"cases": len(tick_cases), "mismatches": mismatch,
                                     "samples": samples}
total += mismatch

report["mutating_callbacks"]=args.mutating_callbacks
report["full_actor_images_checked"]=len(tick_cases) if args.mutating_callbacks else 0
report["original_functions"] = [
    "487EA0-4881B5 per-tick actor update (three returns: 48804B, 488096, 4880B4)",
    "487E10-487E73 selected-actor list rebuild entry",
    "487C50-487CE1 per-actor nearby-actor list; 487CF7/487D72 scan blocks, cap 0x7D0",
    "487370, 488200, 45E050, 464EB0, 465BD0 recorded calls",
    "456E30 profiler marks recorded; callbacks at +110/+10C recorded",
]
report["hooks"] = ["488200/45E050/464EB0/465BD0 counted", "487370 records index, record, tick",
                   "addressed update and secondary callbacks record the target",
                   "456E30 records the mark", "returns stop each emulation"]
report["total_mismatches"] = total
text = json.dumps(report, indent=2)
print(text)
if args.report:
    args.report.write_text(text + "\n")

raise SystemExit(bool(total))
