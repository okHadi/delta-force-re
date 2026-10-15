"""Compare stock mission palettes and gamma mapping with original DF1 x86 code."""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path

import pefile
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP
from mission import Resolver, terrain_name
from pcx import decode

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('game', type=Path)
parser.add_argument('probe', type=Path)
parser.add_argument('--report', type=Path, default=Path('.local/reports/terrain-palette-assets-differential.json'))
args = parser.parse_args()
source = (args.game / 'Df.exe').read_bytes()
source_hash = hashlib.sha256(source).hexdigest()
assert source_hash == 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
gamma = (args.game / 'INTRO.KDV').read_bytes()[:256]
assert len(gamma) == 256
pe = pefile.PE(data=source)
u = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
for address, size in ((0x47e000, 4096), (0x500000, 4096), (0xbfb000, 8192), (0xc22000, 4096),
                      (0x2000000, 65536), (0x3000000, 4096), (0x4000000, 4096)):
    u.mem_map(address, size)
u.mem_write(0x47e000, pe.get_data(0x7e000, 4096))
u.mem_write(0x3000000, gamma)
u.mem_write(0x500b0c, struct.pack('<I', 0x3000000))
def run(address, *parameters):
    u.mem_write(0x2008000, struct.pack('<' + 'I' * (len(parameters) + 1), 0x4000000, *parameters))
    u.reg_write(UC_X86_REG_ESP, 0x2008000)
    u.emu_start(address, 0x4000000, count=10000)
    assert u.reg_read(UC_X86_REG_EIP) == 0x4000000

def fingerprint(data):
    value = 0xcbf29ce484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return f'{value:016x}'

resolver = Resolver(args.game)
expected = {}
night = 0
for archive in resolver.archives:
    for entry in archive.entries:
        if entry.deleted or not entry.name.lower().endswith('.bms'):
            continue
        mission, _ = resolver.read(archive, entry)
        name = 'NIGHT.PAL' if mission[122] & 16 else entry.name.split('.', 1)[0] + '.PAL'
        night += bool(mission[122] & 16)
        palette_bytes, info = resolver.read(*resolver.find(name))
        palette = decode(palette_bytes).palette
        assert palette is not None and len(palette) == 768
        trn, _ = resolver.read(*resolver.find(terrain_name(mission)))
        fields = [line.split('//', 1)[0].split() for line in trn.decode('ascii').splitlines()]
        sky_names = [words[1] + '.PCX' for words in fields if words and words[0] == 'sky_palette']
        assert len(sky_names) == 1
        sky_bytes, _ = resolver.read(*resolver.find(sky_names[0]))
        sky_palette = decode(sky_bytes).palette
        assert sky_palette is not None
        for i in range(256):
            run(0x47e6d0, 0, i, *palette[i * 3:i * 3 + 3])
        run(0x47ebc0, *sky_palette[:3])
        run(0x47eb70, *sky_palette[48:51])
        expected[entry.name] = dict(mission=entry.name, member=info['member'],
            raw=fingerprint(u.mem_read(0xbfbb00, 768)), gamma=fingerprint(u.mem_read(0xbfbe00, 768)),
            fog=list(u.mem_read(0xc22ad8, 3)), light=list(u.mem_read(0xc22ad0, 3)))
actual = json.loads(subprocess.check_output([str(args.probe.resolve()), '--palette-assets', str(args.game)]))
assert {row['mission']: row for row in actual['missions']} == expected
report = dict(source_sha256=source_hash, gamma_sha256=hashlib.sha256(gamma).hexdigest(),
              procedures=['0x47e6d0', '0x47eb70', '0x47ebc0'], selection='0x402940',
              unicorn_version=unicorn.__version__, missions=len(expected), night_missions=night,
              palette_bytes=len(expected) * 1536, mismatches=0, results=list(expected.values()))
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({key: value for key, value in report.items() if key != 'results'}))
