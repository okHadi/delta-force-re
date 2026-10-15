#!/usr/bin/env python3
"""Compare the rebuilt audio data rules with the original Df.exe code.

Original routines emulated with Unicorn (data only, no device, no playback):

  * 0x494c50  WAV image to internal 16-byte sample record (in place)
  * 0x4b4d3a  voice gain update from the pan table, volume and master volume
  * 0x4947e0  master volume store with its 0xff clamp
  * 0x422bd0  effect play wrapper (sample lookup and driver call stubbed)

The rebuilt code answers through tools/audio_probe.cpp. Real WAV images are
read from Dfbase.pff through .local/reports/Dfbase.pff.manifest.json.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

import pefile
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_EDI, UC_X86_REG_EIP, UC_X86_REG_EBP, UC_X86_REG_ESI, UC_X86_REG_ESP,
)

SOURCE_SHA256 = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
CONVERT = 0x494C50
GAIN = 0x4B4D3A
SET_MASTER = 0x4947E0
PLAY = 0x422BD0
LOOKUP = 0x422A60
DRIVER_PLAY = 0x494DD0
TAG_GLOBAL = 0x4CDFF0
MASTER_GLOBAL = 0x4A2716
PAN_TABLE = 0x4B5639
PAN_BYTES = 512
SOUND_READY = 0xD08940
VOICE_LAYOUT = 0x34
TAG = 0x5A5A5A5A

STACK, VOICE, STOP, NAME, BUFFER = 0x2000000, 0x3000000, 0x4000000, 0x4100000, 0x5000000
BUFFER_SIZE = 0x180000


def read32(data, at):
    return struct.unpack_from('<I', data, at)[0]


def read16(data, at):
    return struct.unpack_from('<H', data, at)[0]


def signed32(value):
    return value - 0x100000000 if value >= 0x80000000 else value


class Original:
    """Runs the four original routines inside one Unicorn machine."""

    def __init__(self, image_bytes):
        pe = pefile.PE(data=image_bytes)
        base = pe.OPTIONAL_HEADER.ImageBase
        self.machine = Uc(UC_ARCH_X86, UC_MODE_32)
        self.machine.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 0xFFF) & ~0xFFF)
        self.machine.mem_write(base, pe.get_memory_mapped_image())
        pe.close()
        for address, size in ((STACK, 0x20000), (VOICE, 0x1000), (STOP, 0x1000),
                              (NAME, 0x1000), (BUFFER, BUFFER_SIZE)):
            self.machine.mem_map(address, size)

    def convert(self, image, tag):
        """0x494c50: run on a copy of `image` and return the whole buffer."""
        machine = self.machine
        size = min(max(len(image), 0x40), BUFFER_SIZE)
        machine.mem_write(BUFFER, image[:size] + bytes(max(0, size - len(image))))
        machine.mem_write(TAG_GLOBAL, struct.pack('<I', tag))
        sp = STACK + 0x10000
        machine.mem_write(sp, struct.pack('<II', STOP, BUFFER))
        for reg in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                    UC_X86_REG_ESI, UC_X86_REG_EDI):
            machine.reg_write(reg, 0)
        machine.reg_write(UC_X86_REG_ESP, sp)
        machine.emu_start(CONVERT, STOP, timeout=60_000_000, count=200_000_000)
        if machine.reg_read(UC_X86_REG_EIP) != STOP:
            raise RuntimeError('Original convert routine did not return')
        return bytes(machine.mem_read(BUFFER, size))

    def gain(self, voice, pan, volume):
        """0x4b4d3a with the voice array entry at VOICE and master in the voice."""
        machine = self.machine
        machine.mem_write(VOICE, bytes(voice))
        machine.mem_write(MASTER_GLOBAL, struct.pack('<I', read32(voice, 0)))
        sp = STACK + 0x10000
        machine.mem_write(sp, struct.pack('<I', STOP))
        for reg, value in ((UC_X86_REG_EBX, pan), (UC_X86_REG_ECX, volume),
                           (UC_X86_REG_EDX, 0), (UC_X86_REG_ESI, 0x11111111),
                           (UC_X86_REG_EBP, 0x22222222), (UC_X86_REG_EAX, 0x33333333),
                           (UC_X86_REG_ESP, sp)):
            machine.reg_write(reg, value)
        machine.reg_write(UC_X86_REG_EDI, VOICE)
        machine.emu_start(GAIN, STOP, timeout=1_000_000, count=1_000_000)
        if machine.reg_read(UC_X86_REG_EIP) != STOP:
            raise RuntimeError('Original gain routine did not return')
        out = bytes(machine.mem_read(VOICE, VOICE_LAYOUT))
        return signed32(read32(out, 0x24)), signed32(read32(out, 0x28))

    def set_master(self, value):
        machine = self.machine
        machine.mem_write(SOUND_READY, struct.pack('<I', 1))
        machine.mem_write(MASTER_GLOBAL, struct.pack('<I', 0xDEADBEEF))
        sp = STACK + 0x10000
        machine.mem_write(sp, struct.pack('<III', STOP, value & 0xFFFFFFFF, 0))
        machine.reg_write(UC_X86_REG_ESP, sp)
        machine.emu_start(SET_MASTER, STOP, timeout=1_000_000, count=1_000_000)
        if machine.reg_read(UC_X86_REG_EIP) != STOP:
            raise RuntimeError('Original master volume routine did not return')
        return read32(machine.mem_read(MASTER_GLOBAL, 4), 0)

    def play(self, volume):
        """0x422bd0 with the sample lookup and driver call replaced by stubs."""
        machine = self.machine
        captured = []

        def finish_call(uc, argument_bytes, value):
            sp = uc.reg_read(UC_X86_REG_ESP)
            address = struct.unpack('<I', uc.mem_read(sp, 4))[0]
            uc.reg_write(UC_X86_REG_ESP, sp + 4 + argument_bytes)
            uc.reg_write(UC_X86_REG_EAX, value)
            uc.reg_write(UC_X86_REG_EIP, address)

        def lookup_stub(uc, address, size, user):
            finish_call(uc, 0, 0x0BADF00D)

        def driver_stub(uc, address, size, user):
            sp = uc.reg_read(UC_X86_REG_ESP)
            arguments = struct.unpack('<4I', uc.mem_read(sp + 4, 16))
            captured.append(arguments)
            finish_call(uc, 0, 0)

        machine.mem_write(NAME, b'beep2.wav\0')
        sp = STACK + 0x10000
        machine.mem_write(sp, struct.pack('<III', STOP, NAME, volume & 0xFFFFFFFF))
        for reg in (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                    UC_X86_REG_ESI, UC_X86_REG_EDI):
            machine.reg_write(reg, 0)
        machine.reg_write(UC_X86_REG_ESP, sp)
        lookup_handle = machine.hook_add(UC_HOOK_CODE, lookup_stub, begin=LOOKUP, end=LOOKUP)
        driver_handle = machine.hook_add(UC_HOOK_CODE, driver_stub,
                                         begin=DRIVER_PLAY, end=DRIVER_PLAY)
        try:
            machine.emu_start(PLAY, STOP, timeout=1_000_000, count=1_000_000)
        finally:
            machine.hook_del(lookup_handle)
            machine.hook_del(driver_handle)
        if machine.reg_read(UC_X86_REG_EIP) != STOP:
            raise RuntimeError('Original play wrapper did not return')
        return captured

    def pan_table(self):
        return bytes(self.machine.mem_read(PAN_TABLE, PAN_BYTES))


def make_wav(samples, rate=22050, bits=16, channels=1):
    if bits == 16:
        payload = b''.join(struct.pack('<h', max(-32768, min(32767, s))) for s in samples)
        block = 2 * channels
    else:
        payload = bytes(max(0, min(255, s)) for s in samples)
        block = channels
    header = b'RIFF' + struct.pack('<I', 36 + len(payload)) + b'WAVE'
    header += b'fmt ' + struct.pack('<IHHIIHH', 16, 1, channels, rate,
                                    rate * block, block, bits)
    header += b'data' + struct.pack('<I', len(payload))
    return header + payload


def synthetic_wavs():
    cases = []
    for rate in (8000, 11025, 22050, 44100, 48000):
        samples = [1000, -1000, 32767, -32768, 0, 7, -7, 12345, -12345, 300, -300]
        cases.append((f'16-bit {rate}', make_wav(samples, rate=rate, bits=16)))
    cases.append(('8-bit ramp', make_wav([(i * 37 + 11) & 0xFF for i in range(256)],
                                         rate=11025, bits=8)))
    cases.append(('8-bit silence', make_wav([128] * 64, rate=22050, bits=8)))
    cases.append(('16-bit tiny', make_wav([1, -1], rate=22050, bits=16)))
    cases.append(('16-bit constant', make_wav([1234] * 33, rate=22050, bits=16)))
    cases.append(('16-bit low bytes', make_wav([0x7F01, -0x8001, 0x00FF], rate=22050, bits=16)))
    return cases


def real_wavs(archive, manifest):
    entries = json.loads(Path(manifest).read_text())['entries']
    names = [e for e in entries if e['name'].upper().endswith('.WAV')]
    data = Path(archive).read_bytes()
    return [(entry['name'], data[entry['offset']:entry['offset'] + entry['size']])
            for entry in names]


def gain_corpus():
    cases = []
    for master in (0, 1, 0x80, 0xFF, 0x1234, 0xFFFF, 0x10000):
        for pan in (-1, 0, 1, 21, 63, 64, 65, 127, 128, 129, 191, 192, 255, 256, 1000):
            for volume in (-1, 0, 1, 5, 127, 200, 255, 256, 4096):
                for shift in (0, 1, 8, 15, 16, 31, 32, 48, 255):
                    cases.append((master, pan, volume, shift))
    return cases


def voice_image(master, pan, volume, shift, step=0x10000000):
    voice = bytearray(VOICE_LAYOUT)
    struct.pack_into('<I', voice, 0x00, master)
    struct.pack_into('<i', voice, 0x14, step)
    struct.pack_into('<i', voice, 0x18, pan)
    struct.pack_into('<i', voice, 0x1c, volume)
    voice[0x20] = shift
    return voice


def record_of(buffer, source, name):
    length = read32(buffer, 4)
    clipped = min(length, len(source), BUFFER_SIZE - 0x10)
    return {
        'tag': read32(buffer, 0),
        'length': length,
        'step': read32(buffer, 8),
        'volume': read16(buffer, 0x0c),
        # The rebuilt record keeps one byte for the source bit depth.
        'bits': (read16(source, 0x22) if len(source) >= 0x24 else 0) & 0xFF,
        'pcm': bytes(buffer[0x10:0x10 + clipped]).hex(),
        # Full converted image, so header writes are compared too.
        'image': bytes(buffer[:0x10 + clipped]).hex(),
        'source': name,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--archive', type=Path, default=None)
    parser.add_argument('--manifest', type=Path, default=None)
    args = parser.parse_args()
    try:
        image_bytes = args.executable.read_bytes()
        digest = hashlib.sha256(image_bytes).hexdigest()
        if digest != SOURCE_SHA256:
            raise ValueError('Expected the documented Df.exe SHA-256')
        original = Original(image_bytes)

        wav_cases = synthetic_wavs()
        if args.archive and args.manifest:
            wav_cases.extend(real_wavs(args.archive, args.manifest))

        wav_expect = []
        probe_lines = []
        for name, wav in wav_cases:
            converted = original.convert(wav, TAG)
            wav_expect.append(record_of(converted, wav, name))
            probe_lines.append(f'wav {TAG:08x} {wav.hex()}')
            again = original.convert(converted, TAG)
            wav_expect.append(record_of(again, converted, name + ' (repeat)'))
            probe_lines.append(f'wav {TAG:08x} {converted.hex()}')

        gain_expect = []
        for master, pan, volume, shift in gain_corpus():
            right, left = original.gain(voice_image(master, pan, volume, shift), pan, volume)
            gain_expect.append((right, left))
            probe_lines.append(f'gain {pan} {volume} {master} {shift}')

        master_values = (0, 1, 255, 256, 0x7FFF, 0xFFFFFFFF, 0x12345678)
        master_expect = [original.set_master(value) for value in master_values]
        probe_lines.extend(f'master {value}' for value in master_values)

        play_values = (-3, -1, 0, 1, 254, 255, 256, 4096)
        play_expect = []
        play_rule_ok = True
        for value in play_values:
            captured = original.play(value)
            if len(captured) != 1:
                raise RuntimeError('Play wrapper did not call the driver once')
            sample, pan, play_volume, rate = captured[0]
            expected_volume = 0 if value < 0 else (0xFF if value > 0xFF else value)
            if sample != 0x0BADF00D or pan != 0 or rate != 0x10000 or play_volume != expected_volume:
                play_rule_ok = False
            play_expect.append(play_volume)
            probe_lines.append(f'clamp {value}')

        pan_expect = original.pan_table().hex()
        probe_lines.append('pan')

        result = subprocess.run([str(args.probe.resolve())],
                                input=''.join(line + '\n' for line in probe_lines),
                                capture_output=True, text=True, check=True)
        lines = result.stdout.splitlines()
        expected_count = len(wav_expect) + len(gain_expect) + len(master_expect) + \
            len(play_expect) + 1
        if len(lines) != expected_count:
            raise ValueError(f'Probe returned {len(lines)} results, expected {expected_count}')

        mismatches = []
        index = 0
        for want in wav_expect:
            got = parse_wav(lines[index])
            index += 1
            if any(got.get(key) != want[key] for key in want if key != 'source'):
                mismatches.append(diff_record(len(mismatches), want, got))
        for want in gain_expect:
            right, left = (int(part) for part in lines[index].split())
            index += 1
            if (right, left) != want:
                mismatches.append({'kind': 'gain', 'case': len(mismatches),
                                   'got': [right, left], 'want': list(want)})
        for want in master_expect:
            got = int(lines[index])
            index += 1
            if got != want:
                mismatches.append({'kind': 'master', 'case': len(mismatches),
                                   'got': got, 'want': want})
        for want in play_expect:
            got = int(lines[index])
            index += 1
            if got != want:
                mismatches.append({'kind': 'play', 'case': len(mismatches),
                                   'got': got, 'want': want})
        table = lines[index].strip()
        if table != pan_expect:
            mismatches.append({'kind': 'pan_table', 'got': table[:64], 'want': pan_expect[:64]})

        print(json.dumps({
            'source_sha256': digest,
            'unicorn_version': unicorn.__version__,
            'procedures': {
                'convert_wav': hex(CONVERT),
                'voice_gain': hex(GAIN),
                'set_master_volume': hex(SET_MASTER),
                'play_wrapper': hex(PLAY),
                'pan_table': hex(PAN_TABLE),
            },
            'wav_source_images': len(wav_cases),
            'wav_cases': len(wav_expect),
            'gain_cases': len(gain_expect),
            'master_cases': len(master_expect),
            'play_cases': len(play_expect),
            'pan_table_bytes': PAN_BYTES,
            'total_probe_cases': expected_count,
            'play_args_rule_ok': play_rule_ok,
            'mismatch_count': len(mismatches),
            'mismatches': mismatches[:5],
            'scope': 'Pure data emulation; no device is opened and nothing is played',
        }, indent=2))
        return int(bool(mismatches) or not play_rule_ok)
    except (OSError, ValueError, RuntimeError, pefile.PEFormatError,
            unicorn.UcError, subprocess.CalledProcessError, KeyError,
            struct.error) as error:
        print(f'check_audio_original: {error}', file=sys.stderr)
        return 1


def parse_wav(line):
    parts = line.split()
    return {
        'tag': int(parts[0], 16),
        'length': int(parts[1]),
        'step': int(parts[2], 16),
        'volume': int(parts[3], 16),
        'bits': int(parts[4], 16),
        'pcm': parts[5],
        'image': parts[6],
    }


def diff_record(case, want, got):
    entry = {'kind': 'wav', 'case': case, 'source': want.get('source')}
    for key in want:
        if key == 'source':
            continue
        if got.get(key) != want[key]:
            entry[key] = {'got': preview(got.get(key)), 'want': preview(want[key])}
    return entry


def preview(value):
    if isinstance(value, str) and len(value) > 32:
        return value[:32] + '...'
    return value


if __name__ == '__main__':
    sys.exit(main())
