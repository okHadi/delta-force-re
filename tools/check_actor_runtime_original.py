#!/usr/bin/env python3
"""Check the rebuilt actor runtime stage against original Df.exe instructions.

Sections:
  create  original 0x45D520  definition -> 0x114 actor record (0x44A0C0 and
                             0x42CCE0 hooked with caller handles, never run)
  bind    original 0x43F610  marker pose copy, rotating counter 0x4F820C
  spawn   original 0x440640  17 placement slots -> actors + person names
  init    original 0x4829F0  local player actor scan and writes
          (0x484970 and 0x483DD0 hooked, weapon table work stays external)

The rebuilt stage is driven through tools/actor_runtime_probe; every case
runs the same inputs through both sides and compares full record images.
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
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP

SOURCE_SHA256 = "abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff"
RETURN = 0x412345

CREATE = 0x45D520
BIND = 0x43F610
FINALIZE = 0x440640
INIT = 0x4829F0
PERSON_ALLOC = 0x44A0C0
CHARACTER_RESOLVE = 0x42CCE0
WEAPON_TABLE_INIT = 0x484970
WEAPON_SELECT = 0x483DD0

DEF_TABLE = 0x1000000
DEF_PTR = 0xC23810
DEF_COUNT = 0xC23814
ACTOR_TABLE = 0x7CCA80
ACTOR_COUNT = 0x8CFA18
PLACEMENTS = 0x6E9A90
MARKERS = 0x8ACE80
MARKER_COUNT = 0x8CF900
MODE = 0x8CF97C
NIGHT_FLAG = 0x8CF982
SCALE = 0x67F549
GAME_TYPE = 0x67F53C
BIND_COUNTER = 0x4F820C
FINALIZE_ENABLED = 0x67F528
PLAYER_ID = 0x50133C
WEAPON_SLOT_INDEX = 0xC239B8
WEAPON_SLOT_MAP = 0xC239C0
PLAYER_NAME = 0x67F420
PLAYER_FIELD_98 = 0x67F430
PLAYER_PTR = 0xC23820
PLAYER_PTR_2 = 0xC23A38
COUNT_6001 = 0x8CF9F4
COUNT_6002 = 0x8CFA00
COUNT_6003 = 0x8CF9F8
COUNT_6004 = 0x8CF9FC
PERSON_TABLE = 0x2000000
PERSON_STRIDE = 0x100
STACK = 0x3000000
SP0 = STACK + 0x8000

ACTOR_STRIDE = 0x114
PLACEMENT_STRIDE = 0x9B84

CLASS_IDS = [0x138B, 0x138C, 0x1392, 0x1393, 0x13B4, 0x13B5, 0x13B6, 0x13B7]
MARKER_IDS = [0x1771, 0x1772, 0x1773, 0x1774]

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("game", type=Path)
parser.add_argument("probe", type=Path)
parser.add_argument("--report", type=Path)
parser.add_argument("--quick", action="store_true", help="fewer random cases")
args = parser.parse_args()

raw = (args.game / "Df.exe").read_bytes()
sha = hashlib.sha256(raw).hexdigest()
if sha != SOURCE_SHA256:
    raise SystemExit("Unsupported original executable hash")
pe = pefile.PE(data=raw)
u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
for at, size in [(0x400000, 0xA00000), (0x1000000, 0x100000), (0x2000000, 0x100000),
                 (0x3000000, 0x20000), (0x4000000, 0x1000)]:
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


def word(at):
    return struct.unpack("<I", u.mem_read(at, 4))[0]


def emu_call(func, count=4000000):
    u.reg_write(UC_X86_REG_ESP, SP0)
    u.emu_start(func, RETURN, count=count)
    eip = u.reg_read(UC_X86_REG_EIP)
    if eip != RETURN:
        raise SystemExit(f"original emulation incomplete: EIP=0x{eip:x}, expected 0x{RETURN:x}")
    return u.reg_read(UC_X86_REG_EAX)


def push_args(values):
    u.mem_write(SP0, struct.pack("<%dI" % (len(values) + 1), RETURN, *values))


# --- hooks -------------------------------------------------------------

person_queue = []
character_queue = []
weapon_select_case = [None]  # (slot index, map value) written by the 483DD0 hook


def hook_person(machine, at, size, user):
    sp = u.reg_read(UC_X86_REG_ESP)
    u.reg_write(UC_X86_REG_EAX, person_queue.pop(0))
    u.reg_write(UC_X86_REG_ESP, sp + 4)
    u.reg_write(UC_X86_REG_EIP, word(sp))


def hook_character(machine, at, size, user):
    sp = u.reg_read(UC_X86_REG_ESP)
    u.reg_write(UC_X86_REG_EAX, character_queue.pop(0))
    u.reg_write(UC_X86_REG_ESP, sp + 4)
    u.reg_write(UC_X86_REG_EIP, word(sp))


def hook_pop(machine, at, size, user):
    # Emulated return: pop only the return address; callers clean cdecl args.
    sp = u.reg_read(UC_X86_REG_ESP)
    u.reg_write(UC_X86_REG_EAX, 0)
    u.reg_write(UC_X86_REG_ESP, sp + 4)
    u.reg_write(UC_X86_REG_EIP, word(sp))


def hook_weapon_select(machine, at, size, user):
    # 0x483DD0 selects a weapon slot after the player block was zeroed and
    # 0x484970 filled the table. Stand in for both: seed the case's slot map.
    if weapon_select_case[0] is not None:
        index, value = weapon_select_case[0]
        put(WEAPON_SLOT_INDEX, index)
        put(WEAPON_SLOT_MAP + index * 4, value)
    sp = u.reg_read(UC_X86_REG_ESP)
    u.reg_write(UC_X86_REG_EAX, 0)
    u.reg_write(UC_X86_REG_ESP, sp + 4)
    u.reg_write(UC_X86_REG_EIP, word(sp))


u.hook_add(unicorn.UC_HOOK_CODE, hook_person, begin=PERSON_ALLOC, end=PERSON_ALLOC)
u.hook_add(unicorn.UC_HOOK_CODE, hook_character, begin=CHARACTER_RESOLVE, end=CHARACTER_RESOLVE)
u.hook_add(unicorn.UC_HOOK_CODE, hook_pop, begin=WEAPON_TABLE_INIT, end=WEAPON_TABLE_INIT)
u.hook_add(unicorn.UC_HOOK_CODE, hook_weapon_select, begin=WEAPON_SELECT, end=WEAPON_SELECT)


# --- original state ----------------------------------------------------

def reset_original():
    person_queue.clear()
    character_queue.clear()
    u.mem_write(ACTOR_TABLE, bytes(ACTOR_STRIDE * 256))
    u.mem_write(PLACEMENTS, bytes(PLACEMENT_STRIDE * 17))
    u.mem_write(MARKERS, bytes(ACTOR_STRIDE * 32))
    u.mem_write(DEF_TABLE, bytes(0x13C * 16))
    u.mem_write(PERSON_TABLE, bytes(PERSON_STRIDE * 24))
    put(DEF_PTR, DEF_TABLE)
    put(DEF_COUNT, 0)
    put(ACTOR_COUNT, 0)
    put(MARKER_COUNT, 0)
    put(MODE, 0)
    put(NIGHT_FLAG, 0)
    put(SCALE, 0)
    put(GAME_TYPE, 0)
    put(BIND_COUNTER, 0)
    put(FINALIZE_ENABLED, 0)
    put(COUNT_6001, 0)
    put(COUNT_6002, 0)
    put(COUNT_6003, 0)
    put(COUNT_6004, 0)
    put(PLAYER_PTR, 0)
    put(PLAYER_PTR_2, 0)


def setup_defs(defs):
    put(DEF_PTR, DEF_TABLE)
    put(DEF_COUNT, len(defs))
    for i, (item_id, graphic, character, ai, move, hp) in enumerate(defs):
        base = DEF_TABLE + i * 0x13C
        put(base + 0x20, item_id)
        put(base + 0x6C, graphic)
        put(base + 0x74, character)
        put(base + 0x84, ai)
        put(base + 0x90, move)
        put(base + 0x94, hp)


def setup_markers(marker_defs):
    put(MARKER_COUNT, len(marker_defs))
    for i, (definition_index, pose) in enumerate(marker_defs):
        base = MARKERS + i * ACTOR_STRIDE
        put(base + 0x00, definition_index)
        for j, value in enumerate(pose):
            put(base + 0x24 + j * 4, value)


def setup_placements(placements):
    for index, placement in enumerate(placements):
        base = PLACEMENTS + index * PLACEMENT_STRIDE
        put(base + 0x00, placement["object_id"])
        u.mem_write(base + 0x0C, placement["name"] + b"\0")
        put16(base + 0x9C, placement["field_9c"])
        put(base + 0xA0, placement["argument1"])
        put(base + 0xA4, placement["argument2"])
        put(base + 0x1C4, placement["team_code"])
        u.mem_write(base + 0x18C, bytes([1 if placement["active"] else 0]))


def normalize_actor(data, definitions):
    out = bytearray(data)
    pointer = struct.unpack_from("<I", out, 0x04)[0]
    index = ((pointer - DEF_TABLE) // 0x13C
             if DEF_TABLE <= pointer < DEF_TABLE + len(definitions) * 0x13C else 0)
    struct.pack_into("<I", out, 0x04, index)
    return bytes(out)


def image_line(index, count, definitions):
    return "%x %x %s" % (index, count, normalize_actor(read_actor(index), definitions).hex())


def read_actor(index):
    return bytes(u.mem_read(ACTOR_TABLE + index * ACTOR_STRIDE, ACTOR_STRIDE))


def hexbytes(data):
    return data.hex() if data else "-"


# --- emulated operations ----------------------------------------------

def emu_create_once(argument1, argument2, person, character):
    person_queue.append(person)
    character_queue.append(character)
    push_args([argument1, argument2, 0, 0])
    slot = emu_call(CREATE, count=200000)
    if slot == 0:
        return None, word(ACTOR_COUNT)
    return (slot - ACTOR_TABLE) // ACTOR_STRIDE, word(ACTOR_COUNT)


def emu_create_case(mode, night, scale, argument1, argument2, person, character, defs):
    reset_original()
    put(MODE, mode)
    put(NIGHT_FLAG, 0x10 if night else 0)
    put(SCALE, 1 if scale else 0)
    setup_defs(defs)
    return emu_create_once(argument1, argument2, person, character)


def emu_spawn(placements, handles, bind_config, marker_defs, defs):
    put(FINALIZE_ENABLED, 1)
    put(GAME_TYPE, bind_config[0])
    put(COUNT_6001, bind_config[1])
    put(COUNT_6002, bind_config[2])
    put(COUNT_6003, bind_config[3])
    put(COUNT_6004, bind_config[4])
    put(BIND_COUNTER, bind_config[5])
    setup_defs(defs)
    setup_markers(marker_defs)
    setup_placements(placements)
    for handle in handles:
        person_queue.append(handle[0])
        character_queue.append(handle[1])
    push_args([])
    emu_call(FINALIZE, count=4000000)
    results = []
    for index, placement in enumerate(placements):
        if not placement["active"]:
            continue
        base = PLACEMENTS + index * PLACEMENT_STRIDE
        actor_pointer = word(base + 0x08)
        if actor_pointer == 0:
            results.append((index, None, 0, b""))
            continue
        actor_index = (actor_pointer - ACTOR_TABLE) // ACTOR_STRIDE
        person_pointer = word(actor_pointer + 0x0C)
        name = bytes(u.mem_read(person_pointer + 4, 64)).split(b"\0")[0]
        results.append((index, actor_index, person_pointer, name))
    return results, word(ACTOR_COUNT), word(BIND_COUNTER)


def emu_bind(game_type, counts, counter, marker_defs, placement_index, field_70):
    put(GAME_TYPE, game_type)
    put(COUNT_6001, counts[0])
    put(COUNT_6002, counts[1])
    put(COUNT_6003, counts[2])
    put(COUNT_6004, counts[3])
    put(BIND_COUNTER, counter)
    setup_markers(marker_defs)
    put16(ACTOR_TABLE + 0x70, field_70)
    placement = PLACEMENTS + placement_index * PLACEMENT_STRIDE
    put(placement + 0x08, ACTOR_TABLE)
    push_args([placement])
    emu_call(BIND, count=200000)
    return word(BIND_COUNTER)


def emu_init(player_id, weapon_slot_index, weapon_slot, field_98, name):
    put(PLAYER_ID, player_id)
    weapon_select_case[0] = (weapon_slot_index, weapon_slot)
    u.mem_write(PLAYER_NAME, name + b"\0")
    put(PLAYER_FIELD_98, field_98)
    push_args([])
    found = emu_call(INIT, count=4000000)
    weapon_select_case[0] = None
    if found == 0:
        return 0, None, 0, 0, b""
    pointer = word(PLAYER_PTR)
    if word(PLAYER_PTR_2) != pointer:
        raise SystemExit("original player pointer outputs differ")
    actor_index = (pointer - ACTOR_TABLE) // ACTOR_STRIDE
    person_pointer = word(pointer + 0x0C)
    copied = bytes(u.mem_read(person_pointer + 4, 64)).split(b"\0")[0]
    return 1, actor_index, word(pointer + 0x94), word(pointer + 0x98), copied


# --- case builders -----------------------------------------------------

CASES = []  # (probe lines, expected probe output lines)


def add_case(lines, expected):
    CASES.append((lines, expected))


def probe_config(mode, night, scale, game_type, counts, counter):
    return "config %x %x %x %x %x %x %x %x %x" % (
        mode, 1 if night else 0, 1 if scale else 0, game_type,
        counts[0], counts[1], counts[2], counts[3], counter)


def probe_defs(defs):
    return ["def %x %x %x %x %x %x %x" % (i, *definition) for i, definition in enumerate(defs)]


def probe_markers(markers):
    return ["marker %x %x %x %x %x %x %x %x" % (i, definition_index, *pose)
            for i, (definition_index, pose) in enumerate(markers)]


def probe_placements(placements):
    return ["place %x %x %x %x %x %x %x %s" % (
        index, placement["object_id"], 1 if placement["active"] else 0,
        placement["field_9c"], placement["argument1"], placement["argument2"],
        placement["team_code"], hexbytes(placement["name"]))
        for index, placement in enumerate(placements)]


def random_defs(rng, extra=2):
    ids = CLASS_IDS + MARKER_IDS + [rng.randrange(1, 0x2000) for _ in range(extra)]
    rng.shuffle(ids)
    defs = [(rng.randrange(1, 0x2000), rng.randrange(0x10000000), rng.randrange(0x10000000),
             0x2000000, 0x3000000, rng.randrange(0, 0x10000)) for _ in range(len(ids) + 1)]
    defs[0] = (0xFFFF, 0, 0, 0, 0, 0)  # keep index 0 occupied like the shipped table
    positions = list(range(1, len(defs)))
    rng.shuffle(positions)
    for ident, position in zip(ids, positions):
        defs[position] = (ident, rng.randrange(0x10000000), rng.randrange(0x10000000),
                          0x2000000, 0x3000000, rng.randrange(0, 0x10000))
    return defs


def run_class_cases(rng):
    defs = random_defs(rng)
    total = 0
    for mode in (0, 1, 2, 3, 7):
        for night in (False, True):
            for side in (0, 1, 2):
                index, count = emu_create_case(mode, night, False, 0, side, 0x100, 0x200, defs)
                lines = (["reset", probe_config(mode, night, False, 0, (0, 0, 0, 0), 0)]
                         + probe_defs(defs)
                         + ["create 0 %x 100 200" % side])
                add_case(lines, [image_line(index, count, defs)])
                total += 1
    return total


def run_create_cases(rng):
    total = 0
    for _ in range(20 if args.quick else 200):
        defs = random_defs(rng)
        mode = rng.choice([0, 1, 2, 3, 5])
        night = rng.random() < 0.5
        scale = rng.random() < 0.5
        side = rng.choice([0, 1, 2, 0xFFFFFFFF])
        argument1 = rng.randrange(0, 0x10000)
        person = 0x2000000 + rng.randrange(0, 16) * PERSON_STRIDE
        character = rng.randrange(0x10000000, 0x20000000)
        index, count = emu_create_case(mode, night, scale, argument1, side, person, character, defs)
        lines = (["reset", probe_config(mode, night, scale, 0, (0, 0, 0, 0), 0)]
                 + probe_defs(defs)
                 + ["create %x %x %x %x" % (argument1, side, person, character)])
        add_case(lines, [image_line(index, count, defs)])
        total += 1

    # Repeated creates: allocation order and the high-water count.
    defs = random_defs(rng)
    reset_original()
    setup_defs(defs)
    lines = ["reset", probe_config(0, False, False, 0, (0, 0, 0, 0), 0)] + probe_defs(defs)
    expected = []
    for step in range(6):
        person = 0x2000000 + step * PERSON_STRIDE
        character = 0x11000000 + step
        index, count = emu_create_once(step, step % 2, person, character)
        lines.append("create %x %x %x %x" % (step, step % 2, person, character))
        expected.append(image_line(index, count, defs))
    add_case(lines, expected)
    total += 1

    # All 256 slots taken, then the factory fails like the original.
    defs = random_defs(rng)
    reset_original()
    setup_defs(defs)
    lines = ["reset", probe_config(0, False, False, 0, (0, 0, 0, 0), 0)] + probe_defs(defs)
    expected = []
    for step in range(257):
        person = 0x2000000 + (step % 16) * PERSON_STRIDE
        character = 0x11000000 + step
        index, count = emu_create_once(step, 0, person, character)
        lines.append("create %x 0 %x %x" % (step, person, character))
        expected.append("- %x -" % count if index is None else image_line(index, count, defs))
    add_case(lines, expected)
    total += 1
    return total


def run_bind_cases(rng):
    total = 0
    for _ in range(8 if args.quick else 80):
        defs = random_defs(rng)
        game_type = rng.randrange(0, 7)
        counts = tuple(rng.randrange(0, 4) for _ in range(4))
        counter = rng.choice([0, 1, 5, 9, 0xFFFFFFFE])
        marker_defs = [(rng.choice(MARKER_IDS) if rng.random() < 0.8 else rng.randrange(0x2000),
                        tuple(rng.randrange(0x10000) for _ in range(6)))
                       for _ in range(rng.randrange(1, 12))]
        field_70 = rng.choice([0, 1])
        placement = {"object_id": 1, "name": b"", "field_9c": field_70, "argument1": 0,
                     "argument2": 0, "team_code": 0, "active": True}
        placements = [placement] + [dict(object_id=0, name=b"", field_9c=0, argument1=0,
                                         argument2=0, team_code=0, active=False)
                                    for _ in range(16)]
        reset_original()
        spawn_results, count, _ = emu_spawn(placements, [(0x2000000, 0x11000000)],
                                            (game_type, *counts, counter), marker_defs, defs)
        index, actor_index, person, name = spawn_results[0]
        lines = (["reset", probe_config(0, False, False, game_type, counts, counter)]
                 + probe_defs(defs) + probe_markers(marker_defs) + probe_placements(placements)
                 + ["spawn " + " ".join("2000000 11000000" if i == 0 else "0 0"
                                        for i in range(17))])
        expected = ["%x %x %x %s %s %x" % (index, actor_index, person, hexbytes(name),
                                           normalize_actor(read_actor(actor_index), defs).hex(),
                                           word(BIND_COUNTER))]
        expected.append("count %x" % count)
        for _ in range(3):
            emu_bind(game_type, counts, word(BIND_COUNTER), marker_defs, index, field_70)
            expected.append("%s %x" % (normalize_actor(read_actor(actor_index), defs).hex(),
                                       word(BIND_COUNTER)))
            lines.append("bind %x %x" % (index, actor_index))
        add_case(lines, expected)
        total += 1
    return total


def run_spawn_cases(rng):
    total = 0
    for _ in range(8 if args.quick else 80):
        defs = random_defs(rng)
        game_type = rng.randrange(0, 7)
        counts = tuple(rng.randrange(0, 4) for _ in range(4))
        counter = rng.choice([0, 2, 7])
        marker_defs = [(rng.choice(MARKER_IDS), tuple(rng.randrange(0x10000) for _ in range(6)))
                       for _ in range(rng.randrange(1, 16))]
        placements = []
        for _ in range(17):
            active = rng.random() < 0.4
            name = b""
            if active and rng.random() < 0.85:
                name = bytes(rng.randrange(32, 127) for _ in range(rng.randrange(1, 12)))
            placements.append({"object_id": rng.randrange(0, 0x10000),
                               "name": name, "field_9c": rng.choice([0, 1, 0x10]),
                               "argument1": rng.randrange(0, 0x10000),
                               "argument2": rng.choice([0, 1]), "team_code": rng.randrange(0, 4),
                               "active": active})
        active_order = [i for i, placement in enumerate(placements) if placement["active"]]
        call_handles = [(0x2000000 + k * PERSON_STRIDE, 0x11000000 + k)
                        for k in range(len(active_order))]
        probe_handles = [(0, 0)] * 17
        for k, i in enumerate(active_order):
            probe_handles[i] = call_handles[k]
        reset_original()
        spawn_results, count, final_counter = emu_spawn(placements, call_handles,
                                                        (game_type, *counts, counter),
                                                        marker_defs, defs)
        lines = (["reset", probe_config(0, False, False, game_type, counts, counter)]
                 + probe_defs(defs) + probe_markers(marker_defs) + probe_placements(placements)
                 + ["spawn " + " ".join("%x %x" % handle for handle in probe_handles)])
        expected = []
        for index, actor_index, person, name in spawn_results:
            if actor_index is None:
                expected.append("%x - %x %s - %x" % (index, person, hexbytes(name), final_counter))
                continue
            expected.append("%x %x %x %s %s %x" % (index, actor_index, person, hexbytes(name),
                                                   normalize_actor(read_actor(actor_index), defs).hex(),
                                                   final_counter))
        expected.append("count %x" % count)
        add_case(lines, expected)
        total += 1
    return total


def run_init_cases(rng):
    total = 0
    for _ in range(8 if args.quick else 60):
        defs = random_defs(rng)
        placement = {"object_id": rng.randrange(1, 0x10000), "name": b"slot",
                     "field_9c": rng.choice([0, 1]), "argument1": 0, "argument2": 0,
                     "team_code": 0, "active": True}
        placements = [placement] + [dict(object_id=0, name=b"", field_9c=0, argument1=0,
                                         argument2=0, team_code=0, active=False)
                                    for _ in range(16)]
        handles = [(0x2000000 + i * PERSON_STRIDE, 0x11000000 + i) for i in range(17)]
        marker_defs = [(0x1771, (0,) * 6)]
        reset_original()
        spawn_results, count, _ = emu_spawn(placements, handles, (0, 0, 0, 0, 0, 0),
                                            marker_defs, defs)
        index, actor_index, person, name = spawn_results[0]
        spawn_image = normalize_actor(read_actor(actor_index), defs).hex()
        player_id = placement["object_id"] if rng.random() < 0.8 else 0x9999
        weapon_slot_index = rng.randrange(0, 10)
        weapon_slot = rng.randrange(0, 0x10000)
        field_98 = rng.randrange(0, 0x10000)
        player_name = bytes(rng.randrange(32, 127) for _ in range(rng.randrange(0, 10)))
        found, index2, field_94, field_98_value, copied = emu_init(player_id, weapon_slot_index,
                                                                   weapon_slot, field_98,
                                                                   player_name)
        lines = (["reset", probe_config(0, False, False, 0, (0, 0, 0, 0), 0)]
                 + probe_defs(defs) + probe_markers(marker_defs) + probe_placements(placements)
                 + ["spawn " + " ".join("%x %x" % handle for handle in handles)]
                 + ["init %x %x %x %s" % (player_id, weapon_slot, field_98, hexbytes(player_name))])
        expected = ["%x %x %x %s %s %x" % (index, actor_index, person, hexbytes(name),
                                           spawn_image, 0),
                    "count %x" % count]
        if found:
            expected.append("1 %x %x %x %s" % (index2, field_94, field_98_value, hexbytes(copied)))
        else:
            expected.append("0 - - - -")
        add_case(lines, expected)
        total += 1
    return total


rng = random.Random(0xAC702026)
classes = run_class_cases(rng)
created = run_create_cases(rng)
bound = run_bind_cases(rng)
spawned = run_spawn_cases(rng)
inited = run_init_cases(rng)

input_text = "".join("\n".join(lines) + "\n" for lines, _ in CASES)
expected_lines = [line for _, expected in CASES for line in expected]
done = subprocess.run([PROBE], input=input_text.encode(), capture_output=True)
if done.returncode != 0:
    raise SystemExit("probe failed: " + done.stderr.decode(errors="replace"))
actual_lines = [line for line in done.stdout.decode().splitlines() if line]
if len(actual_lines) != len(expected_lines):
    raise SystemExit(f"probe returned {len(actual_lines)} lines for {len(expected_lines)} expected")

mismatches = []
samples = []
position = 0
for case_index, (_, expected) in enumerate(CASES):
    for expected_line in expected:
        if actual_lines[position] != expected_line:
            first = next((i for i, (a, b) in enumerate(zip(expected_line, actual_lines[position]))
                          if a != b), min(len(expected_line), len(actual_lines[position])))
            mismatches.append({"case": case_index, "line": position, "at": first})
            if len(samples) < 4:
                samples.append({"case": case_index, "at": first,
                                "expected": expected_line[max(0, first - 40):first + 60],
                                "actual": actual_lines[position][max(0, first - 40):first + 60]})
        position += 1

report = {
    "source_sha256": sha,
    "probe": PROBE,
    "checks": {
        "class_selector_0x45D520": {"cases": classes},
        "create_0x45D520": {"cases": created},
        "bind_0x43F610": {"cases": bound},
        "finalize_0x440640": {"cases": spawned},
        "definition_init_0x4829F0": {"cases": inited},
        "total_cases": len(CASES),
    },
    "total_mismatches": len(mismatches) + (0 if position == len(expected_lines) else 1),
    "samples": samples,
    "hooks": [
        "44A0C0 person allocator returns caller people handles",
        "42CCE0 character resolve returns caller handles",
        "484970 and 483DD0 weapon table work skipped (external modules)",
    ],
    "normalized_fields": ["actor +0x04 definition pointer compared as table index"],
}
text = json.dumps(report, indent=2)
print(text)
if args.report:
    args.report.write_text(text + "\n")
sys.exit(1 if report["total_mismatches"] else 0)
