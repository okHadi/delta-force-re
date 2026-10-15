#!/usr/bin/env python3
"""Compare the Cmprssh0 codec port with the original DLL under Unicorn.

Reads the original DLL read-only, runs its init_compression, compress_buffer
and expand_buffer exports on the original x86 code, and compares them with
tools/compression_probe over a deterministic corpus: empty, short, random,
low-entropy, protocol-like and optional real-file payloads, plus invalid and
truncated streams. No network traffic and no writes outside the emulator.

Usage:
  .local/tools/python/bin/python tools/check_compression_original.py \
      "/path/to/Cmprssh0.dll" .local/reports/compression-probe [real file ...]

Exit code 0 when every compared case matches.
"""

import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess
import sys

import pefile
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP,
    UC_X86_REG_EIP, UC_X86_REG_EFLAGS,
)

SOURCE_HASH = '4dbc4de8458b92c4b07933fe6ac6132912f51e3490c511c65731ac12e13539a6'
INIT, COMPRESS, EXPAND = 0x10001150, 0x100012E0, 0x10001400
ROOT_VA = 0x10007E80
CODES_VA = 0x100096C0
STACK, DATA, OUT, STOP = 0x20000000, 0x30000000, 0x31000000, 0x40000000
EXPAND_CAPACITY = 0x10000
# Zero bytes the emulator guarantees after a stream. Worst case a decode
# consumes 11 bits per output byte, so a walk that runs to the 64 KiB output
# cap needs under 11 * (0x10000 + 1) bits. 0x18000 bytes cover that, which
# keeps every original run deterministic: it either ends inside the padding or
# overflows the output window (the port then refuses by capacity).
EXPAND_PADDING = 0x18000
INSTRUCTION_BUDGET = 20_000_000
TIME_BUDGET_US = 10_000_000


class Original:
    """Original x86 code under Unicorn; all calls are bounded and recorded."""

    def __init__(self, data):
        self.pe = pefile.PE(data=data)
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        base = self.pe.OPTIONAL_HEADER.ImageBase
        size = (self.pe.OPTIONAL_HEADER.SizeOfImage + 0xFFF) & ~0xFFF
        self.uc.mem_map(base, size)
        self.uc.mem_write(base, self.pe.get_memory_mapped_image())
        self.pe.close()
        for region, span in ((STACK, 0x10000), (DATA, 0x200000), (OUT, 0x20000), (STOP, 0x1000)):
            self.uc.mem_map(region, span)
        self.call(INIT, [], budget=60_000_000)
        self.root = struct.unpack('<I', self.uc.mem_read(ROOT_VA, 4))[0]

    def call(self, fn, args, budget=INSTRUCTION_BUDGET):
        sp = STACK + 0x8000
        words = [STOP] + list(args)
        self.uc.mem_write(sp, struct.pack('<%dI' % len(words), *words))
        for reg in (UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI,
                    UC_X86_REG_EDI, UC_X86_REG_EBP):
            self.uc.reg_write(reg, 0)
        self.uc.reg_write(UC_X86_REG_ESP, sp)
        self.uc.reg_write(UC_X86_REG_EFLAGS, 2)
        try:
            self.uc.emu_start(fn, STOP, timeout=TIME_BUDGET_US, count=budget)
        except unicorn.UcError as error:
            return None, 'error:%s' % error
        if self.uc.reg_read(UC_X86_REG_EIP) != STOP:
            return None, 'budget'
        return self.uc.reg_read(UC_X86_REG_EAX) & 0xFFFFFFFF, 'ok'

    def compress(self, payload):
        """Returns (bits, bytes); raises if the original does not return."""
        self.uc.mem_write(DATA, payload + b'\x00' * 16)
        self.uc.mem_write(OUT, bytes(EXPAND_CAPACITY + 16))
        value, status = self.call(COMPRESS, [DATA, len(payload), OUT, EXPAND_CAPACITY])
        if status != 'ok':
            raise RuntimeError('original compress did not return: %s' % status)
        bits = value
        return bits, bytes(self.uc.mem_read(OUT, (bits + 7) // 8))

    def expand(self, stream):
        """Bounded expand of `stream` plus zero padding.

        Returns (status, bytes). status is 'ok', 'overflow' (the original wrote
        past the capacity window) or 'budget' (no return within the budget).
        """
        padded = stream + bytes(EXPAND_PADDING)
        self.uc.mem_write(DATA, padded)
        self.uc.mem_write(OUT, bytes(EXPAND_CAPACITY + 16))
        value, status = self.call(EXPAND, [DATA, OUT, EXPAND_CAPACITY])
        if status == 'budget':
            return 'budget', None
        if status != 'ok':
            return 'overflow', None  # wrote past the guarded output window
        if value > EXPAND_CAPACITY:
            return 'overflow', None
        return 'ok', bytes(self.uc.mem_read(OUT, value))

    def code_table(self):
        table = []
        for symbol in range(257):
            code = struct.unpack('<I', self.uc.mem_read(CODES_VA + symbol * 8, 4))[0]
            bits = struct.unpack('<I', self.uc.mem_read(CODES_VA + symbol * 8 + 4, 4))[0]
            table.append((code, bits))
        return table


def protocol_like():
    out = bytearray()
    for i in range(20):
        body = ('player%d joined the game' % i).encode()
        out += bytes([0x02, 0x00, (len(body) + 8) & 0xFF, 0x00]) + body + bytes(4)
    return bytes(out)


def build_corpus(real_files):
    cases = []  # (label, payload)

    def add(label, payload):
        cases.append((label, bytes(payload)))

    add('empty', b'')
    for size in range(0, 81):
        add('cycle%d' % size, bytes((i * 7) & 0xFF for i in range(size)))
    for value in range(256):
        add('byte%02x' % value, bytes([value]))
    for fill in (0x00, 0xFF, ord('a')):
        for size in (2, 3, 4, 5, 8, 16, 31, 32, 33, 64):
            add('run%02x.%d' % (fill, size), bytes([fill]) * size)
    add('http', b'GET / HTTP/1.1\r\nHost: game\r\n\r\n')
    add('chat', b'\x02\x00\x12\x00player joined the game\x00')
    add('protocol', protocol_like())
    add('text', b'Delta Force network compression test payload')
    add('range', bytes(range(256)))
    lengths = (1, 2, 3, 7, 8, 15, 16, 31, 32, 33, 63, 64, 127, 128, 255, 256, 511, 512, 1024, 2048)
    for seed in (0xDF1A01, 0x5150, 0xC0FFEE, 0x9E3779B9):
        rng = random.Random(seed)
        for size in lengths:
            add('random.%06x.%d' % (seed, size), bytes(rng.randrange(256) for _ in range(size)))
    for seed in (0x51E0, 0xABCDEF):
        rng = random.Random(seed)
        for size in (16, 64, 256, 1000):
            add('low.%06x.%d' % (seed, size), bytes(rng.randrange(4) for _ in range(size)))
    rng = random.Random(0xF00D)
    for size in (64, 256, 1024):
        add('skew.%d' % size, bytes(ord('a') if rng.randrange(10) else rng.randrange(256)
                                    for _ in range(size)))
    for path in real_files:
        blob = Path(path).read_bytes()
        add('real.%s' % Path(path).name, blob[:65536])
        if len(blob) > 65536:
            add('real.%s.tail' % Path(path).name, blob[-4096:])
    return cases


def build_invalid_streams():
    streams = []
    rng = random.Random(0x1BADB002)
    for size in (1, 2, 3, 4, 5, 8, 16, 32, 64, 128):
        streams.append(('zeros.%d' % size, bytes(size)))
        streams.append(('ones.%d' % size, bytes([0xFF]) * size))
        streams.append(('raw.%d' % size, bytes(rng.randrange(256) for _ in range(size))))
    for size in (7, 33, 255):
        streams.append(('count.%d' % size, bytes((i * 13 + 5) & 0xFF for i in range(size))))
    # Raw streams whose zero-padded walk reaches the end-of-stream code inside
    # the padding, so the original returns a real result for non-compressed
    # input and the port must reproduce it exactly.
    streams.append(('rawok.32', bytes.fromhex(
        '642d33b09a64cf2d5f81eef1c251cc4207d0b7ee16af731bc8a4038615e637f5')))
    streams.append(('rawok.255a', bytes.fromhex(
        '7f341f4fb6c8c4c323b0d0fe1438487b19d68417b01fbf130079620697c3c1945795ac972504fc663493c28a8530cc'
        '5437993cc9e11cfb3d02f47e776d6e191924b3eafe7e19d2af01b4dc0d7ca592f7865b78cdbed9bc49bd2ccb530af0'
        '861cef5114e54db7c0f1a88febc0fc6bd6715cf996600567c76f3e57eb442093857967d0a569f2ccc0da50004a47af'
        'ebb2255a80c0126faee41e2fa74e9e04c10449c60e21d8fbfc58125e273eada44ae28c916b16bc92682409f0610241'
        '8f0e9a845dbd14ee771e3c3fe43110abbccda4598b85ef3ab80b7663df7c7970eb115c8543b32c6949a628a5ede68c'
        '90abd20448c1294ccb6f52c9b85af71e35ee941a')))
    streams.append(('rawok.1024', bytes.fromhex(
        '5301136f6a9a3959a02a12ad0630f06350fdc6f6c4936c95767c2cba0ee8fc40ec600b10a6745e34cc0940532d6ad8'
        'f9ddea275fdff0e43f333488fd7cc9cfec26b7fa37ff95a9760b75d721e4489275a3c8108696e25bdfea07abf77d'
        '599c94224b6b8e6a7e021ba86e01486e20d7fa5eaf1a3243186b1f8cdd4efd1c188b37c7956c1cfa377d1ed23fbe30'
        '5bf4767cb1e0837c07b21e5f0f03f6a2343fef9944d73756cdebfdfba9507b68fe6cba7ac438b7928cdc7339187a81'
        'ce9ae46b83024cdfb421256415e2b475328680c27e6aabf83c68b1446217ca1ba9732232e025545d4bedc8341c37a'
        '2ee5b7ffce668c0b840f0a490ca9014384f93eca67c803dfad7d7454119a4670cd68eef12fde0e9467975d60d5364c'
        'ff1fb37180342c3d50a685c63c8662255029c46969171d0a6b2710fe9a5d43ae044bef8a2d02aaf01c8bc2125de0c'
        'c064a8462bd8a862abe37ca84e7ec4fe4bc4120e7ed0a8020b2893891c42577085a32574891197af660ec19dffa5'
        '992494d265e8beee3b57749f627acea30701fe2a68bd0082c43efec0a3cb54efb4643fa6d47b9dce12caf003691c1d'
        'bc2b0b7050e4688de87e8b2aa3d08a104ab2baa95f73f9585b35089f480bc2021fa640719a1fcd82e979e3a9211592'
        'c93b86c02433d4f41876b2d35fbf9362a1e00805595e6dec1eb8d3710fd5b85aabbe747b154a50205e622e3b0aae07'
        '3e4fec7ca488a5efaa90b0d59475120ddaa2ffa8714ec2da7ccfb99a5d4e7449bdabd3305e2716dfcbb55f45290c29'
        'a1ac3edffddf32119cb002ac77455fb3051a28c1f806796bbab2a9c20d32bdeed3300ba4d677529c27a0102b0c09da'
        '3b533381f5f59baaf861ef4fbfb5048fa7f42896b113ce8c14153d822123c9d7e4b528ba602c313f13a89d5c47afd'
        '0d59d9442fdf7fd4e64106d5223d1894dea9db05100320dfd6efb1497b574c14dc57254a9b53848a8e89d34899d15'
        'ce26cf12986004d73377d9cbac258c1bb669a44900d6ceabc0b34a9ed028b7c4379dc934482775c05b720b5304ef58'
        '6efb23ec2571ab55bdfc7a414a7e55ffb6fbf0b9de94945f2c117863d2ea90fa699377c7394920681a30da63dd1cc1'
        '76d561066b49fdc62b1f7680a1a6d0fbfe036c55996f70f9655e3352f3bf1873fb0860507d7a0da3355d2f63873612'
        '5785b51cd60a00e8ef767a6f3704576171b0916c846671b74e94c55841fe12b28e7f400881b11669f888d54723738c'
        'e0116f04bfbcc18078bd43b6ac843cad6096fb3d5938ef90151f6f25d8adc26b199b81f8b66a86c9c341f210ce733d'
        'cb261df94d3ca27a75ff069ab0c6b6712251c274c4b4d32f2c584322824af2042d2af7f660afecb6defeafa20af35e'
        '1105b09d67a9c3d3d0d004dec2cb89d17d34ae56155e989a6867937eb7967c08e7336a8718fae4a869')))
    return streams


def run_probe(probe, lines):
    result = subprocess.run([str(probe.resolve())], input='\n'.join(lines) + '\n',
                            capture_output=True, text=True, check=True)
    return result.stdout.splitlines()


def probe_bytes(text):
    """'-' means an empty byte string."""
    return b'' if text == '-' else bytes.fromhex(text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compression_dll', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('real_files', nargs='*', type=Path,
                        help='optional real payload files for extra cases')
    args = parser.parse_args()

    data = args.compression_dll.read_bytes()
    if hashlib.sha256(data).hexdigest() != SOURCE_HASH:
        print('check_compression: the DLL does not match the documented SHA-256', file=sys.stderr)
        return 1
    probe_hash = hashlib.sha256(args.probe.read_bytes()).hexdigest()
    original = Original(data)
    corpus = build_corpus(args.real_files)
    corpus.append(('byte-boundary-reset', bytes([150]) * 7))
    invalid = build_invalid_streams()
    # Truncated real compressed streams: cut before the end-of-stream code.
    valid_by_label = dict(corpus)
    for label in ('digits', 'http', 'protocol', 'range'):
        if label in valid_by_label:
            _, packed = original.compress(valid_by_label[label])
            invalid.append(('trunc.%s' % label, packed[:-1]))
            invalid.append(('half.%s' % label, packed[:max(1, len(packed) // 2)]))

    # Original measurements first, then one probe run for all queries.
    queries = ['t']
    expectations = []
    for label, payload in corpus:
        bits, packed = original.compress(payload)
        status, decoded = original.expand(packed)
        if status != 'ok' or decoded != payload:
            raise RuntimeError('original roundtrip failed for %s' % label)
        queries.append('c ' + payload.hex())
        queries.append('e ' + packed.hex())
        expectations.append(('compress', label, payload, bits, packed))
        expectations.append(('decode', label, payload, None, packed))
    for label, stream in invalid:
        status, decoded = original.expand(stream)
        queries.append('e ' + (stream + bytes(EXPAND_PADDING)).hex())
        expectations.append(('invalid', label, decoded, status, stream))

    lines = run_probe(args.probe, queries)
    if len(lines) != 257 + len(expectations):
        raise RuntimeError('Compression probe output count differs from the case count')
    table = []
    for line in lines[:257]:
        symbol, code, bits = line.split()
        table.append((int(symbol), int(code, 16), int(bits)))
    if [symbol for symbol, _, _ in table] != list(range(257)):
        raise RuntimeError('Compression probe code table is incomplete or out of order')
    probe_lines = lines[257:]

    original_table = original.code_table()
    code_mismatches = sum(1 for symbol, code, bits in table if (code, bits) != original_table[symbol])

    mismatches = []
    compared = {'compress': 0, 'decode': 0, 'invalid': 0}
    invalid_outcomes = {'port_ok_original_ok': 0, 'port_err_original_bounded': 0}

    for (kind, label, payload, expected, packed), line in zip(expectations, probe_lines):
        if kind == 'compress':
            compared['compress'] += 1
            fields = line.split()
            if len(fields) != 2 or int(fields[0]) != expected or probe_bytes(fields[1]) != packed:
                mismatches.append({'kind': kind, 'case': label, 'length': len(payload)})
        elif kind == 'decode':
            compared['decode'] += 1
            fields = line.split()
            if fields[0] == 'err' or int(fields[0]) != len(payload) or probe_bytes(fields[1]) != payload:
                mismatches.append({'kind': kind, 'case': label, 'length': len(payload)})
        else:
            compared['invalid'] += 1
            # The port gets the same zero-padded stream as the emulated
            # original. Where the original returns, the results must agree;
            # where it cannot finish (no end-of-stream code in the window or
            # an output overflow) the port must refuse instead of emitting
            # bytes from outside the padding.
            if expected == 'ok':
                fields = line.split()
                if fields[0] == 'err' or probe_bytes(fields[1]) != payload:
                    mismatches.append({'kind': kind, 'case': label, 'original': 'ok'})
                else:
                    invalid_outcomes['port_ok_original_ok'] += 1
            else:
                if line.split()[0] == 'err':
                    invalid_outcomes['port_err_original_bounded'] += 1
                else:
                    mismatches.append({'kind': kind, 'case': label, 'original': expected})

    summary = {
        'source_sha256': SOURCE_HASH,
        'probe_sha256': probe_hash,
        'unicorn_version': unicorn.__version__,
        'root_node': hex(original.root),
        'code_table_entries': len(table),
        'code_table_mismatches': code_mismatches,
        'corpus_cases': len(corpus),
        'invalid_cases': len(invalid),
        'compared': compared,
        'invalid_outcomes': invalid_outcomes,
        'mismatch_count': len(mismatches) + code_mismatches,
        'mismatches': mismatches[:10],
        'scope': 'Original x86 replay, read-only DLL. No socket or file traffic.',
        'limits': ('The emulated expander is given the stream plus %d zero bytes and an '
                   '0x%x byte output window; runs that leave the window are recorded as '
                   'bounded, never as mismatches.' % (EXPAND_PADDING, EXPAND_CAPACITY)),
    }
    print(json.dumps(summary, indent=2))
    return int(summary['mismatch_count'] != 0)


if __name__ == '__main__':
    sys.exit(main())
