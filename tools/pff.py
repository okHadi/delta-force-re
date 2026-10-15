#!/usr/bin/env python3
"""Read Delta Force 1 PFF3 archives without changing them."""

import argparse
from dataclasses import asdict, dataclass
import json
from pathlib import Path
import struct
import sys


class PffError(ValueError):
    pass


@dataclass(frozen=True)
class Entry:
    name: str
    offset: int
    size: int
    timestamp: int
    deleted: bool


def safe_name(name):
    return (bool(name) and name not in (".", "..")
            and not any(c in name for c in '/\\:')
            and all(32 <= ord(c) < 127 for c in name))


class Archive:
    def __init__(self, path):
        self.path = Path(path)
        with self.path.open('rb') as source:
            source.seek(0, 2)
            self.size = source.tell()
            source.seek(0)
            header = source.read(20)
            if len(header) != 20:
                raise PffError('Truncated header')
            length, version, count, record_size, table = struct.unpack('<I4sIII', header)
            if length != 20 or version != b'PFF3' or record_size != 32:
                raise PffError('Expected DF1 PFF3 header (20 bytes, 32-byte records)')
            if table < 20 or table > self.size or count > (self.size - table) // 32:
                raise PffError('Entry table is outside the archive')
            self.table_offset = table
            self.footer_bytes = self.size - table - count * 32
            self.entries = []
            source.seek(table)
            for index in range(count):
                record = source.read(32)
                if len(record) != 32:
                    raise PffError('Truncated entry table')
                flags, offset, size, timestamp, raw_name = struct.unpack('<IIII16s', record)
                if flags not in (0, 1):
                    raise PffError(f'Entry {index}: unknown flags')
                if b'\0' not in raw_name:
                    raise PffError(f'Entry {index}: missing name terminator')
                try:
                    name = raw_name.split(b'\0', 1)[0].decode('ascii')
                except UnicodeDecodeError as error:
                    raise PffError(f'Entry {index}: non-ASCII name') from error
                if not safe_name(name):
                    raise PffError(f'Entry {index}: unsafe name')
                if flags == 1 and name != '<DEAD SPACE>':
                    raise PffError(f'Entry {index}: invalid deleted entry')
                if offset < 20 or offset > table or size > table - offset:
                    raise PffError(f'Entry {index}: data is outside the data section')
                self.entries.append(Entry(name, offset, size, timestamp, bool(flags)))

    def manifest(self):
        return {'archive': self.path.name, 'size': self.size, 'version': 'PFF3',
                'table_offset': self.table_offset, 'footer_bytes': self.footer_bytes,
                'entries': [asdict(entry) for entry in self.entries]}

    def extract(self, name, output):
        if not safe_name(name):
            raise PffError('Unsafe entry name')
        matches = [entry for entry in self.entries
                   if not entry.deleted and entry.name.casefold() == name.casefold()]
        if len(matches) != 1:
            raise PffError('Expected one matching live entry')
        entry = matches[0]
        # Exclusive creation also refuses existing symlinks and the source path.
        with self.path.open('rb') as source, Path(output).open('xb') as target:
            source.seek(entry.offset)
            remaining = entry.size
            while remaining:
                block = source.read(min(remaining, 1024 * 1024))
                if not block:
                    raise PffError('Entry data was truncated during extraction')
                target.write(block)
                remaining -= len(block)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    listing = commands.add_parser('list', help='Write entry metadata as JSON')
    listing.add_argument('archive')
    extraction = commands.add_parser('extract', help='Copy one live entry to a new file')
    extraction.add_argument('archive')
    extraction.add_argument('name')
    extraction.add_argument('output')
    args = parser.parse_args()
    try:
        archive = Archive(args.archive)
        if args.command == 'list':
            print(json.dumps(archive.manifest(), indent=2))
        else:
            archive.extract(args.name, args.output)
    except (OSError, PffError) as error:
        print(f'pff: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
