#!/usr/bin/env python3
"""Read 8-bit, single-plane PCX files. Export pixel indices as PGM."""

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import struct
import sys


class PcxError(ValueError):
    pass


@dataclass(frozen=True)
class Image:
    width: int
    height: int
    pixels: bytes
    palette: bytes | None
    ancillary: bytes | None = None


def decode(data):
    if len(data) < 128:
        raise PcxError('Truncated PCX header')
    if data[:4] != bytes((10, 5, 1, 8)) or data[65] != 1:
        raise PcxError('Expected version 5, RLE, 8-bit, single-plane PCX')
    xmin, ymin, xmax, ymax = struct.unpack_from('<HHHH', data, 4)
    if xmax < xmin or ymax < ymin:
        raise PcxError('Invalid image bounds')
    width, height = xmax - xmin + 1, ymax - ymin + 1
    stride = struct.unpack_from('<H', data, 66)[0]
    if width > 32768 or height > 32768 or stride < width or stride % 2:
        raise PcxError('Invalid image size or row stride')
    if stride * height > 64 * 1024 * 1024:
        raise PcxError('Image exceeds the 64 MiB decode limit')
    pixels = bytearray()
    at = 128
    for _ in range(height):
        row = bytearray()
        while len(row) < stride:
            if at >= len(data):
                raise PcxError('Truncated RLE data')
            value = data[at]
            at += 1
            count = 1
            if value >= 192:
                count = value & 63
                if count == 0:
                    raise PcxError('Zero-length RLE run')
                if at >= len(data):
                    raise PcxError('Truncated RLE run')
                value = data[at]
                at += 1
            if count > stride - len(row):
                raise PcxError('RLE run crosses a row boundary')
            row.extend(bytes((value,)) * count)
        pixels.extend(row[:width])
    tail = data[at:]
    ancillary = None
    # Observed DF1 sky gradients carry one fixed-size opaque CerP block.
    if (width, height, stride) == (4, 4, 4) and len(tail) == 1797 and tail[:4] == b'CerP':
        ancillary, tail = tail[:1028], tail[1028:]
    if tail and (len(tail) != 769 or tail[0] != 12):
        raise PcxError('Invalid palette or trailing data')
    return Image(width, height, bytes(pixels), tail[1:] if tail else None, ancillary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input')
    parser.add_argument('--pgm', help='Write raw pixel indices to a new PGM file')
    args = parser.parse_args()
    try:
        image = decode(Path(args.input).read_bytes())
        if args.pgm:
            with Path(args.pgm).open('xb') as target:
                target.write(f'P5\n{image.width} {image.height}\n255\n'.encode('ascii'))
                target.write(image.pixels)
        print(json.dumps({'width': image.width, 'height': image.height,
                          'pixels': len(image.pixels), 'minimum': min(image.pixels),
                          'maximum': max(image.pixels), 'unique_indices': len(set(image.pixels)),
                          'palette_entries': 256 if image.palette else 0,
                          'ancillary_bytes': len(image.ancillary or b'')}, indent=2))
    except (OSError, PcxError) as error:
        print(f'pcx: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
