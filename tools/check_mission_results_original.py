#!/usr/bin/env python3
"""Check mission result rules against original Df.exe x86.

Builds the native probe and test with /usr/bin/clang++, codesigns them, then:

  1. runs tests/native_mission_results.cpp,
  2. replays original single-player result recording (0x401e70 -> 0x407de0 ->
     0x407980) and the debrief won check (0x41f320) under Unicorn and compares
     the resulting profile bytes and flags with the native probe,
  3. replays the all-complete scan (0x41ef40) against the native rule,
  4. checks the original campaign text tables carry Win/Lose debrief text for
     every mission, and that original BMS prerequisite words drive the
     availability rule the way the campaign map does.

The original executable is read-only; the executable hash is verified first.
The debrief stats accumulators are not covered (see docs). Run with the venv
python that provides unicorn and pefile:

  .local/tools/python/bin/python3 tools/check_mission_results_original.py \
      "/Users/khanh/Desktop/workspace/Delta Force" [--report FILE]
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
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pff import Archive  # noqa: E402

SOURCE_SHA = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
PROFILE_SIZE = 0xa90
SCRATCH = Path('.local/opencode/mission_results')
MISSION_COUNTS = (6, 6, 8, 10, 10)

# Original single-player result recording (see docs/mission-results.md).
RESULT_RECORDER = 0x401e70
COMPLETION_WRITER = 0x407de0
MASK_STORE = 0x407980
DEBRIEF_WON = 0x41f320
ALL_COMPLETE = 0x41ef40
MISSION_COUNT_TABLE = 0x4d0d08


def build(source_root, out_dir, core=None):
    """Compile the native test and probe with clang++ and sign them."""
    clang = Path('/usr/bin/clang++')
    probe = out_dir / 'mission_results_probe'
    test = out_dir / 'native_mission_results'
    core = core or source_root / 'build' / 'libdfre_core.a'
    if not core.exists():
        raise SystemExit('build/libdfre_core.a is missing; run the CMake build first')
    subprocess.run([str(clang), '-std=c++17', '-O2', '-I', str(source_root / 'include'),
                    str(source_root / 'src' / 'mission_results.cpp'),
                    str(source_root / 'tools' / 'mission_results_probe.cpp'),
                    str(core), '-o', str(probe)], check=True)
    subprocess.run([str(clang), '-std=c++17', '-O2', '-I', str(source_root / 'include'),
                    str(source_root / 'src' / 'mission_results.cpp'),
                    str(source_root / 'tests' / 'native_mission_results.cpp'),
                    '-o', str(test)], check=True)
    for binary in (probe, test):
        subprocess.run(['codesign', '--force', '--sign', '-', str(binary)], check=True)
    return probe, test


class Original:
    """Runs original Df.exe fragments under Unicorn."""

    def __init__(self, source_root, game):
        source = (game / 'Df.exe').read_bytes()
        if hashlib.sha256(source).hexdigest() != SOURCE_SHA:
            raise ValueError('Unsupported original executable hash')
        self.pe = pefile.PE(data=source)
        self.game = game
        archive_names = ('Df.pff', 'Dfbase.pff')
        self.archives = [Archive(game / name) for name in archive_names]

    def read(self, name):
        for archive in self.archives:
            for entry in archive.entries:
                if not entry.deleted and entry.name.lower() == name.lower():
                    with archive.path.open('rb') as source:
                        source.seek(entry.offset)
                        return source.read(entry.size)
        raise ValueError(f'missing archive member {name}')

    def machine(self):
        u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
        u.mem_map(0x400000, 0xa00000)
        u.mem_write(0x400000, self.pe.get_memory_mapped_image())
        u.mem_map(0x2000000, 0x10000)  # stack
        u.mem_map(0x3000000, 0x10000)  # profile buffer
        return u

    @staticmethod
    def w32(u, address, value):
        u.mem_write(address, struct.pack('<I', value & 0xffffffff))

    @staticmethod
    def r32(u, address):
        return struct.unpack('<I', u.mem_read(address, 4))[0]

    def call(self, u, entry, profile, extra=None, expect_return=True):
        u.mem_write(0x3000000, bytes(profile))
        self.w32(u, 0x4cf2e4, 0x3000000)
        if extra:
            extra(u)
        self.w32(u, 0x2008000, 0x2009000)
        u.mem_write(0x2009000, b'\xc3')
        u.reg_write(UC_X86_REG_ESP, 0x2008000)
        u.emu_start(entry, 0x2009000, count=4_000_000)
        if expect_return and u.reg_read(UC_X86_REG_EIP) != 0x2009000:
            raise ValueError(f'original 0x{entry:x} did not return')

    def apply_result(self, profile, campaign, mission, completed, mask):
        """Single-player 0x401e70: completion word, mask, live objective bits."""
        u = self.machine()

        def setup(machine):
            self.w32(machine, 0x67f520, 0)          # single player
            self.w32(machine, 0x67f528, 1)          # host (single player value)
            machine.mem_write(0x67f714, bytes(0x514))
            self.w32(machine, 0x67f718, 1 if completed else 0)
            self.w32(machine, 0x67dbec, mask)
            self.w32(machine, 0x67f70c, self.r32(machine, 0x3000000 + 0x70 + campaign * 0x44))
            self.w32(machine, 0x68ba34, mission)
        self.call(u, RESULT_RECORDER, profile, setup)
        return u.mem_read(0x3000000, len(profile))

    def debrief_won(self, profile, campaign, mission):
        u = self.machine()

        def setup(machine):
            self.w32(machine, 0x68ba34, mission)
            self.w32(machine, 0x4e6cfc, 0xffffffff)
        self.call(u, DEBRIEF_WON, profile, setup)
        return self.r32(u, 0x4e6cfc) != 0

    def all_complete(self, profile, campaign, count):
        """0x41ef40 reaches its 0x4c4f40 call site only when every word is set."""
        u = self.machine()
        self.w32(u, MISSION_COUNT_TABLE + campaign * 4, count)
        reached = []

        def hook(machine, address, size, user):
            if address == 0x41ef68:  # push 3 before the 0x4c4f40 call
                reached.append(True)
                machine.emu_stop()

        u.hook_add(unicorn.UC_HOOK_CODE, hook)
        self.call(u, ALL_COMPLETE, profile, expect_return=False)
        return bool(reached)


def synthetic_profile(campaign=1, selected=0):
    profile = bytearray(PROFILE_SIZE)
    profile[0:3] = b'PLY'
    profile[3] = 0x1b
    struct.pack_into('<I', profile, 0x28, campaign)
    struct.pack_into('<I', profile, 0x470 + campaign * 4, selected)
    return bytes(profile)


def parse_profile_words(data, campaign, mission):
    completion = struct.unpack_from('<I', data, 0x30 + campaign * 0x44 + mission * 4)[0]
    mask = struct.unpack_from('<I', data, 0x70 + campaign * 0x44)[0]
    return completion, mask


def rtxt_groups(data):
    if data[:4] != b'RTXT':
        raise ValueError('expected RTXT')
    metadata = struct.unpack_from('<I', data, 4)[0]
    count = struct.unpack_from('<I', data, 12)[0]
    groups = struct.unpack_from('<I', data, metadata)[0]
    text_base = 16 + count * 16
    out = {}
    name_at = metadata + 4 + groups * 8
    index = 0
    for group in range(groups):
        end = data.index(b'\0', name_at)
        name = data[name_at:end].decode('latin1')
        name_at = end + 1
        key_at = metadata + 4 + struct.unpack_from('<I', data, metadata + 4 + group * 8)[0]
        length = struct.unpack_from('<I', data, metadata + 8 + group * 8)[0]
        table = {}
        for _ in range(length):
            end = data.index(b'\0', key_at)
            key = data[key_at:end].decode('latin1')
            key_at = end + 1
            offset = struct.unpack_from('<I', data, 16 + index * 16)[0]
            end = data.index(b'\0', text_base + offset)
            table[key] = data[text_base + offset:end].decode('latin1')
            index += 1
        out[name] = table
    return out


def check(game, source_root, report_path, core=None):
    original = Original(source_root, game)
    SCRATCH.mkdir(parents=True, exist_ok=True)
    probe, test = build(source_root, SCRATCH, core)
    subprocess.run([str(test)], check=True, capture_output=True)

    original_cases = 0
    for campaign in range(1, 6):
        for mission in (0, MISSION_COUNTS[campaign - 1] // 2, MISSION_COUNTS[campaign - 1] - 1):
            for completed in (False, True):
                for mask in (0, 1, 0x55, 0xDEADBEEF, 0x80000000):
                    profile = bytearray(synthetic_profile(campaign, mission))
                    struct.pack_into('<I', profile, 0x30 + campaign * 0x44 + mission * 4, 0)
                    struct.pack_into('<I', profile, 0x70 + campaign * 0x44, 0x5678)
                    profile = bytes(profile)
                    expected_bytes = original.apply_result(profile, campaign, mission, completed, mask)
                    expected_completion, expected_mask = parse_profile_words(expected_bytes, campaign, mission)

                    work = SCRATCH / f'case-{campaign}-{mission}-{int(completed)}-{mask:x}.ply'
                    out = work.with_suffix('.out.ply')
                    work.write_bytes(profile)
                    args = [str(probe), str(game), str(work), '--apply', str(campaign), str(mission),
                            '1' if completed else '0', hex(mask), '--out', str(out)]
                    result = json.loads(subprocess.check_output(args))
                    native_bytes = out.read_bytes()
                    if native_bytes != expected_bytes:
                        raise ValueError(f'profile bytes differ: campaign {campaign} mission {mission} '
                                         f'completed {completed} mask {mask:#x}')
                    apply = result['apply']
                    if apply['completion_written'] != int(completed) or apply['mask_written'] != int(completed):
                        raise ValueError(f'write flags differ: {campaign}/{mission}/{completed}')
                    if completed:
                        if apply['completion_word'] != expected_completion or apply['campaign_mask'] != expected_mask:
                            raise ValueError(f'update values differ: {campaign}/{mission}/{completed}/{mask:#x}')
                        if expected_completion != 1 or expected_mask != mask:
                            raise ValueError(f'original update differs: {campaign}/{mission}/{completed}/{mask:#x}')
                    elif expected_completion != 0 or expected_mask != 0x5678:
                        raise ValueError(f'original loss differs: {campaign}/{mission}')
                    if apply['debrief_won'] != (expected_completion != 0):
                        raise ValueError(f'debrief flag differs: {campaign}/{mission}/{completed}')
                    if bool(apply['debrief_won']) != original.debrief_won(expected_bytes, campaign, mission):
                        raise ValueError(f'original 0x41f320 differs: {campaign}/{mission}')
                    original_cases += 1

    # All-complete rule: original 0x41ef40, native probe, and the Python rule
    # agree for zero, one missing, and complete word sets.
    all_complete_cases = 0
    for campaign in (1, 3, 5):
        count = MISSION_COUNTS[campaign - 1]
        for missing in (None, 0, count - 1):
            profile = bytearray(synthetic_profile(campaign, 0))
            for mission in range(count):
                if mission != missing:
                    struct.pack_into('<I', profile, 0x30 + campaign * 0x44 + mission * 4, 1)
            expected = missing is None
            if original.all_complete(bytes(profile), campaign, count) != expected:
                raise ValueError(f'original 0x41ef40 differs: campaign {campaign} missing {missing}')
            work = SCRATCH / f'all-complete-{campaign}-{missing}.ply'
            work.write_bytes(profile)
            result = json.loads(subprocess.check_output([str(probe), str(game), str(work)]))
            if bool(result['campaigns'][campaign - 1]['complete']) != expected:
                raise ValueError(f'probe campaign_complete differs: campaign {campaign} missing {missing}')
            if campaign_complete_rule(bytes(profile), campaign, count) != expected:
                raise ValueError(f'python all-complete rule differs: campaign {campaign} missing {missing}')
            all_complete_cases += 1

    # Debrief text: every mission has Win/Lose text in the campaign table.
    text_cases = 0
    for campaign in range(1, 6):
        data = original.read(f'dfcamp{campaign + 1:02d}.bin')
        groups = rtxt_groups(data)
        for mission in range(1, MISSION_COUNTS[campaign - 1] + 1):
            group = groups.get(f'Briefing{mission}')
            if not group or not group.get(f'Win{mission}') or not group.get(f'Lose{mission}'):
                raise ValueError(f'missing debrief text: campaign {campaign} mission {mission}')
            text_cases += 1

    # Availability rule vs original BMS prerequisite words.
    availability_cases = 0
    for campaign in range(1, 6):
        mask_all = 0
        for mission in range(1, MISSION_COUNTS[campaign - 1] + 1):
            header = original.read(f'C{campaign + 1:02d}M{mission:02d}.BMS')
            prerequisites = struct.unpack_from('<I', header, 0x7c)[0]
            mask_all |= prerequisites
        profile0 = synthetic_profile(campaign, 0)
        profile_all = bytearray(profile0)
        # Original masks accumulate objective bits; a full pass uses every
        # prerequisite bit any mission in the campaign asks for.
        for mission in range(1, MISSION_COUNTS[campaign - 1] + 1):
            header = original.read(f'C{campaign + 1:02d}M{mission:02d}.BMS')
            prerequisites = struct.unpack_from('<I', header, 0x7c)[0]
            struct.pack_into('<I', profile_all, 0x70 + campaign * 0x44, mask_all)
            if prerequisites and not mission_available_rule(bytes(profile_all), campaign, mission - 1, prerequisites):
                raise ValueError(f'full mask must unlock: campaign {campaign} mission {mission}')
            if prerequisites and mission_available_rule(profile0, campaign, mission - 1, prerequisites):
                raise ValueError(f'empty mask must lock: campaign {campaign} mission {mission}')
            availability_cases += 1

    result = dict(
        source_sha256=SOURCE_SHA,
        original_result_cases=original_cases,
        all_complete_cases=all_complete_cases,
        debrief_text_cases=text_cases,
        availability_cases=availability_cases,
        functions=[hex(RESULT_RECORDER), hex(COMPLETION_WRITER), hex(MASK_STORE), hex(DEBRIEF_WON), hex(ALL_COMPLETE)],
        scope='single-player completion word (0x407f16), campaign mask store (0x407a80), debrief won read (0x41f320), '
              'all-complete scan (0x41ef40), original BMS prerequisite words and debrief text keys; stats accumulators '
              'and the writer of the result state block flag 0x67f718 are not covered')
    output = json.dumps(result, indent=2)
    print(output)
    if report_path:
        Path(report_path).write_text(output + '\n')
    return result


# The native rule lives in the compiled probe/library. The checks above also
# reimplement the two small decisions in Python so availability and campaign
# completeness inputs remain readable next to the original BMS words.
def campaign_complete_rule(profile, campaign, count):
    for mission in range(count):
        if struct.unpack_from('<I', profile, 0x30 + campaign * 0x44 + mission * 4)[0] == 0:
            return False
    return True


def mission_available_rule(profile, campaign, mission, prerequisites):
    if struct.unpack_from('<I', profile, 0x30 + campaign * 0x44 + mission * 4)[0] != 0:
        return False
    mask = struct.unpack_from('<I', profile, 0x70 + campaign * 0x44)[0]
    return prerequisites == 0 or (mask & prerequisites) == prerequisites


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument('--core', type=Path)
    args = parser.parse_args()
    check(args.game, args.root, args.report, args.core)
