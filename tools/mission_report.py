#!/usr/bin/env python3
"""Inspect the checked DF1 BMS v13 layout. Keep reports under .local/."""

import argparse
import collections
import json
import struct
import sys

from mission import Resolver, terrain_name


def parse(data):
    if len(data) < 268 or data[:4] != b'BMS\r':
        raise ValueError('Expected DF1 BMS v13 header')
    terrain = terrain_name(data)
    counts = struct.unpack_from('<4I', data, 152)
    fixed = 268 + sum(counts) * 96
    event_start = fixed + 17408 + 2048 + 640
    if event_start + 12 > len(data):
        raise ValueError('Truncated BMS objects or fixed blocks')
    event_count, trigger_count, action_count = struct.unpack_from('<3I', data, event_start)
    if struct.unpack_from('<I', data, 168)[0] != event_count:
        raise ValueError('BMS event counts differ')
    if event_start + 12 + event_count * 24 + (trigger_count + action_count) * 32 != len(data):
        raise ValueError('BMS event tables do not match the file length')
    groups = []
    at = 268
    ids = set()
    for count in counts:
        group = []
        for _ in range(count):
            words = struct.unpack_from('<24I', data, at)
            if words[2] in ids:
                raise ValueError('Duplicate BMS object ID')
            ids.add(words[2])
            group.append({'offset': at, 'type_id': words[0], 'name_id': words[1],
                          'object_id': words[2], 'flags': words[3],
                          'position_raw': struct.unpack_from('<3i', data, at + 16),
                          'angle_degrees': struct.unpack_from('<2h', data, at + 52),
                          'byte_67': data[at + 67], 'byte_72': data[at + 72],
                          'words': words})
            at += 96
        groups.append(group)
    marker_lists = []
    for _ in range(128):
        words = struct.unpack_from('<34I', data, at)
        if words[1] > 32 or any(i >= counts[2] for i in words[2:2 + words[1]]):
            raise ValueError('Invalid BMS marker-list count or index')
        marker_lists.append(words)
        at += 136
    group_records = [struct.unpack_from('<8I', data, at + i * 32) for i in range(64)]
    at += 2048
    text_records = []
    for i in range(32):
        raw = data[at + i * 20:at + (i + 1) * 20]
        if b'\0' not in raw[4:]:
            raise ValueError('Unterminated BMS text slot')
        text_records.append(raw.hex())
    at = event_start + 12
    events = [struct.unpack_from('<6I', data, at + i * 24) for i in range(event_count)]
    at += event_count * 24
    triggers = [struct.unpack_from('<8I', data, at + i * 32) for i in range(trigger_count)]
    at += trigger_count * 32
    actions = [struct.unpack_from('<8I', data, at + i * 32) for i in range(action_count)]
    for words in events:
        triggers_used = (words[5] >> 8) & 255
        actions_used = (words[5] >> 16) & 255
        if (words[1] > trigger_count or words[2] > action_count
                or triggers_used > trigger_count - words[1]
                or actions_used > action_count - words[2]):
            raise ValueError('BMS event table index is outside its table')
    return {'terrain': terrain, 'header': data[:268].hex(), 'object_groups': groups,
            'marker_lists': marker_lists, 'group_records': group_records,
            'text_records': text_records, 'events': events, 'triggers': triggers, 'actions': actions}


def summary(parsed):
    return {'terrain': parsed['terrain'],
            'object_counts': [len(group) for group in parsed['object_groups']],
            'type_counts': [dict(collections.Counter(row['type_id'] for row in group))
                            for group in parsed['object_groups']],
            'marker_lists_used': sum(bool(row[1]) for row in parsed['marker_lists']),
            'events': len(parsed['events']), 'triggers': len(parsed['triggers']),
            'actions': len(parsed['actions'])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_folder')
    parser.add_argument('mission', nargs='?')
    parser.add_argument('--records', action='store_true', help='Include all raw records')
    args = parser.parse_args()
    try:
        resolver = Resolver(args.game_folder)
        names = [args.mission] if args.mission else [
            entry.name for archive in resolver.archives for entry in archive.entries
            if not entry.deleted and entry.name.lower().endswith('.bms')]
        results = []
        for name in names:
            data, source = resolver.read(*resolver.find(name))
            parsed = parse(data)
            result = {'source': source, **summary(parsed)}
            if args.records:
                result['records'] = parsed
            results.append(result)
        print(json.dumps({'missions': results, 'resolved': len(results)}, indent=2))
        return 0
    except (OSError, ValueError) as error:
        print(f'mission_report: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
