#!/usr/bin/env python3
"""Compare the script decoder with original x86 code. Read game files only."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess
import tempfile

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP
from pff import Archive

SHA = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_folder', type=Path)
    parser.add_argument('script_probe', type=Path)
    args = parser.parse_args()
    exe = (args.game_folder / 'Df.exe').read_bytes()
    if hashlib.sha256(exe).hexdigest() != SHA:
        raise ValueError('Unexpected original executable hash')
    pe = pefile.PE(data=exe)
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0x456000, 0x1000)
    cpu.mem_write(0x456000, pe.get_data(0x56000, 0x1000))
    cpu.mem_map(0x2000000, 0x10000)
    cpu.mem_map(0x3000000, 0x100000)
    cpu.mem_map(0x4000000, 0x1000)
    cases = []
    rng = random.Random(1998)
    for size in (2, 3, 4, 15, 16, 17, 255, 256, 257, 4096, 8192):
        cases.append((str(size), rng.randbytes(size)))
    for archive_name in ('Df.pff', 'Dfbase.pff'):
        path = args.game_folder / archive_name
        archive = Archive(path)
        with path.open('rb') as source:
            for entry in archive.entries:
                if not entry.deleted and entry.name.lower().endswith(('.def', '.anm')):
                    source.seek(entry.offset)
                    cases.append((entry.name, source.read(entry.size)))
    total = 0
    with tempfile.TemporaryDirectory(prefix='dfre-script-') as folder:
        for name, data in cases:
            if not 2 <= len(data) <= 0x100000:
                raise ValueError('Replay buffer outside bounds')
            cpu.mem_write(0x3000000, data)
            cpu.mem_write(0x2000800, struct.pack('<4I', 0x4000000, 0x3000000, len(data), 0x04960552))
            cpu.reg_write(UC_X86_REG_ESP, 0x2000800)
            cpu.emu_start(0x456eb0, 0x4000000, count=20*len(data)+100)
            if cpu.reg_read(UC_X86_REG_EIP) != 0x4000000:
                raise AssertionError('Original decoder did not return: ' + name)
            expected = bytes(cpu.mem_read(0x3000000, len(data)))
            source = Path(folder) / 'input.bin'
            source.write_bytes(data)
            actual = subprocess.check_output([str(args.script_probe.resolve()), str(source)], timeout=30)
            if actual != expected:
                raise AssertionError('Script bytes differ: ' + name)
            total += len(data)
    print(json.dumps({'cases': len(cases), 'decoded_bytes': total, 'mismatches': 0, 'source_sha256': SHA}))

if __name__ == '__main__':
    main()
