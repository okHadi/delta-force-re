#!/usr/bin/env python3
"""Resolve a DF1 BMS member to its TRN and three terrain maps."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

from pff import Archive, PffError
from pcx import decode, PcxError


class MissionError(ValueError):
    pass


FIELDS = ('color_map', 'elev_map', 'detail_map')
STEM = re.compile(r'[A-Za-z0-9_]{1,11}', re.ASCII)


def member_name(name, suffix):
    if not name.lower().endswith(suffix) or not STEM.fullmatch(name[:-len(suffix)]):
        raise MissionError(f'Invalid {suffix} member name')
    return name


def terrain_name(data):
    if len(data) < 100 or data[:4] != b'BMS\r':
        raise MissionError('Expected a BMS header and 16-byte terrain field at byte 84')
    field = data[84:100]
    if b'\0' not in field:
        raise MissionError('Terrain filename has no terminator')
    try:
        name = field.split(b'\0', 1)[0].decode('ascii')
    except UnicodeDecodeError as error:
        raise MissionError('Terrain filename is not ASCII') from error
    return member_name(name, '.trn')


def map_names(data):
    if len(data) > 65536 or b'\0' in data:
        raise MissionError('Invalid TRN text length or null byte')
    try:
        text = data.decode('ascii')
    except UnicodeDecodeError as error:
        raise MissionError('TRN text is not ASCII') from error
    if any(ord(char) < 32 and char not in '\r\n\t' for char in text):
        raise MissionError('TRN text contains control bytes')
    result = {}
    for line in text.splitlines():
        words = line.split('//', 1)[0].split()
        if not words or words[0] not in FIELDS:
            continue
        field = words[0]
        if field in result or len(words) != 2 or not STEM.fullmatch(words[1]):
            raise MissionError(f'Invalid or duplicate {field}')
        result[field] = words[1] + '.PCX'
    if set(result) != set(FIELDS):
        raise MissionError('TRN requires one color_map, elev_map, and detail_map')
    return result


class Resolver:
    def __init__(self, folder):
        self.archives = [Archive(Path(folder) / name) for name in ('Df.pff', 'Dfbase.pff')]
        self.map_cache = {}

    def find(self, name):
        matches = [(archive, entry) for archive in self.archives for entry in archive.entries
                   if not entry.deleted and entry.name.casefold() == name.casefold()]
        if len(matches) != 1:
            raise MissionError(f'{name}: expected one live member; found {len(matches)}')
        return matches[0]

    @staticmethod
    def read(archive, entry):
        with archive.path.open('rb') as source:
            source.seek(entry.offset)
            data = source.read(entry.size)
        if len(data) != entry.size:
            raise MissionError('Member was truncated during read')
        metadata = {'archive': archive.path.name, 'member': entry.name,
                    'offset': entry.offset, 'bytes': entry.size,
                    'sha256': hashlib.sha256(data).hexdigest()}
        return data, metadata

    def resolve(self, mission):
        member_name(mission, '.bms')
        data, mission_info = self.read(*self.find(mission))
        trn = terrain_name(data)
        data, terrain_info = self.read(*self.find(trn))
        maps = {}
        for field, name in map_names(data).items():
            key = name.casefold()
            if key not in self.map_cache:
                data, info = self.read(*self.find(name))
                image = decode(data)
                info.update(width=image.width, height=image.height,
                            pixels=len(image.pixels), minimum=min(image.pixels),
                            maximum=max(image.pixels), palette_entries=256 if image.palette else 0)
                self.map_cache[key] = info
            maps[field] = dict(self.map_cache[key])
        return {'mission': mission_info, 'terrain': terrain_info, 'maps': maps}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_folder')
    parser.add_argument('mission', help='BMS member name')
    args = parser.parse_args()
    try:
        print(json.dumps(Resolver(args.game_folder).resolve(args.mission), indent=2))
    except (OSError, PffError, PcxError, MissionError) as error:
        print(f'mission: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
