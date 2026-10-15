#!/usr/bin/env python3
"""Check the rebuilt mission actor and person state stage against original Df.exe instructions.

Sections:
  convert  original 0x449710  BMS record -> actor record + person record (0x44A0C0)
  refresh  original 0x44A110  live-type counts (0x793680 slot table)
  reset    original 0x487E10  reset scan selection (0x487C50 hooked, never executed)
  slots    original 0x449E62-0x449E98  BMS group records -> type slots
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
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP,
                               UC_X86_REG_FPCW)

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mission import Resolver  # noqa: E402

SOURCE_SHA256 = "abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff"
RETURN = 0x412345
DEF_TABLE = 0x3000000
DEF_PTR = 0xC23810
DEF_COUNT = 0xC23814
ACTOR = 0x2002000
RECORD = 0x2001000
FAKE_ENTRY = 0x2003000
FAKE_NAME = 0x2003100
SCRATCH = 0x3000000
PERSON_TABLE = 0x794680
PERSON_STRIDE = 0xB4
ACTOR_STRIDE = 0x114
TYPE_TABLE = 0x793680
TYPE_STRIDE = 0x40
GROUP1_TABLE = 0x867E80
GROUP3_TABLE = 0x7CCA80
GROUP0_TABLE = 0x7DDE80
GROUP1_COUNT = 0x8CF904
GROUP2_TABLE = 0x8ACE80
GROUP3_COUNT = 0x8CFA18
GROUP0_COUNT = 0x8CFA1C

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("game", type=Path)
p.add_argument("probe", type=Path)
p.add_argument("--report", type=Path)
p.add_argument("--limit", type=int, default=0, help="cap BMS objects per mission for a quick run")
args = p.parse_args()

raw = (args.game / "Df.exe").read_bytes()
sha = hashlib.sha256(raw).hexdigest()
if sha != SOURCE_SHA256:
    raise SystemExit("Unsupported original executable hash")
pe = pefile.PE(data=raw)
u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
for at, size in [(0x400000, 0xA00000), (0x1000000, 0x200000), (0x2000000, 0x10000),
                 (0x3000000, 0x200000), (0x4000000, 0x1000)]:
    u.mem_map(at, size)
for section in pe.sections:
    if section.PointerToRawData and not section.Characteristics & 0x80:
        u.mem_write(0x400000 + section.VirtualAddress,
                    raw[section.PointerToRawData:section.PointerToRawData + section.SizeOfRawData])
PROBE = str(args.probe.resolve())


def put(at, value):
    u.mem_write(at, struct.pack("<I", value & 0xFFFFFFFF))


def put16(at, value):
    u.mem_write(at, struct.pack("<H", value & 0xFFFF))


def put8(at, value):
    u.mem_write(at, bytes([value & 0xFF]))


def word(at):
    return struct.unpack("<I", u.mem_read(at, 4))[0]


def le32(data, at):
    return struct.unpack_from("<I", data, at)[0]


def probe(mode, rows):
    text = "".join(" ".join(str(field) for field in row) + "\n" for row in rows)
    done = subprocess.run([PROBE, mode], input=text.encode(), capture_output=True)
    if done.returncode != 0:
        raise SystemExit("probe failed: " + done.stderr.decode(errors="replace"))
    lines = [line for line in done.stdout.decode().splitlines() if line]
    if len(lines) != len(rows):
        raise SystemExit(f"probe {mode}: {len(lines)} result lines for {len(rows)} cases")
    return lines


def emu_run(begin, until, count=200000):
    u.emu_start(begin, until, count=count)
    eip = u.reg_read(UC_X86_REG_EIP)
    if eip != until:
        raise SystemExit(f"original emulation incomplete: EIP=0x{eip:x}, expected 0x{until:x}")


def record():
    return bytearray(96)


def put_rec32(rec, at, value):
    struct.pack_into("<I", rec, at, value & 0xFFFFFFFF)


def put_rec16(rec, at, value):
    struct.pack_into("<H", rec, at, value & 0xFFFF)


def accepted(rec, multiplayer, rank):
    f0 = rec[0x0C]
    signed_rank = rank if rank < 0x80000000 else rank - 0x100000000
    if f0 & 0x10:
        if not multiplayer:
            return False
        if rec[0x45] > signed_rank:
            return False
    if f0 & 0x20:
        if multiplayer and rec[0x44] < signed_rank:
            return False
    if f0 & 0x40 and multiplayer:
        return False
    if f0 & 0x80 and not multiplayer:
        return False
    return True


def setup_defs(defs):
    put(DEF_PTR, DEF_TABLE)
    put(DEF_COUNT, len(defs))
    u.mem_write(DEF_TABLE, bytes(0x13C * len(defs)))
    for i, (ident, flags24, f6c, f84, f90, f94) in enumerate(defs):
        base = DEF_TABLE + i * 0x13C
        put(base + 0x20, ident)
        put8(base + 0x24, flags24)
        put(base + 0x6C, f6c)
        put(base + 0x84, f84)
        put(base + 0x90, f90)
        put16(base + 0x94, f94)


def normalize_actor(data, ndefs):
    out = bytearray(data)
    dptr = le32(out, 0x04)
    out[0x04:0x08] = struct.pack("<I", (dptr - DEF_TABLE) // 0x13C if DEF_TABLE <= dptr < DEF_TABLE + ndefs * 0x13C else 0)
    pptr = le32(out, 0x0C)
    out[0x0C:0x10] = struct.pack("<I", (pptr - PERSON_TABLE) // PERSON_STRIDE if pptr else 0)
    return bytes(out)


name_result = [0]


def hook_rtxt(machine, at, size, user):
    global name_result
    sp = u.reg_read(UC_X86_REG_ESP)
    u.reg_write(UC_X86_REG_EAX, name_result[0])
    u.reg_write(UC_X86_REG_ESP, sp + 4)
    u.reg_write(UC_X86_REG_EIP, word(sp))


def hook_sprintf(machine, at, size, user):
    sp = u.reg_read(UC_X86_REG_ESP)
    dest = word(sp + 4)
    value = word(sp + 12)
    signed = value if value < 0x80000000 else value - 0x100000000
    text = f"NAMEID{signed:03d}".encode() + b"\0"
    u.mem_write(dest, text)
    u.reg_write(UC_X86_REG_EAX, dest)
    u.reg_write(UC_X86_REG_ESP, sp + 4)
    u.reg_write(UC_X86_REG_EIP, word(sp))


u.hook_add(unicorn.UC_HOOK_CODE, hook_rtxt, begin=0x49A092, end=0x49A092)
u.hook_add(unicorn.UC_HOOK_CODE, hook_sprintf, begin=0x493A80, end=0x493A80)


def emu_convert(case):
    rec, defs, multiplayer, rank, counter, name, param3 = case
    setup_defs(defs)
    put(0x67F520, 1 if multiplayer else 0)
    put(0x67F530, rank & 0xFFFFFFFF)
    put(0x4F821C, counter)
    u.mem_write(RECORD, bytes(rec))
    u.mem_write(ACTOR, bytes(ACTOR_STRIDE))
    u.mem_write(PERSON_TABLE, bytes(PERSON_STRIDE * 2))
    if name is not None:
        u.mem_write(FAKE_NAME, name + b"\0")
        put(FAKE_ENTRY, FAKE_NAME)
        name_result[0] = FAKE_ENTRY
    else:
        name_result[0] = 0
    sp = 0x300E000
    u.mem_write(sp, struct.pack("<4I", RETURN, ACTOR, RECORD, param3))
    u.reg_write(UC_X86_REG_ESP, sp)
    u.reg_write(UC_X86_REG_FPCW, 0x037F)
    emu_run(0x449710, RETURN)
    actor = bytes(u.mem_read(ACTOR, ACTOR_STRIDE))
    person = None
    pptr = le32(actor, 0x0C)
    if pptr:
        slot = (pptr - PERSON_TABLE) // PERSON_STRIDE
        person = bytes(u.mem_read(PERSON_TABLE + slot * PERSON_STRIDE, PERSON_STRIDE))
    return actor, person


def defs_token(defs):
    return ";".join(",".join(str(v) for v in entry) for entry in defs)


def convert_row(case):
    rec, defs, multiplayer, rank, counter, name, _ = case
    return [rec.hex(), defs_token(defs), ACTOR, 1 if multiplayer else 0, rank, counter,
            name.hex() if name else "-"]


def run_convert_checks(convert_cases, label, report):
    native = probe("convert", [convert_row(case) for case in convert_cases])
    if len(native) != len(convert_cases):
        raise SystemExit(f"{label}: probe returned {len(native)} lines for {len(convert_cases)} cases")
    mismatch = 0
    samples = []
    for index, (case, line) in enumerate(zip(convert_cases, native)):
        fields = line.split()
        if len(fields) != 3:
            raise SystemExit(f"{label}: case {index} returned {len(fields)} fields")
        actor_hex, person_hex, counter_out = fields[0], fields[1], int(fields[2])
        rec, defs, multiplayer, rank, counter, name, param3 = case
        actor, person = emu_convert(case)
        want_actor = normalize_actor(actor, len(defs))
        got_actor = bytes.fromhex(actor_hex)
        want_person = person
        got_person = None if person_hex == "-" else bytes.fromhex(person_hex)
        want_counter = (counter + (11 if accepted(rec, multiplayer, rank) else 0)) & 0xFFFFFFFF
        if (want_actor != got_actor or want_person != got_person or want_counter != counter_out):
            mismatch += 1
            if len(samples) < 5:
                detail = []
                if want_actor != got_actor:
                    offsets = [i for i in range(len(want_actor)) if want_actor[i] != got_actor[i]]
                    detail.append("actor@" + ",".join(f"{i:x}:{want_actor[i]:02x}>{got_actor[i]:02x}"
                                                      for i in offsets[:8]))
                if want_person != got_person:
                    offsets = [] if want_person is None or got_person is None else \
                        [i for i in range(len(want_person)) if want_person[i] != got_person[i]]
                    detail.append("person@" + ("missing" if not offsets else
                                               ",".join(f"{i:x}:{want_person[i]:02x}>{got_person[i]:02x}"
                                                        for i in offsets[:8])))
                    detail.append("rec@" + rec[0x20:0x50].hex())
                if want_counter != counter_out:
                    detail.append(f"counter {want_counter}!={counter_out}")
                samples.append({"case": index, "type_id": le32(rec, 0), "flags": le32(rec, 0x0C),
                                "diff": detail})
    report[label] = {"cases": len(convert_cases), "mismatches": mismatch,
                     "samples": samples}
    return mismatch


report = {"source_sha256": sha, "probe": PROBE, "checks": {}}
total = 0

# ---- generated convert edge cases -----------------------------------------
random.seed(20261016)
edge = []
person_def = lambda ident, person: [(0xFFFFFFF0, 0, 0x11, 0x22, 0x33, 0x44),
                                    (ident, 4 if person else 0, 0x55, 0x66, 0x77, 0x88)]
for bits in range(16):
    for multiplayer in (0, 1):
        for rank in (0, 3, 5, 0xFFFFFFFF):
            for b44 in (5, 6):
                for b45 in (4, 5):
                    rec = record()
                    put_rec32(rec, 0, 3012)
                    put_rec32(rec, 0x0C, bits)
                    rec[0x44] = b44
                    rec[0x45] = b45
                    edge.append((rec, person_def(3012, bits & 1), multiplayer, rank,
                                 (len(edge) * 7) & 0xFFFFFFFF, None, 0))
for field in (0x20, 0x24, 0x28, 0x2C, 0x30):
    for value in (0, 1, 7, 100, 150, 32767, 0x7FFFFFFF, 0x40000000, 0x80000000, 0xFFFFFFFF):
        rec = record()
        put_rec32(rec, 0, 3012)
        put_rec32(rec, field, value)
        edge.append((rec, person_def(3012, True), 0, 0, value * 3, None, 0))
for field in (0x38, 0x3A, 0x3C, 0x3E, 0x40):
    for value in (0, 1, 2, 0x100, 0x7FFF, 0x8000, 0xFFFF):
        rec = record()
        put_rec32(rec, 0, 3012)
        put_rec16(rec, field, value)
        edge.append((rec, person_def(3012, True), 1, 5, value, None, 0))
for field, values in ((0x34, (0, 90, 180, 0x7FFF, 0x8000, 0xFFFF)),
                      (0x36, (0, 90, 180, 0x7FFF, 0x8000, 0xFFFF))):
    for value in values:
        rec = record()
        put_rec32(rec, 0, 3012)
        put_rec16(rec, field, value)
        edge.append((rec, person_def(3012, True), 0, 0, value, None, 0))
for _ in range(24):
    rec = bytearray(random.getrandbits(8) for _ in range(96))
    put_rec32(rec, 0, 3012)
    put_rec32(rec, 0x0C, random.randrange(256))
    edge.append((rec, person_def(3012, True), random.randrange(2), random.choice((0, 5, 0xFFFFFFFF)),
                 random.getrandbits(32), None, 0))
for ident in (0x1775, 0x1776, 0x177D, 3012):
    for value in (0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
        rec = record()
        put_rec32(rec, 0, ident)
        put_rec32(rec, 0x1C, value)
        rec[0x4A] = 0x6A
        edge.append((rec, person_def(ident, True), 0, 0, value, None, 0))
for name_id in (1, 7, 12, 255):
    for text in (b"AB", b"GRANT", b"People Names 12"):
        for person in (0, 1):
            rec = record()
            put_rec32(rec, 0, 3012)
            put_rec32(rec, 4, name_id)
            edge.append((rec, person_def(3012, person), 0, 0, name_id, text, 0x3002000))
    rec = record()
    put_rec32(rec, 0, 3012)
    put_rec32(rec, 4, name_id)
    edge.append((rec, person_def(3012, True), 0, 0, name_id, None, 0x3002000))
total += run_convert_checks(edge, "convert_edges_0x449710", report["checks"])

# ---- real BMS corpus -------------------------------------------------------
resolver = Resolver(args.game)
missions = []
for archive in resolver.archives:
    for entry in archive.entries:
        if not entry.deleted and entry.name.lower().endswith(".bms"):
            missions.append(entry.name)
missions = sorted(set(missions))
corpus = []
corpus_objects = 0
blocks = []
for name in missions:
    data = resolver.read(*resolver.find(name))[0]
    if data[:3] != b"BMS":
        raise SystemExit(f"{name}: not a BMS member")
    counts = struct.unpack_from("<4I", data, 152)
    at = 268
    groups = []
    for count in counts:
        groups.append([data[at + i * 96: at + i * 96 + 96] for i in range(count)])
        at += count * 96
    blocks.append((name, data, at, counts))
    for group in groups:
        for index, rec in enumerate(group):
            if args.limit and corpus_objects >= args.limit:
                break
            person = (corpus_objects % 2) == 0
            defs = person_def(le32(rec, 0), person)
            corpus.append((bytearray(rec), defs, 0, 0, (corpus_objects * 13) & 0xFFFFFFFF, None, 0))
            corpus_objects += 1
total += run_convert_checks(corpus, "convert_bms_corpus_0x449710", report["checks"])
report["checks"]["convert_bms_corpus_0x449710"]["missions"] = len(missions)
report["checks"]["convert_bms_corpus_0x449710"]["records"] = corpus_objects

# ---- live-type refresh -----------------------------------------------------
count_cases = []
for _ in range(40):
    sizes = [random.randrange(0, 9) for _ in range(3)]
    case = []
    for size in sizes:
        case.append(size)
        for _ in range(size):
            case.append(random.choice([0, 1, 5, 60, 63]))
    count_cases.append(case)
for _ in range(10):
    sizes = [random.randrange(0, 19) for _ in range(3)]
    case = []
    for size in sizes:
        case.append(size)
        for _ in range(size):
            case.append(random.randrange(64))
    count_cases.append(case)
native = probe("count", count_cases)
mismatch = 0
samples = []
for index, (case, line) in enumerate(zip(count_cases, native)):
    groups = []
    at = 0
    for _ in range(3):
        size = case[at]
        at += 1
        groups.append(case[at:at + size])
        at += size
    for table, count_at, values in ((GROUP0_TABLE, GROUP0_COUNT, groups[0]),
                                    (GROUP1_TABLE, GROUP1_COUNT, groups[1]),
                                    (GROUP3_TABLE, GROUP3_COUNT, groups[2])):
        for i, value in enumerate(values):
            put(table + i * ACTOR_STRIDE + 0x70, value)
        put(count_at, len(values))
    u.mem_write(TYPE_TABLE, b"\xAA" * (TYPE_STRIDE * 64))
    u.reg_write(UC_X86_REG_ESP, 0x300E000)
    u.reg_write(UC_X86_REG_FPCW, 0x037F)
    emu_run(0x44A110, 0x44A1F9)
    tokens = line.split()
    if len(tokens) != 64:
        raise SystemExit(f"live_type_refresh case {index}: {len(tokens)} slot tokens")
    for slot in range(64):
        want = (word(TYPE_TABLE + slot * TYPE_STRIDE + 0x28),
                word(TYPE_TABLE + slot * TYPE_STRIDE + 0x2C),
                word(TYPE_TABLE + slot * TYPE_STRIDE + 0x38))
        got = tuple(int(v, 10) for v in tokens[slot].split(","))
        if want != (got[0], got[1], got[2]):
            mismatch += 1
            if len(samples) < 5:
                samples.append({"case": index, "slot": slot, "want": want, "got": got[:3]})
            break
    for slot in range(64):
        base = TYPE_TABLE + slot * TYPE_STRIDE
        if (u.mem_read(base, 0x28) != b"\xAA" * 0x28 or
                u.mem_read(base + 0x30, 8) != b"\xAA" * 8):
            mismatch += 1
            break
report["checks"]["live_type_refresh_0x44A110"] = {"cases": len(count_cases), "mismatches": mismatch, "samples": samples}
total += mismatch

# ---- reset scan ------------------------------------------------------------
reset_cases = []
for _ in range(200):
    case = []
    for _ in range(2):
        size = random.randrange(0, 9)
        case.append(size)
        for _ in range(size):
            first = random.choice((0, 1, 2, 4096))
            byte6c = random.choice((0, 1, 2, 3, 0x12, 0x80))
            case.append(f"{first:x}:{byte6c:x}")
    reset_cases.append(case)
native = probe("reset", reset_cases)
mismatch = 0
samples = []
for index, (case, line) in enumerate(zip(reset_cases, native)):
    tokens = line.split()
    selection = []
    at = 0
    for _ in range(2):
        size = int(tokens[at])
        at += 1
        selection.append([int(v) for v in tokens[at:at + size]])
        at += size
    if at != len(tokens):
        raise SystemExit(f"reset case {index}: {len(tokens) - at} trailing tokens")
    done = []
    cursor = 0
    records = []
    for table_size in (case[0], case[1 + case[0]]):
        records.append(case[cursor + 1: cursor + 1 + table_size])
        cursor += 1 + table_size
    for table, count_at, entries in ((GROUP3_TABLE, GROUP3_COUNT, records[0]),
                                     (GROUP0_TABLE, GROUP0_COUNT, records[1])):
        for i, token in enumerate(entries):
            first, byte6c = (int(v, 16) for v in token.split(":"))
            u.mem_write(table + i * ACTOR_STRIDE, bytes(ACTOR_STRIDE))
            put(table + i * ACTOR_STRIDE, first)
            put8(table + i * ACTOR_STRIDE + 0x6C, byte6c)
        put(count_at, len(entries))
    put(0xC92A90, 0xDEADBEEF)
    put(0xC92A98, 0xDEADBEEF)

    def hook_reset(machine, at, size, user):
        sp = u.reg_read(UC_X86_REG_ESP)
        pointer = word(sp + 4)
        for base, slots, tag in ((GROUP3_TABLE, 256, 0), (GROUP0_TABLE, 1024, 1)):
            if base <= pointer < base + slots * ACTOR_STRIDE:
                done.append((tag, (pointer - base) // ACTOR_STRIDE))
                break
        u.reg_write(UC_X86_REG_ESP, sp + 4)
        u.reg_write(UC_X86_REG_EIP, word(sp))

    handle = u.hook_add(unicorn.UC_HOOK_CODE, hook_reset, begin=0x487C50, end=0x487C50)
    u.reg_write(UC_X86_REG_ESP, 0x300E000)
    emu_run(0x487E10, 0x487E73)
    u.hook_del(handle)
    got = [[index for tag, index in done if tag == 0], [index for tag, index in done if tag == 1]]
    if got != selection or word(0xC92A90) != 0 or word(0xC92A98) != 0:
        mismatch += 1
        if len(samples) < 5:
            samples.append({"case": index, "want": selection, "got": got})
report["checks"]["reset_scan_0x487E10"] = {"cases": len(reset_cases), "mismatches": mismatch, "samples": samples}
total += mismatch

# ---- loader type slots -----------------------------------------------------
def group_record_words(data, at):
    values = []
    for k in range(64):
        base = at + k * 32
        values.append((le32(data, base), le32(data, base + 8), le32(data, base + 12)))
    return values


slot_cases = []
for name, data, at, counts in blocks:
    slot_cases.append(group_record_words(data, at + 17408))
for _ in range(10):
    slot_cases.append([(random.getrandbits(32), random.getrandbits(32), random.getrandbits(32))
                       for _ in range(64)])
native = probe("slots", [[f"{a:x} {b:x} {c:x}" for a, b, c in case] for case in slot_cases])
mismatch = 0
samples = []
for index, (case, line) in enumerate(zip(slot_cases, native)):
    feed = list(case)
    def hook_fread(machine, at, size, user):
        sp = u.reg_read(UC_X86_REG_ESP)
        dest = word(sp + 4)
        payload = feed.pop(0)
        u.mem_write(dest, struct.pack("<8I", payload[0], 0, payload[1], payload[2], 0, 0, 0, 0))
        u.reg_write(UC_X86_REG_EAX, 1)
        u.reg_write(UC_X86_REG_ESP, sp + 4)
        u.reg_write(UC_X86_REG_EIP, word(sp))
    u.mem_write(TYPE_TABLE, b"\xAA" * (TYPE_STRIDE * 64))
    handle = u.hook_add(unicorn.UC_HOOK_CODE, hook_fread, begin=0x499280, end=0x499280)
    u.reg_write(UC_X86_REG_ESP, 0x300E000)
    u.reg_write(UC_X86_REG_FPCW, 0x037F)
    emu_run(0x449E60, 0x449E9A)
    u.hook_del(handle)
    if feed:
        raise SystemExit(f"loader slots case {index}: {len(feed)} group records never read")
    tokens = line.split()
    if len(tokens) != 64:
        raise SystemExit(f"loader slots case {index}: {len(tokens)} slot tokens")
    for slot in range(64):
        want = (word(TYPE_TABLE + slot * TYPE_STRIDE + 0x00),
                word(TYPE_TABLE + slot * TYPE_STRIDE + 0x30),
                word(TYPE_TABLE + slot * TYPE_STRIDE + 0x34))
        got = tuple(int(v, 10) for v in tokens[slot].split(","))
        if want != got:
            mismatch += 1
            if len(samples) < 3:
                samples.append({"case": index, "slot": slot, "want": want, "got": got})
            break
    for slot in range(64):
        base = TYPE_TABLE + slot * TYPE_STRIDE
        if (u.mem_read(base + 0x04, 0x2C) != b"\xAA" * 0x2C or
                u.mem_read(base + 0x38, 8) != b"\xAA" * 8):
            mismatch += 1
            break
report["checks"]["loader_type_slots_0x449E62"] = {"cases": len(slot_cases), "mismatches": mismatch, "samples": samples}
total += mismatch

# ---- person +70 reverse divide --------------------------------------------
# Original 449978-44999D sequence replayed at a scratch address; the truncate
# helper 49BAE6 is executed from original bytes.
PERCENT_CODE = 0x3000100
percent_program = bytes.fromhex("db05") + struct.pack("<I", 0x3000000)      # fild dword [in]
percent_program += bytes.fromhex("dc35") + struct.pack("<I", 0x3000020)     # fdiv qword [100.0]
percent_program += bytes.fromhex("d9e8")                                    # fld1
percent_program += bytes.fromhex("def1")                                    # fdivrp st(1)
percent_program += bytes.fromhex("dc0d") + struct.pack("<I", 0x3000030)     # fmul qword [65536.0]
percent_call = (0x49BAE6 - (PERCENT_CODE + len(percent_program) + 5)) & 0xFFFFFFFF
percent_program += bytes.fromhex("e8") + struct.pack("<I", percent_call)    # call 49BAE6
percent_program += bytes.fromhex("db1d") + struct.pack("<I", 0x3000010)     # fistp dword [out]
u.mem_write(0x3000020, struct.pack("<d", 100.0))
u.mem_write(0x3000030, struct.pack("<d", 65536.0))
u.mem_write(PERCENT_CODE, percent_program)


def percent_original(value):
    u.mem_write(0x3000000, struct.pack("<i", value))
    u.mem_write(0x3000010, b"\x00" * 4)
    u.reg_write(UC_X86_REG_ESP, 0x300E000)
    u.reg_write(UC_X86_REG_FPCW, 0x037F)
    emu_run(PERCENT_CODE, PERCENT_CODE + len(percent_program))
    return struct.unpack("<I", u.mem_read(0x3000010, 4))[0]


divisors = sorted({2 ** a * 5 ** b for a in range(19) for b in range(3)})
percent_cases = [x for x in divisors] + [-x for x in divisors]
percent_cases += list(range(-20000, 20001))
percent_cases += [d + 1 for d in divisors] + [d - 1 for d in divisors]
percent_cases += [1, -1, 2, -2, 100, -100, 101, -101, 32767, -32767, 32768, -32768,
                  65535, -65535, 65536, -65536, 6553600, -6553600, 6553601, -6553601,
                  0x7FFFFFFF, -0x80000000, 0x7FFFFFFE, -0x7FFFFFFF]
rng = random.Random(99)
percent_cases += [rng.randint(-0x80000000, 0x7FFFFFFF) for _ in range(5000)]
percent_cases += [6553600 // k for k in range(1, 2001)]
percent_cases += [6553600 // k + 1 for k in range(1, 2001)]
seen = set()
percent_cases = [x for x in percent_cases if x != 0 and not (x in seen or seen.add(x))]
native = probe("percent", [[x] for x in percent_cases])
mismatch = 0
samples = []
for index, (value, line) in enumerate(zip(percent_cases, native)):
    want = percent_original(value)
    got = int(line)
    if want != got:
        mismatch += 1
        if len(samples) < 5:
            samples.append({"case": index, "value": value, "original": want, "native": got})
divisor_checks = [(d, percent_original(d), 6553600 // d) for d in divisors]
if any(original != exact for _, original, exact in divisor_checks):
    raise SystemExit("divisor golden set no longer matches original x86")
report["checks"]["percent_reverse_divide_0x449978"] = {
    "cases": len(percent_cases), "mismatches": mismatch, "divisors": len(divisors),
    "dense_range": 40000, "samples": samples}
total += mismatch

report["original_functions"] = [
    "449710-449C67 BMS record -> actor and person state",
    "44A0C0-44A102 person slot allocator (called by 449710)",
    "44A110-44A1F9 live-type count refresh",
    "487E10-487E73 reset scan (thunk 488970; 487C50 hooked, never executed)",
    "449E60-449E98 group record -> type slot copy",
    "449978-44999D person +70 x87 sequence replayed (49BAE6 executed)",
    "4828C0-4828EE item lookup (executed)",
    "49A620/4BA590 memset (executed), 49BAE6 x87 truncate (executed)",
]
report["hooks"] = ["493A80 formats NAMEID%03i", "49A092 returns the case name or no entry",
                   "487C50 records the reset argument only", "499280 feeds group records"]
report["total_mismatches"] = total
text = json.dumps(report, indent=2)
print(text)
if args.report:
    args.report.write_text(text + "\n")
