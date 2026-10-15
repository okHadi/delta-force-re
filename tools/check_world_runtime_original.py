#!/usr/bin/env python3
"""Check the joined mission initialization against original Df.exe code.

Sections:
  assets_62        native audit of all 62 stock missions (counts, acceptance,
                   people, resource handles, removals, heights, placements)
  replay_402EC0    original 0x449710 + 0x402EC0 with real mission data, hooks
                   for the resource loads and releases that stay with root
  replay_finalize  original 0x403C93 height loops, 0x444C50 slot 0, 0x440640
                   pass and 0x4829F0 local player, real markers and counts
  names_rtxt       People Names lookups from the campaign RTXT against the
                   native person records

Native handle ids are compared through their documented maps, never guessed.
"""
import argparse
import hashlib
import json
import struct
import subprocess
import sys
from pathlib import Path

import pefile
import unicorn
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_EBP,
                               UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW)

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mission import Resolver, terrain_name, map_names  # noqa: E402

SOURCE_SHA256 = "abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff"
RETURN = 0x412345

CONVERT = 0x449710
RESOURCE_PASS = 0x402EC0
HEIGHT_LOOP = (0x403C93, 0x403DD4)
FINALIZE = 0x440640
LOCAL_PLAYER = 0x4829F0
PERSON_ALLOC = 0x44A0C0
CHARACTER_RESOLVE = 0x42CCE0
ITEM_LOADER = 0x45D6F0
WEAPON_FIXED = 0x483CD0
EFFECT_LOAD = 0x45DD40
RELEASE_A = 0x4604C0
RELEASE_B = 0x42CD30
RELEASE_C = 0x43F070
RTXT_LOOKUP = 0x49A092
WEAPON_TABLE = 0x484970
WEAPON_SELECT = 0x483DD0
PROBE_WEAPON_SLOT = 0x1234
PROBE_FACE_ID = 0x66

DEF_TABLE = 0x1000000
DEF_PTR = 0xC23810
DEF_COUNT = 0xC23814
HEADER_COPY = 0x8CF908
GROUP_TABLES = ((0x7DDE80, 2048), (0x867E80, 1024), (0x8ACE80, 512), (0x7CCA80, 256))
ACTOR_COUNT = (0x8CFA1C, 0x8CF904, 0x8CF900, 0x8CFA18)
GROUP_OBJECT_COUNT = (0x8CF9A0, 0x8CF9A4, 0x8CF9A8, 0x8CF9AC)
PLACEMENTS = 0x6E9A90
PLACEMENT_STRIDE = 0x9B84
PERSON_TABLE = 0x794680
PERSON_STRIDE = 0xB4
RECORD = 0x2001000
ACTOR_TABLE = 0x7CCA80
PLAYER_PTR = 0xC23820
PLAYER_ID = 0x50133C
WEAPON_SLOT_INDEX = 0xC239B8
WEAPON_SLOT_MAP = 0xC239C0
BIND_COUNTER = 0x4F820C
FINALIZE_ENABLED = 0x67F528
MULTIPLAYER = 0x67F520
MODE_FLAG = 0x8CF97C
SCALE_HEALTH = 0x67F549
GAME_TYPE = 0x67F53C
TERRAIN_PTR = 0xBD5284
TERRAIN_BUFFER = 0x3000000
STACK = 0x2008000
FPCW = 0x037F

HANDLE_BASES = {"model": 0x71000000, "character": 0x72000000, "animation": 0x73000000,
                "instance": 0x74000000, "person": 0x75000000, "actor": 0x76000000}

CLASS_PAIR_IDS = {(False, False): (5003, 5004), (True, False): (5010, 5011),
                  (False, True): (5044, 5045), (True, True): (5046, 5047)}
SQUAD_IDS = {5002, 5038, 5048, 5049}

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("game", type=Path)
parser.add_argument("probe", type=Path)
parser.add_argument("--report", type=Path)
parser.add_argument("--limit", type=int, default=0, help="only the first N missions")
args = parser.parse_args()

raw = (args.game / "Df.exe").read_bytes()
sha = hashlib.sha256(raw).hexdigest()
if sha != SOURCE_SHA256:
    raise SystemExit("Unsupported original executable hash")
pe = pefile.PE(data=raw)
u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
for at, size in [(0x400000, 0xA00000), (0x1000000, 0x100000), (0x2000000, 0x100000),
                 (0x2100000, 0x40000), (0x3000000, 0x200000), (0x4000000, 0x1000)]:
    u.mem_map(at, size)
for section in pe.sections:
    if section.PointerToRawData and not section.Characteristics & 0x80:
        u.mem_write(0x400000 + section.VirtualAddress,
                    raw[section.PointerToRawData:section.PointerToRawData + section.SizeOfRawData])
PROBE = str(args.probe.resolve())
resolver = Resolver(args.game)


def put(at, value):
    u.mem_write(at, struct.pack("<I", value & 0xFFFFFFFF))


def put16(at, value):
    u.mem_write(at, struct.pack("<H", value & 0xFFFF))


def word(at):
    return struct.unpack("<I", u.mem_read(at, 4))[0]


def le32(data, at):
    return struct.unpack_from("<I", data, at)[0]


import os
TRACE = []
if os.environ.get("DFRE_TRACE"):
    def _trace(machine, address, size, user):
        TRACE.append(address)
        if len(TRACE) > 40:
            TRACE.pop(0)
    u.hook_add(unicorn.UC_HOOK_CODE, _trace)


def emu_run(begin, until, count=8000000):
    # [STACK] is the return address; explicit frames rewrite it identically.
    u.mem_write(STACK, struct.pack("<I", RETURN))
    u.reg_write(UC_X86_REG_ESP, STACK)
    try:
        u.emu_start(begin, until, count=count)
    except unicorn.UcError as error:
        eip = u.reg_read(UC_X86_REG_EIP)
        raise SystemExit(f"emulation fault at 0x{begin:x}: {error} EIP=0x{eip:x} "
                         f"trace=" + " ".join(hex(a) for a in TRACE[-14:]))
    eip = u.reg_read(UC_X86_REG_EIP)
    if eip != until:
        raise SystemExit(f"original emulation incomplete: EIP=0x{eip:x}, expected 0x{until:x}")


def probe_run(arguments):
    done = subprocess.run([PROBE] + [str(a) for a in arguments], capture_output=True)
    if done.returncode != 0:
        raise SystemExit("probe failed: " + done.stderr.decode(errors="replace"))
    return [line for line in done.stdout.decode().splitlines() if line]


# --- hooks -----------------------------------------------------------------

person_calls = []
character_calls = []
loader_calls = [0]


def finish_hook(sp, value, arguments):
    u.reg_write(UC_X86_REG_EAX, value)
    u.reg_write(UC_X86_REG_ESP, sp + 4 + 4 * arguments)
    u.reg_write(UC_X86_REG_EIP, word(sp))


def hook_character(machine, at, size, user):
    # 42CCE0 resolves the CHR registry pointer; stubbed with the probe's
    # instance ids in call order. The caller cleans the argument.
    sp = u.reg_read(UC_X86_REG_ESP)
    handle = HANDLE_BASES["instance"] + len(character_calls)
    character_calls.append(handle)
    finish_hook(sp, handle, 0)


def hook_loader(machine, at, size, user):
    sp = u.reg_read(UC_X86_REG_ESP)
    loader_calls[0] += 1
    finish_hook(sp, 0, 0)


def hook_noop(machine, at, size, user):
    sp = u.reg_read(UC_X86_REG_ESP)
    finish_hook(sp, 0, 0)


def hook_weapon_select(machine, at, size, user):
    # 0x483DD0 selects a slot after the 0x4829F0 memset; the checker seeds the
    # case's weapon slot value, mirroring the probe --weapon-slot option.
    sp = u.reg_read(UC_X86_REG_ESP)
    put(WEAPON_SLOT_INDEX, 3)
    put(WEAPON_SLOT_MAP + 12, PROBE_WEAPON_SLOT)
    finish_hook(sp, 0, 0)


def hook_rtxt(machine, at, size, user):
    sp = u.reg_read(UC_X86_REG_ESP)
    finish_hook(sp, 0, 0)


def hook_sprintf(machine, at, size, user):
    # 0x493A80 formats "NAMEID%03i" into a caller buffer; the lookup is stubbed
    # so only the frame has to behave: caller cleans the three arguments.
    sp = u.reg_read(UC_X86_REG_ESP)
    dest = word(sp + 4)
    value = word(sp + 12)
    signed = value if value < 0x80000000 else value - 0x100000000
    u.mem_write(dest, f"NAMEID{signed:03d}".encode() + b"\0")
    finish_hook(sp, dest, 0)


u.hook_add(unicorn.UC_HOOK_CODE, hook_character, begin=CHARACTER_RESOLVE, end=CHARACTER_RESOLVE)
u.hook_add(unicorn.UC_HOOK_CODE, hook_loader, begin=ITEM_LOADER, end=ITEM_LOADER)
u.hook_add(unicorn.UC_HOOK_CODE, hook_noop, begin=WEAPON_FIXED, end=WEAPON_FIXED)
u.hook_add(unicorn.UC_HOOK_CODE, hook_noop, begin=EFFECT_LOAD, end=EFFECT_LOAD)
u.hook_add(unicorn.UC_HOOK_CODE, hook_noop, begin=RELEASE_A, end=RELEASE_A)
u.hook_add(unicorn.UC_HOOK_CODE, hook_noop, begin=RELEASE_B, end=RELEASE_B)
u.hook_add(unicorn.UC_HOOK_CODE, hook_noop, begin=RELEASE_C, end=RELEASE_C)
u.hook_add(unicorn.UC_HOOK_CODE, hook_rtxt, begin=RTXT_LOOKUP, end=RTXT_LOOKUP)
u.hook_add(unicorn.UC_HOOK_CODE, hook_sprintf, begin=0x493A80, end=0x493A80)
u.hook_add(unicorn.UC_HOOK_CODE, hook_noop, begin=WEAPON_TABLE, end=WEAPON_TABLE)
u.hook_add(unicorn.UC_HOOK_CODE, hook_weapon_select, begin=WEAPON_SELECT, end=WEAPON_SELECT)


# --- independent data helpers ---------------------------------------------

def read_member(name):
    return resolver.read(*resolver.find(name))[0]


def decode_game_script(data):
    """src/scripts.cpp: reversed bytes XOR a 32-bit key stream."""
    out = bytearray(reversed(data))
    key = 0x04960552
    for index in range(len(out)):
        key = (key + ((key << 11) | (key >> 21))) & 0xFFFFFFFF
        key = (((key << 4) | (key >> 28)) ^ 1) & 0xFFFFFFFF
        out[index] ^= key & 0xFF
    return bytes(out)


class Items:
    """Minimal ITEMS.DEF parse for independent checks (ids, type, hp, flags)."""

    def __init__(self, text):
        self.records = []
        current = None
        for line in text.splitlines():
            words = line.replace('\t', ' ').split(' ')
            words = [w for w in words if w]
            if not words or words[0] == '/':
                continue
            head = words[0].lower()
            if head == 'begin' and len(words) >= 2:
                current = {"name": words[1].strip('"'), "id": 0, "type": 0, "hp": 0, "flags": 0,
                           "graphic": "", "husk": "", "chr": "", "anim": ""}
            elif head == 'end':
                if current is not None:
                    current["id"] = (current["id"] - 100000) & 0xFFFFFFFF
                    self.records.append(current)
                    current = None
            elif current is not None:
                value = words[1] if len(words) > 1 else ""
                if head == 'id':
                    current["id"] = int(value)
                elif head == 'type':
                    current["type"] = {"person": 3, "vehicle": 1, "building": 5, "decoration": 2,
                                       "marker": 4, "object": 6, "effect": 8}.get(value.lower(), 0)
                elif head == 'hp':
                    current["hp"] = int(value)
                elif head == 'graphic':
                    current["graphic"] = value
                elif head == 'husk':
                    current["husk"] = value
                elif head == 'chr_file':
                    current["chr"] = value
                elif head == 'anim_def':
                    current["anim"] = value
                elif head == 'attrib:':
                    for flag in words[1:]:
                        current["flags"] |= {"good": 1, "evil": 2, "aidata": 4, "fo": 8,
                                             "movecb": 0x10, "powerup": 0x20, "flesh": 0x40,
                                             "noak": 0x80}.get(flag.lower(), 0)
        self.by_id = {}
        for record in self.records:
            self.by_id.setdefault(record["id"], record)

    def find(self, ident):
        return self.by_id.get(ident, self.records[0] if self.records else
                              {"id": 0, "type": 0, "hp": 0, "flags": 0, "graphic": "", "husk": "",
                               "chr": "", "anim": ""})


def accepted(flags, rank):
    flags &= 0xFF
    if flags & 0x10:
        return False  # single player rejects multiplayer-only records
    if flags & 0x80:
        return False
    return True


def mission_theme(bms):
    return le32(bms, 0x74)


def mission_night(bms):
    return bool(bms[0x7A] & 0x10)


class Rtxt:
    """Menu RtxtTable parse (src/menu.cpp) for People Names."""

    def __init__(self, data):
        if data[:4] != b"RTXT":
            raise SystemExit("Expected RTXT table")
        metadata, count = le32(data, 4), le32(data, 12)
        text_base = 16 + count * 16
        groups = le32(data, metadata)
        group_name = metadata + 4 + groups * 8
        self.groups = {}
        index = 0
        for group in range(groups):
            end = data.index(b"\0", group_name)
            key = data[group_name:end].decode("latin1")
            group_name = end + 1
            table = self.groups.setdefault(key.lower(), {})
            key_at = metadata + 4 + le32(data, metadata + 4 + group * 8)
            length = le32(data, metadata + 8 + group * 8)
            for _ in range(length):
                end = data.index(b"\0", key_at)
                entry_key = data[key_at:end].decode("latin1")
                key_at = end + 1
                offset = le32(data, 16 + index * 16)
                end = data.index(b"\0", text_base + offset)
                # std::unordered_map::emplace keeps the first duplicate key.
                table.setdefault(entry_key.lower(),
                                 data[text_base + offset:end].decode("latin1"))
                index += 1

    def get(self, group, key):
        return self.groups.get(group.lower(), {}).get(key.lower(), "")


def _objects(bms, counts):
    """Per-group raw 96-byte BMS object slices in file order."""
    groups = []
    at = 268
    for count in counts:
        records = [bms[at + i * 96:at + i * 96 + 96] for i in range(count)]
        groups.append(records)
        at += count * 96
    return groups


def height_image(bms):
    name = map_names(read_member(terrain_name(bms)))["elev_map"]
    from PIL import Image
    import io
    image = Image.open(io.BytesIO(read_member(name)))
    image.load()
    if image.size != (1024, 1024):
        raise SystemExit("Unexpected terrain height size")
    return image.transpose(Image.Transpose.FLIP_TOP_BOTTOM).tobytes()


def original_height(x, y):
    """Emulated 0x47DF40 with the terrain pointer installed."""
    sp = STACK
    u.mem_write(sp, struct.pack("<3I", RETURN, x & 0xFFFFFFFF, y & 0xFFFFFFFF))
    emu_run(0x47DF40, RETURN, count=20000)
    return u.reg_read(UC_X86_REG_EAX)


# --- native probe runs -----------------------------------------------------

audit_arguments = ["audit", args.game]
if args.limit > 0:
    audit_arguments += ["--limit", args.limit]
audit = [json.loads(line) for line in probe_run(audit_arguments)]
missions = [row["mission"] for row in audit]
expected_missions = sorted(entry.name for archive in resolver.archives for entry in archive.entries
                           if not entry.deleted and entry.name.lower().endswith(".bms"))
if args.limit > 0:
    if not set(missions) <= set(expected_missions):
        raise SystemExit("Audit returned an unknown mission")
elif sorted(missions) != expected_missions:
    raise SystemExit("Audit does not cover the stock BMS corpus")

NAME_OPTIONS = ["--weapon-slot", hex(PROBE_WEAPON_SLOT), "--face", hex(PROBE_FACE_ID)]
STATE_OPTIONS = ["--no-names"] + NAME_OPTIONS
state_rows = {}
for mission in missions:
    state_rows[mission] = json.loads(probe_run(["state", args.game, mission] + STATE_OPTIONS)[0])

report = {
    "source_sha256": sha,
    "probe": PROBE,
    "checks": {},
    "mismatches": [],
    "handle_maps": HANDLE_BASES,
}
mismatches = report["mismatches"]


def check(condition, detail):
    if not condition:
        mismatches.append(detail)


# --- section 1: native audit against independent BMS and ITEMS.DEF data ----

items_text = decode_game_script(read_member("items.def")).decode("latin1")
items = Items(items_text)
expected_defs = items.records
probe_defs = state_rows[missions[0]]["defs"]
check(len(probe_defs) == len(expected_defs),
      f"definition count {len(probe_defs)} != items.def {len(expected_defs)}")
for index, (got, want) in enumerate(zip(probe_defs, expected_defs)):
    if got["id"] != want["id"] or got["t"] != want["type"] or got["hp"] != want["hp"] or \
            got["f"] != want["flags"]:
        check(False, f"definition {index} differs: {got} vs {want}")
        break

mission_expectations = {}
heights = {}
for mission in missions:
    bms = read_member(mission)
    height_name = map_names(read_member(terrain_name(bms)))["elev_map"]
    if height_name not in heights:
        heights[height_name] = height_image(bms)
    u.mem_write(TERRAIN_BUFFER, heights[height_name])
    put(TERRAIN_PTR, TERRAIN_BUFFER)
    row = next(r for r in audit if r["mission"] == mission)
    state = state_rows[mission]
    counts = struct.unpack_from("<4I", bms, 152)
    check(list(counts) == state["counts"], f"{mission}: BMS counts differ")
    check(list(counts) == row["counts"], f"{mission}: audit counts differ")
    # acceptance filter, removal set and person gates
    active_expected = [0, 0, 0, 0]
    persons_expected = 0
    removals_expected = []
    removed_keys = set()
    removed_persons = 0
    for group in range(4):
        for index in range(counts[group]):
            raw = bms[268 + (sum(counts[:group]) + index) * 96:][:96]
            flags = le32(raw, 12)
            if not accepted(flags, 1):
                continue
            item = items.find(le32(raw, 0))
            # The default run keeps squad_pass off: only the 0x402E70 powerup
            # removal clears slots.
            removed = group == 0 and (item["flags"] & 0x20)
            if removed:
                removed_keys.add((group, index))
                if item["flags"] & 4:
                    removed_persons += 1
                removals_expected.append((0, index, item["id"]))
            else:
                active_expected[group] += 1
                if item["flags"] & 4:
                    persons_expected += 1
    check(sum(active_expected) == row["active"], f"{mission}: active count differs")
    # conversion + placement person: one extra person for the local player
    check(persons_expected + 1 == row["persons"],
          f"{mission}: person count {row['persons']} != {persons_expected + 1}")
    check([(r["g"], r["s"], r["id"]) for r in state["removals"]] == removals_expected,
          f"{mission}: removals differ")
    mission_expectations[mission] = {"removed": removed_keys,
                                     "active": active_expected,
                                     "persons": persons_expected}
    # height loops for every converted slot, rejected records included
    for group in (3, 0, 1, 2):
        for slot, actor in enumerate(state["groups"][group]):
            if actor["a"]:
                raw = struct.pack("<I", actor["o"])
                source = next((obj for obj in _objects(bms, counts)[group]
                               if le32(obj, 8) == actor["o"]), None)
                x = le32(source, 16) if source else actor["x"]
                y = le32(source, 20) if source else actor["y"]
                base = le32(source, 24) if source else actor["z"]
            else:
                x = y = base = 0
            z = (base + original_height(x, y) + (0x10000 if group == 3 else 0)) & 0xFFFFFFFF
            if actor["z"] != z:
                check(False, f"{mission}: group {group} slot {slot} height {actor['z']} != {z}")
                break
    # handles for the items this mission loads (soldier pair and groups 0/1/3)
    model_map = {model["n"]: model["h"] for model in state["models"]}
    covered = []
    pair = CLASS_PAIR_IDS[(mission_theme(bms) == 2, mission_night(bms))]
    covered.extend(pair)
    for group in (0, 1, 3):
        for raw_obj in _objects(bms, counts)[group]:
            if le32(raw_obj, 12) & 0x90:
                continue
            covered.append(le32(raw_obj, 0))
    for item_id in sorted(set(covered)):
        entry = next((e for e in state["defs"] if e["id"] == item_id), None)
        if entry is None:
            check(False, f"{mission}: covered item {item_id} missing from definitions")
            continue
        item = items.find(item_id)
        for field, name, suffix in (("m", item["graphic"], ".3di"), ("h", item["husk"], ".3di")):
            if not name:
                continue
            key = name.lower() if name.lower().endswith(suffix) else name.lower() + suffix
            present = key in model_map
            if (entry[field] != 0) != present or (present and entry[field] != model_map[key]):
                check(False, f"{mission}: {field} handle for {key} differs")
        if item["type"] == 3 and item["graphic"]:
            for field, name, suffix in (("c", item["chr"], ".chr"), ("a", item["anim"], ".anm")):
                if not name:
                    continue
                key = name.lower() if name.lower().endswith(suffix) else name.lower() + suffix
                exists = any(not e.deleted and e.name.lower() == key
                             for archive in resolver.archives for e in archive.entries)
                if (entry[field] != 0) != exists:
                    check(False, f"{mission}: {field} handle for {key} differs")
    # placement and local player structure
    placement = state["placements"][0]
    check(placement["on"] == 1 and placement["f9c"] == 1 and placement["o"] == 0 and
          placement["a1"] == PROBE_FACE_ID and placement["a2"] == 0,
          f"{mission}: placement slot 0 fields differ")
    if state["converted"][3] == 0:
        free = 0
    else:
        free = next((i for i, a in enumerate(state["groups"][3]) if a["d"] == 0),
                    state["converted"][3])
    check(placement["actor"] == free, f"{mission}: placement actor slot {placement['actor']} != {free}")
    check(placement["actor"] != -1 and state["local"]["found"] == 1 and
          state["local"]["actor"] == placement["actor"],
          f"{mission}: local player slot differs")
    check(state["counters"]["group3"] == max(state["converted"][3], free + 1),
          f"{mission}: group 3 count differs")

report["checks"]["assets_62"] = {"missions": len(missions), "definitions": len(expected_defs)}

# --- names: campaign RTXT lookups ------------------------------------------

name_rows = 0
for mission in missions:
    campaign = "dfcamp" + mission[1:3] + ".bin"
    if campaign not in {entry.name.lower() for archive in resolver.archives
                        for entry in archive.entries if not entry.deleted}:
        continue
    rtxt = Rtxt(read_member(campaign))
    state = json.loads(probe_run(["state", args.game, mission] + NAME_OPTIONS)[0])
    bms = read_member(mission)
    counts = struct.unpack_from("<4I", bms, 152)
    expected_names = []
    for group in range(4):
        for index in range(counts[group]):
            raw = bms[268 + (sum(counts[:group]) + index) * 96:][:96]
            if not accepted(le32(raw, 12), 1):
                continue
            item = items.find(le32(raw, 0))
            if not (item["flags"] & 4):
                continue
            if group == 0 and (item["flags"] & 0x20):
                continue
            name_id = le32(raw, 4)
            expected_names.append(rtxt.get("People Names", f"NAMEID{name_id:03d}") if name_id else "")
    got_names = [bytes.fromhex(person["name"]).decode("latin1") if person["name"] != "-" else ""
                 for person in state["persons"]]
    # the local player record is the placement person, renamed to ProbePlayer
    if len(got_names) != len(expected_names) + 1:
        check(False, f"{mission}: person name count {len(got_names)} != {len(expected_names) + 1}")
        continue
    local_person = state["spawns"][0]["person"]
    local_slot = (local_person - HANDLE_BASES["person"]) // PERSON_STRIDE
    reordered = got_names[:local_slot] + got_names[local_slot + 1:]
    check(got_names[local_slot] == "ProbePlayer", f"{mission}: local player name differs")
    if reordered != expected_names:
        detail = next(((i, a, b) for i, (a, b) in enumerate(zip(reordered, expected_names))
                       if a != b), None)
        check(False, f"{mission}: People Names text differs {detail}")
    name_rows += len(expected_names)

report["checks"]["names_rtxt"] = {"missions": len(missions), "names": name_rows}

# --- replay sections: real mission data through the original code ----------

replay_definitions = 0
replay_slots = 0
replay_placements = 0
for mission in missions:
    state = state_rows[mission]
    bms = read_member(mission)
    counts = struct.unpack_from("<4I", bms, 152)

    # 0x8CF908 header copy and the runtime definition table.
    u.mem_write(HEADER_COPY, bms[:0x10C])
    u.mem_write(DEF_TABLE, bytes(len(state["defs"]) * 0x13C))
    for entry in state["defs"]:
        base = DEF_TABLE + entry["i"] * 0x13C
        put(base + 0x20, entry["id"])
        u.mem_write(base + 0x24, bytes([entry["f"]]))
        put(base + 0x28, entry["t"])
        u.mem_write(base + 0x2C, entry["gn"].encode("latin1")[:15] + b"\0")
        u.mem_write(base + 0x3C, entry["hn"].encode("latin1")[:15] + b"\0")
        put(base + 0x6C, entry["m"])
        put(base + 0x70, entry["h"])
        put(base + 0x74, entry["c"])
        put(base + 0x78, entry["a"])
        put(base + 0x84, entry["ai"])
        put(base + 0x90, entry["mo"])
        put(base + 0x94, entry["hp"])
    put(DEF_PTR, DEF_TABLE)
    put(DEF_COUNT, len(state["defs"]))
    replay_definitions += len(state["defs"])

    # Convert every BMS object with the original 0x449710.
    person_calls.clear()
    character_calls.clear()
    loader_calls[0] = 0
    put(0x4F821C, 0)
    u.mem_write(PERSON_TABLE, bytes(0x500 * PERSON_STRIDE))
    for group in range(4):
        base, limit = GROUP_TABLES[group]
        put(ACTOR_COUNT[group], counts[group])
        put(GROUP_OBJECT_COUNT[group], counts[group])
        # Zero the complete table extent: the 0x45D520 free scan sees stale
        # slots from earlier missions otherwise.
        u.mem_write(base, bytes(limit * 0x114))
    at = 268
    for group in range(4):
        base = GROUP_TABLES[group][0]
        for index in range(counts[group]):
            raw = bms[at:at + 96]
            at += 96
            u.mem_write(RECORD, raw)
            sp = STACK
            u.mem_write(sp, struct.pack("<4I", RETURN, base + index * 0x114, RECORD, 0))
            u.reg_write(UC_X86_REG_ESP, sp)
            u.reg_write(UC_X86_REG_FPCW, FPCW)
            emu_run(CONVERT, RETURN, count=200000)
    # Compare the converted records with the native group rows.
    for group in range(4):
        base = GROUP_TABLES[group][0]
        for slot, actor in enumerate(state["groups"][group]):
            got = u.mem_read(base + slot * 0x114, 0x114)
            if not actor["a"]:
                if (group, slot) in mission_expectations[mission]["removed"]:
                    continue  # 0x402EC0 removal clears this record later
                check(got == bytes(0x114), f"{mission}: rejected record {group}/{slot} not zero")
                continue
            person = got[0x0C:0x10]
            person_slot = ((le32(person, 0) - PERSON_TABLE) // PERSON_STRIDE
                           if le32(person, 0) >= PERSON_TABLE and le32(person, 0) < PERSON_TABLE + 0x40000
                           else -1)
            fields = {
                "d": (le32(got, 0), actor["d"]), "o": (le32(got, 8), actor["o"]),
                "p": (person_slot, actor["p"]), "f70": (le32(got, 0x70) & 0xFFFF, actor["f70"]),
                "f72": (le32(got, 0x70) >> 16, actor["f72"]), "f6c": (le32(got, 0x6C), actor["f6c"]),
                "f10c": (le32(got, 0x10C), actor["f10c"]),
                "f110": (le32(got, 0x110), actor["f110"]),
                "xy": ([le32(got, 0x24 + 4 * i) for i in range(2)],
                       [actor["x"], actor["y"]])}
            wrong = {k: v for k, v in fields.items() if v[0] != v[1]}
            if wrong:
                check(False, f"{mission}: converted record {group}/{slot} differs {wrong}")
                break
    # Conversion loader counts: 0x402EC0 calls 45D6F0 twice for the soldier
    # pair, then once per non-null slot in groups 0, 1 and 3.
    used = sum(1 for group in (0, 1, 3)
               for actor in state["groups"][group] if actor["d"] != 0)
    person_calls.clear()
    character_calls.clear()
    loader_calls[0] = 0
    put(SCALE_HEALTH, 0)
    put(MULTIPLAYER, 0)
    emu_run(RESOURCE_PASS, RETURN, count=8000000)
    check(loader_calls[0] == used + len(state["removals"]) + 2,
          f"{mission}: 0x45D6F0 calls {loader_calls[0]} != "
          f"{used + len(state['removals']) + 2}")
    # Compare 0x402EC0 actor writes and removals.
    removed = {(r["g"], r["s"]) for r in state["removals"]}
    checked_slots = 0
    for group in (0, 1, 3):
        base = GROUP_TABLES[group][0]
        for slot, actor in enumerate(state["groups"][group]):
            got = u.mem_read(base + slot * 0x114, 0x114)
            if (group, slot) in removed:
                check(got == bytes(0x114), f"{mission}: removed slot {group}/{slot} not cleared")
                continue
            if actor["d"] == 0:
                continue
            checked_slots += 1
            health = struct.unpack_from("<H", got, 0x74)[0]
            if le32(got, 0x10) != actor["f10"] or le32(got, 0x14) != actor["f14"] or \
                    le32(got, 0x18) != actor["f18"] or health != actor["hp"]:
                check(False, f"{mission}: 0x402EC0 writes differ at {group}/{slot}")
                break
    check(checked_slots == used,
          f"{mission}: resource slot count {checked_slots} != {used}")
    # Character instances: the first calls come from the used group 3 slots.
    group3_used = [a for a in state["groups"][3] if a["d"] != 0]
    if state["instances"][:len(group3_used)] != [a["d"] for a in group3_used]:
        check(False, f"{mission}: character instance order differs")
    # Height loops: the original range 0x403C93-0x403DD4 over the four tables.
    u.mem_write(TERRAIN_BUFFER, heights[map_names(read_member(terrain_name(bms)))["elev_map"]])
    put(TERRAIN_PTR, TERRAIN_BUFFER)
    emu_run(*HEIGHT_LOOP)
    for group in (3, 0, 1, 2):
        base = GROUP_TABLES[group][0]
        for slot, actor in enumerate(state["groups"][group]):
            got = u.mem_read(base + slot * 0x114, 0x114)
            if le32(got, 0x2C) != actor["z"]:
                check(False, f"{mission}: height loop differs at {group}/{slot}")
                break
    replay_slots += sum(len(state["groups"][group]) for group in range(4))

    # 0x444C50 slot 0 and the 0x440640/0x4829F0 pair.
    person_calls.clear()
    character_calls.clear()
    placement = PLACEMENTS
    u.mem_write(placement, bytes(PLACEMENT_STRIDE))
    put(placement + 0x00, state["placements"][0]["o"])
    u.mem_write(placement + 0x0C, b"ProbePlayer\0")
    put16(placement + 0x9C, 1)
    put(placement + 0xA0, state["placements"][0]["a1"])
    put(placement + 0xA4, state["placements"][0]["a2"])
    u.mem_write(placement + 0x18C, b"\x01")
    put16(placement + 0x1C4, 0)
    put(FINALIZE_ENABLED, 1)
    put(GAME_TYPE, 0)
    u.mem_write(0x67F420, b"ProbePlayer\0")
    put(0x67F430, PROBE_FACE_ID)
    put(BIND_COUNTER, 0)
    put(PLAYER_ID, 0)
    put(WEAPON_SLOT_INDEX, 3)
    put(WEAPON_SLOT_MAP + 12, 0)
    put(PLAYER_PTR, 0)
    emu_run(FINALIZE, RETURN, count=4000000)
    emu_run(LOCAL_PLAYER, RETURN, count=4000000)
    expected = state["spawns"][0]
    actor_pointer = word(placement + 0x08)
    if actor_pointer == 0:
        check(False, f"{mission}: no placement actor")
        continue
    actor_index = (actor_pointer - ACTOR_TABLE) // 0x114
    got = u.mem_read(actor_pointer, 0x114)
    created = next((c for c in state["created"] if c["o"] == word(actor_pointer + 8)), None)
    fields = {
        "slot": (actor_index, expected["a"]),
        "o": (le32(got, 8), created["o"] if created else None),
        "f70": (le32(got, 0x70) & 0xFFFF, created["f70"] if created else None),
        "hp": (struct.unpack_from("<H", got, 0x74)[0], created["hp"] if created else None),
        "f6c": (le32(got, 0x6C), created["f6c"] if created else None),
        "f72": (le32(got, 0x70) >> 16, created["f72"] if created else None)}
    wrong = {k: v for k, v in fields.items() if v[0] != v[1]}
    if wrong:
        check(False, f"{mission}: placement actor fields differ {wrong}")
    if created and [le32(got, 0x24 + 4 * i) for i in range(5)] != [created["x"], created["y"],
                                                                   created["z"], created["yaw"],
                                                                   created["pitch"]]:
        check(False, f"{mission}: placement pose differs "
                     f"{[le32(got, 0x24 + 4 * i) for i in range(5)]} vs "
                     f"{[created['x'], created['y'], created['z'], created['yaw'], created['pitch']]}")
    player_pointer = word(PLAYER_PTR)
    if player_pointer != actor_pointer:
        check(False, f"{mission}: local player pointer differs")
    if word(actor_pointer + 0x98) != state["local"]["f98"] or \
            word(actor_pointer + 0x94) != state["local"]["f94"]:
        check(False, f"{mission}: local player writes differ")
    person_pointer = word(actor_pointer + 0x0C)
    name = bytes(u.mem_read(person_pointer + 4, 32)).split(b"\0")[0]
    if name != b"ProbePlayer":
        check(False, f"{mission}: placement person name differs")
    replay_placements += 1
    person_calls.clear()
    character_calls.clear()

report["checks"]["replay_402EC0"] = {"missions": len(missions), "definitions": replay_definitions,
                                     "slots": replay_slots}
report["checks"]["replay_finalize"] = {"missions": len(missions), "placements": replay_placements}
report["mismatch_count"] = len(mismatches)
output = json.dumps(report, indent=2)
print(output)
if args.report:
    args.report.write_text(output + "\n")
sys.exit(1 if mismatches else 0)
