#!/usr/bin/env python3
"""Check the rebuilt NPC/player behavior callbacks against original Df.exe code.

Callbacks: 45B590 null, 45B5A0 org0, 45B5B0 org1 (human person), 45BDF0 plyr.
Foreign calls are answered deterministically so both sides follow the same
branch flow; every call is recorded and compared in order with the port's
typed actions, and the final actor/person records are compared byte for byte
(actor +0x0C pointer normalized).

Executed for real: 45B3A0, 45B400, 45C6A0, 45C810, 45C880, 477670, 477860,
and the integer paths of 477770.
Hooked: 460410, 45F2C0, 45E280, 45F090, 45F010, 45FB00, 45EDD0, 42E2B0,
477770's FPU path at 0x4777CA, 42CEF0, 442760, 445CE0, 42EDD0, 42EE00, 42EE30,
44A3E0, 45FD90, 45FE30, 442810, 4604C0, and the weapon-table reads.
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
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
                               UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP,
                               UC_X86_REG_FPCW)

SOURCE_SHA256 = "abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff"
WEAPON_TABLE = 0x2000000
ACTOR = 0x2010000
PERSON = 0x2011000
TARGET_BASE = 0x2012000
MARKER_BASE = 0x8ACE80  # original group-2 marker table base
TARGET_STRIDE = 0x114

SITES = {
    "anim": 0x460410, "combat": 0x45F2C0, "combat_result": 0x45F2C1, "target": 0x45E280,
    "selector": 0x45F090, "nearby": 0x45F010, "ray": 0x45FB00, "facing": 0x45EDD0,
    "sight": 0x42E2B0, "angle": 0x477770, "anim810": 0x45C832, "anim880": 0x45C8A8,
    "weapon_step": 0x45B953, "set_anim": 0x42CEF0, "event11": 0x442760, "message": 0x445CE0,
    "region1": 0x42EDD0, "region2": 0x42EE00, "region3": 0x42EE30, "slot": 0x44A3E0,
    "zone": 0x45FD90, "zone2": 0x45FE30, "setup": 0x442810, "ground": 0x4604C0,
}
QUERY = {SITES["anim"], SITES["target"], SITES["selector"], SITES["nearby"], SITES["ray"],
         SITES["facing"], SITES["sight"], SITES["angle"]}
WEAPON = {SITES["anim810"], SITES["anim880"], SITES["weapon_step"]}
RETS = {
    "null": (0x45B59E,),
    "org0": (0x45B5A0,),
    "org1": (0x45B602, 0x45B8B6, 0x45B8CC, 0x45B92E, 0x45B970, 0x45B983, 0x45B9DB, 0x45B9FD,
             0x45BA3B, 0x45BA95, 0x45BAD3, 0x45BB50, 0x45BB63, 0x45BBB0, 0x45BC77, 0x45BCAD,
             0x45BD18, 0x45BDEF),
    "plyr": (0x45BF19, 0x45BFB2),
}
ENTRY = {"null": 0x45B590, "org0": 0x45B5A0, "org1": 0x45B5B0, "plyr": 0x45BDF0}

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("game", type=Path)
p.add_argument("probe", type=Path)
p.add_argument("--report", type=Path)
p.add_argument("--cases", type=int, default=300)
p.add_argument("--seed", type=int, default=20261016)
p.add_argument("--live-queries",action="store_true")
args = p.parse_args()

raw = (args.game / "Df.exe").read_bytes()
sha = hashlib.sha256(raw).hexdigest()
if sha != SOURCE_SHA256:
    raise SystemExit("Unsupported original executable hash")
pe = pefile.PE(data=raw)
u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
for at, size in [(0x400000, 0xA00000), (0x1000000, 0x200000), (0x2000000, 0x100000),
                 (0x3000000, 0x200000), (0x4000000, 0x1000)]:
    u.mem_map(at, size)
for section in pe.sections:
    if section.PointerToRawData and not section.Characteristics & 0x80:
        u.mem_write(0x400000 + section.VirtualAddress,
                    raw[section.PointerToRawData:section.PointerToRawData + section.SizeOfRawData])
PROBE = str(args.probe.resolve())

MASK = 0xFFFFFFFF


def put(at, value):
    u.mem_write(at, struct.pack("<I", value & MASK))


def word(at):
    return struct.unpack("<I", u.mem_read(at, 4))[0]


def image_word(image, at):
    return struct.unpack_from("<I", image, at)[0]


def image_put(image, at, value):
    struct.pack_into("<I", image, at, value & MASK)


def image_put16(image, at, value):
    struct.pack_into("<H", image, at, value & 0xFFFF)


def weapon_value(index):
    return (index * 0x111 + 4) & 0xFFFF


def probe_rows(rows):
    text = "".join(" ".join(str(f) for f in row) + "\n" for row in rows)
    done = subprocess.run([PROBE, "live" if args.live_queries else "case"], input=text.encode(), capture_output=True)
    if done.returncode != 0:
        raise SystemExit("probe failed: " + done.stderr.decode(errors="replace"))
    lines = [line for line in done.stdout.decode().splitlines() if line]
    if len(lines) != len(rows):
        raise SystemExit(f"probe returned {len(lines)} lines for {len(rows)} cases")
    return lines


def parse_actions(token):
    actions = []
    if token == "-":
        return actions
    for part in token.split(";"):
        fields = part.split(",")
        values = [int(v) for v in fields[2:]]
        while len(values) < 3:
            values.append(0)
        actions.append((fields[0], int(fields[1], 16), tuple(values[:3])))
    return actions


rng = random.Random(args.seed)
cases = []
for index in range(args.cases):
    callback = ["org1", "org1", "org1", "plyr", "null", "org0"][index % 6]
    actor_image = bytearray(0x114)
    person_image = bytearray(0xB4)
    image_put(actor_image, 0x00, 0)
    image_put(actor_image, 0x04, 0)
    image_put(actor_image, 0x0C, 0)
    image_put16(actor_image, 0x2E, rng.randrange(4))
    image_put16(actor_image, 0x32, rng.randrange(4))
    image_put(actor_image, 0x24, rng.randrange(0, 0x200000) - 0x100000)
    image_put(actor_image, 0x28, rng.randrange(0, 0x200000) - 0x100000)
    image_put(actor_image, 0x2C, rng.randrange(0, 0x200000) - 0x100000)
    image_put(actor_image, 0x30, rng.randrange(0x200))
    image_put16(actor_image, 0x70, rng.choice([1, 2, 4, 5]))
    image_put16(actor_image, 0x72, rng.randrange(4))
    actor_image[0x6C] = rng.choice([0, 0, 0, 4])
    actor_image[0x6D] = rng.choice([0, 1])
    image_put16(actor_image, 0x74, rng.choice([0, 0, 1, 0xFFFF]))
    image_put(actor_image, 0x7C, rng.choice([0, 0x14, 0x44]))
    image_put(actor_image, 0x84, rng.choice([0, 0x14, 0x64]))
    image_put(actor_image, 0x94, rng.randrange(4))
    image_put(actor_image, 0xF8, 0)
    image_put(actor_image, 0xFC, rng.choice([0, 1, 0x40]))
    image_put(person_image, 0x00, 0)
    person_image[0x24] = rng.choice([0, 0, 2, 0x10, 0x20, 0x40, 0x80, 0x82])
    person_image[0x25] = rng.choice([0, 0, 1])
    image_put(person_image, 0x28, rng.choice([0, 1, 2]))
    image_put(person_image, 0x2C, rng.choice([0, 1, 0x100, 0x300]))
    image_put(person_image, 0x30, rng.randrange(0x200))
    image_put(person_image, 0x38, rng.choice([0, 0xF7, 0xF8, 0x180]))
    image_put(person_image, 0x3C, rng.choice([0, 0x1F0, 0x1F1, 0x240]))
    image_put(person_image, 0x40, rng.choice([0, 0xBA, 0xBB]))
    image_put(person_image, 0x44, 0)
    image_put(person_image, 0x48, rng.randrange(0, 0x100000))
    image_put(person_image, 0x4C, rng.randrange(0, 0x100000))
    image_put(person_image, 0x50, rng.randrange(0, 0x100000))
    image_put(person_image, 0x64, rng.randrange(4))
    image_put(person_image, 0x6C, rng.choice([0, 0, 1]))
    image_put(person_image, 0x80, rng.choice([0, 0x400000, 0x2000000]))
    image_put(person_image, 0x84, rng.choice([0, 0x10000, 0x100000]))
    image_put(person_image, 0x88, rng.choice([0, 0x40, 0x200]))
    image_put(person_image, 0x8C, rng.choice([0, 0x40, 0x200]))
    image_put(person_image, 0x94, rng.choice([0, 0x4000, 0x10000]))
    person_image[0x9C] = rng.choice([0, 1, 2, 2, 3])
    image_put(person_image, 0xA0, rng.choice([0, 1]))
    # plyr reads +0xAC as a marker-list index unconditionally; org1 reads it
    # as a list index (destination 1) or a marker record pointer (destination 3).
    if callback == "plyr":
        image_put(person_image, 0xA4, rng.choice([0, 1]))
        image_put(person_image, 0xAC, rng.choice([0, 1, 2, 3]))
    else:
        # Destination 3 (a record pointer) is not generated: the original's
        # waypoint arrival code treats +0xAC as a list index and faults for a
        # record pointer, while the port raises BehaviorError. See the needs
        # file for the +0xA4 = 3 semantics.
        destination = rng.choice([0, 1, 1])
        image_put(person_image, 0xA4, destination)
        if destination == 1:
            image_put(person_image, 0xAC, rng.choice([0, 1, 2, 3]))
        else:
            image_put(person_image, 0xAC, 0)
    image_put(person_image, 0xB0, rng.choice([0, 1, 2, 3]))

    targets = []
    for t in range(2):
        target = bytearray(0x114)
        image_put(target, 0x00, 0)
        image_put(target, 0x04, 0)
        image_put(target, 0x70, rng.choice([1, 2, 4]) << 16)
        target[0x6C] = rng.choice([0, 4])
        image_put(target, 0xF8, 0)
        image_put(target, 0x24, rng.randrange(0, 0x100000))
        image_put(target, 0x28, rng.randrange(0, 0x100000))
        image_put(target, 0x2C, rng.randrange(0, 0x100000))
        targets.append(target)
    markers = []
    for m in range(3):
        marker = bytearray(0x114)
        image_put(marker, 0x08, rng.choice([0, 1, 2, 3]))
        image_put(marker, 0x24, rng.randrange(0, 0x400000) - 0x200000)
        image_put(marker, 0x28, rng.randrange(0, 0x400000) - 0x200000)
        image_put(marker, 0x2C, rng.randrange(0, 0x10000))
        image_put(marker, 0x30, rng.randrange(0x1000))
        image_put(marker, 0x7C, rng.choice([0, 0x1000, 0x100000]))
        markers.append(marker)
    lists = []
    for l in range(4):
        entry_count = rng.randrange(0, 4)
        lists.append((rng.choice([0, 1]), entry_count,
                      [rng.randrange(3) for _ in range(entry_count)]))
    cases.append({
        "callback": callback,
        "actor": bytes(actor_image),
        "person": bytes(person_image),
        "targets": targets,
        "markers": markers,
        "lists": lists,
        "host": rng.choice([0, 1]),
        "param2": rng.choice([0, 0, 4]),
        "rng": rng.getrandbits(32),
    })

rows = []
expected = []
for case_index, case in enumerate(cases):
    callback = case["callback"]
    person = case["person"]
    actor = case["actor"]
    weapon_index = image_word(person, 0x64) if callback == "org1" else image_word(actor, 0x94)

    def answer(site, call_index):
        h = (site * 2654435761 + call_index * 2246822519) & MASK
        if site == SITES["target"]:
            return (TARGET_BASE, 0x4000 + ((h >> 9) % 4) * 0x2000)
        if site == SITES["ray"]:
            return (h & 1, 0)
        if site == SITES["sight"]:
            return ((h & 0x3FFFF) - 0x20000, 0)
        if site == SITES["nearby"]:
            return (h & 1, 0)
        if site == SITES["selector"]:
            return (h % 4, 0)
        if site == SITES["angle"]:
            return (0x40000000, 0)
        if site == SITES["facing"]:
            return (h, 0)
        if site == SITES["anim"]:
            return (0x10 + h % 0x30, 0)
        return (h & 0xFFFF, 0)

    # Emulated world setup.
    u.mem_write(ACTOR, bytes(actor))
    put(ACTOR + 0x0C, PERSON)
    u.mem_write(PERSON, bytes(person))
    for t, target in enumerate(case["targets"]):
        u.mem_write(TARGET_BASE + t * TARGET_STRIDE, bytes(target))
    for m, marker in enumerate(case["markers"]):
        u.mem_write(MARKER_BASE + m * TARGET_STRIDE, bytes(marker))
    u.mem_write(0x78F280, bytes(0x88 * 128))
    for l, (flags, count, indices) in enumerate(case["lists"]):
        put(0x78F280 + l * 0x88, flags)
        put(0x78F280 + l * 0x88 + 4, count)
        for i, entry in enumerate(indices):
            put(0x78F280 + l * 0x88 + 8 + i * 4, entry)
    put(0x67F528, case["host"])
    put(0xBD0060, case["rng"])
    put(0xAD3ED0, WEAPON_TABLE)
    u.mem_write(WEAPON_TABLE, bytes(0x100 * 0xA8))
    for i in range(0x100):
        put(WEAPON_TABLE + i * 0xA8 + 0x9C, weapon_value(i))
        put(WEAPON_TABLE + i * 0xA8 + 0xA4, weapon_value(i))

    events = []
    inputs = []
    state = {"index": 0}

    def add_call(site, args, value=0, value2=0):
        state["index"] += 1
        events.append((site, tuple(args)))
        if site in QUERY or site in WEAPON or site == SITES["combat_result"]:
            inputs.append((site, value & MASK, value2 & MASK))
        return value

    def hook_call(site, arity, arg_offset=0):
        def callback(machine, at, size, user):
            sp = u.reg_read(UC_X86_REG_ESP)
            stack_args = [word(sp + 4 + i * 4) for i in range(arity)]
            args = stack_args[arg_offset:]
            if site == SITES["event11"]:
                actor_ptr = stack_args[0]
                args = [(actor_ptr - 0x7CCA80) // TARGET_STRIDE, word(actor_ptr + 0x84)]
            if site == SITES["message"]:
                args = [(stack_args[0] - 0x7CCA80) // TARGET_STRIDE, stack_args[1]]
            if site == SITES["target"]:
                value, value2 = answer(site, state["index"])
                put(stack_args[1], value2)
            elif site in WEAPON:
                value, value2 = weapon_value(weapon_index), 0
            else:
                value, value2 = answer(site, state["index"])
                if site not in QUERY:
                    value, value2 = 0, 0
            add_call(site, args, value, value2)
            u.reg_write(UC_X86_REG_EAX, value)
            u.reg_write(UC_X86_REG_ESP, sp + 4)
            u.reg_write(UC_X86_REG_EIP, word(sp))
        return callback

    def hook_weapon(site):
        def callback(machine, at, size, user):
            add_call(site, [weapon_index], weapon_value(weapon_index))
        return callback

    def hook_combat(machine, at, size, user):
        sp = u.reg_read(UC_X86_REG_ESP)
        add_call(SITES["combat"], [])
        value, _ = answer(SITES["combat_result"], state["index"])
        add_call(SITES["combat_result"], [], value)
        put(PERSON + 0x6C, value)
        u.reg_write(UC_X86_REG_ESP, sp + 4)
        u.reg_write(UC_X86_REG_EIP, word(sp))

    def hook_angle(machine, at, size, user):
        # FPU path of 477770; skip its epilogue and return the scripted angle.
        sp = u.reg_read(UC_X86_REG_ESP)
        dx = struct.unpack("<i", u.mem_read(sp + 0x20, 4))[0]
        dy = struct.unpack("<i", u.mem_read(sp + 0x24, 4))[0]
        value, _ = answer(SITES["angle"], state["index"])
        add_call(SITES["angle"], [dx & MASK, dy & MASK], value)
        return_address = word(sp + 0x1C)
        u.reg_write(UC_X86_REG_EAX, value)
        # Restore the registers the skipped epilogue would pop.
        u.reg_write(UC_X86_REG_ESI, word(sp + 0x14))
        u.reg_write(UC_X86_REG_EBX, word(sp + 0x18))
        u.reg_write(UC_X86_REG_ESP, sp + 0x20)
        u.reg_write(UC_X86_REG_EIP, return_address)

    fired = []

    def hook_ret(machine, at, size, user):
        fired.append(at)
        u.emu_stop()

    handles = []
    for name, arity, offset in (("anim", 4, 1), ("selector", 1, 1), ("nearby", 1, 1),
                                ("ray", 3, 1), ("facing", 1, 1), ("sight", 4, 1),
                                ("target", 2, 2), ("set_anim", 3, 1), ("event11", 1, 0),
                                ("message", 2, 0), ("region1", 2, 0), ("region2", 2, 0),
                                ("region3", 3, 0), ("slot", 1, 0), ("zone", 1, 1),
                                ("zone2", 1, 1), ("setup", 1, 1), ("ground", 1, 1)):
        handles.append(u.hook_add(unicorn.UC_HOOK_CODE, hook_call(SITES[name], arity, offset),
                                  begin=SITES[name], end=SITES[name]))
    handles.append(u.hook_add(unicorn.UC_HOOK_CODE, hook_combat,
                              begin=SITES["combat"], end=SITES["combat"]))
    handles.append(u.hook_add(unicorn.UC_HOOK_CODE, hook_angle, begin=0x4777CA, end=0x4777CA))
    for site in WEAPON:
        handles.append(u.hook_add(unicorn.UC_HOOK_CODE, hook_weapon(site), begin=site, end=site))
    for ret in RETS[callback]:
        handles.append(u.hook_add(unicorn.UC_HOOK_CODE, hook_ret, begin=ret, end=ret))
    if word(ACTOR + 0x0C) != PERSON:
        raise SystemExit(f"case {case_index}: actor person pointer is 0x{word(ACTOR + 0x0C):x}")
    sp = 0x300E000
    u.mem_write(sp, struct.pack("<3I", 0x412345, ACTOR, case["param2"]))
    u.reg_write(UC_X86_REG_ESP, sp)
    u.reg_write(UC_X86_REG_FPCW, 0x037F)
    try:
        u.emu_start(ENTRY[callback], 0x1000, count=400000)
    except unicorn.UcError as error:
        for handle in handles:
            u.hook_del(handle)
        raise SystemExit(f"case {case_index} ({callback}): emulation failed at "
                         f"0x{u.reg_read(UC_X86_REG_EIP):x}: {error}; "
                         f"eax=0x{u.reg_read(UC_X86_REG_EAX):x} ecx=0x{u.reg_read(UC_X86_REG_ECX):x} "
                         f"esi=0x{u.reg_read(UC_X86_REG_ESI):x} "
                         f"a4=0x{word(PERSON+0xA4):x} ac=0x{word(PERSON+0xAC):x} b0=0x{word(PERSON+0xB0):x}")
    for handle in handles:
        u.hook_del(handle)
    if len(fired) != 1:
        raise SystemExit(f"case {case_index} ({callback}): reached {len(fired)} return addresses")

    final_actor = bytearray(u.mem_read(ACTOR, 0x114))
    final_person = bytes(u.mem_read(PERSON, 0xB4))
    final_actor[0x0C:0x10] = b"\0\0\0\0"
    final_rng = word(0xBD0060)

    markers_token = "-" if not case["markers"] else ";".join(
        f"{struct.unpack('<i', m[0x24:0x28])[0]},{struct.unpack('<i', m[0x28:0x2C])[0]},"
        f"{struct.unpack('<i', m[0x2C:0x30])[0]},{image_word(m, 0x30)},{image_word(m, 0x7C)},"
        f"{image_word(m, 0x08)}" for m in case["markers"])
    lists_token = "-" if not case["lists"] else ";".join(
        ",".join([str(f), str(c)] + [str(i) for i in idx]) for f, c, idx in case["lists"])
    targets_token = "-" if not case["targets"] else ";".join(
        f"{TARGET_BASE + t * TARGET_STRIDE:x},{image_word(target, 0x70):x},{target[0x6C]},"
        f"{image_word(target, 0xF8):x},{struct.unpack('<i', target[0x24:0x28])[0]},"
        f"{struct.unpack('<i', target[0x28:0x2C])[0]},{struct.unpack('<i', target[0x2C:0x30])[0]}"
        for t, target in enumerate(case["targets"]))
    mres_token = ";".join(f"{MARKER_BASE + m * TARGET_STRIDE:x},{m}" for m in range(3))
    inputs_token = "-" if not inputs else ";".join(
        f"{site:x},{value:x},{value2:x}" for site, value, value2 in inputs)
    rows.append([callback, case["rng"], case["host"], case["param2"], hex(ACTOR),
                 bytes(actor).hex(), bytes(person).hex(), markers_token, lists_token,
                 targets_token, mres_token, inputs_token])
    expected.append({
        "callback": callback,
        "events": events,
        "actor": bytes(final_actor),
        "person": final_person,
        "rng": final_rng,
        "inputs": len(inputs),
    })

import os
if os.environ.get("AB_DUMP"):
    with open(os.environ["AB_DUMP"], "w") as dump:
        for row in rows:
            dump.write(" ".join(str(f) for f in row) + "\n")
results = probe_rows(rows)
mismatch = 0
samples = []
for index, (case, line, want) in enumerate(zip(cases, results, expected)):
    fields = line.split()
    if len(fields) != 5:
        raise SystemExit(f"case {index}: probe returned {len(fields)} fields")
    got_actor = bytes.fromhex(fields[0])
    got_person = bytes.fromhex(fields[1])
    got_actions = parse_actions(fields[2])
    got_cursor = int(fields[3])
    got_rng = int(fields[4])
    want_actions = []
    for site, params in want["events"]:
        kind = "q" if site in QUERY or site in WEAPON or site == SITES["combat_result"] else "e"
        values = list(params)[:3] + [0] * (3 - len(params[:3]))
        want_actions.append((kind, site, tuple(values)))
    diffs = []
    if got_actions != want_actions:
        first = next((i for i, (a, b) in enumerate(zip(got_actions, want_actions)) if a != b), None)
        diffs.append(f"actions at {first}: got {got_actions[first] if first is not None else None} "
                     f"want {want_actions[first] if first is not None else None} "
                     f"({len(got_actions)} vs {len(want_actions)})")
    if got_actor != want["actor"]:
        offsets = [o for o in range(0x114) if got_actor[o] != want["actor"][o]][:6]
        diffs.append("actor@" + ",".join(
            f"{o:x}:{got_actor[o]:02x}>{want['actor'][o]:02x}" for o in offsets))
    if got_person != want["person"]:
        offsets = [o for o in range(0xB4) if got_person[o] != want["person"][o]][:6]
        diffs.append("person@" + ",".join(
            f"{o:x}:{got_person[o]:02x}>{want['person'][o]:02x}" for o in offsets))
    if got_cursor != want["inputs"]:
        diffs.append(f"cursor {got_cursor} vs {want['inputs']}")
    if got_rng != want["rng"]:
        diffs.append(f"rng {got_rng:x} vs {want['rng']:x}")
    if diffs:
        mismatch += 1
        if len(samples) < 5:
            samples.append({"case": index, "callback": case["callback"], "diff": diffs})

report = {
    "source_sha256": sha,
    "probe": PROBE,
    "live_queries":args.live_queries,
    "checks": {"callbacks": {
        "cases": len(cases), "mismatches": mismatch, "samples": samples,
        "callbacks": sorted({case["callback"] for case in cases}),
    }},
    "original_functions": [
        "45B590 null, 45B5A0 org0, 45B5B0 org1, 45BDF0 plyr callback bodies",
        "45B3A0 step, 45B400 waypoint, 45C6A0 marker query, 45C810/45C880 selectors (executed)",
        "477670/477860 and the 477770 integer paths (executed); 477770 FPU path answered",
        "foreign calls hooked in order with deterministic answers",
    ],
    "total_mismatches": mismatch,
}
text = json.dumps(report, indent=2)
print(text)
if args.report:
    args.report.write_text(text + "\n")

raise SystemExit(bool(mismatch))
