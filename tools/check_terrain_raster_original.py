import hashlib,json,struct,subprocess
from pathlib import Path
import argparse
parser = argparse.ArgumentParser(description="Compare native terrain with original DF1 x86 code.")
parser.add_argument("game", type=Path)
parser.add_argument("probe", type=Path)
parser.add_argument("--report", type=Path)
args = parser.parse_args()
import pefile,unicorn
from unicorn.x86_const import *
ROOT=args.game;source=(ROOT/'Df.exe').read_bytes()
assert hashlib.sha256(source).hexdigest()=='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
pe=pefile.PE(data=source);u=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
for address,size in ((0x456000,4096),(0x459000,4096),(0x2000000,65536),(0x3000000,1048576),(0x3100000,1048576),(0x3500000,4096),(0x3600000,4096),(0x3610000,4096),(0x3620000,4096),(0x3630000,4096),(0x4000000,4096)):
 u.mem_map(address,size)
for address in (0x456000,0x459000):
 off=pe.get_offset_from_rva(address-0x400000);u.mem_write(address,source[off:off+4096])
mask=0xffffffff

def fixture(seed):
 state=seed
 def next():
  nonlocal state
  state=(state*1664525+1013904223)&mask
  return state
 lod=seed%10;stop=7+next()%24;top=(next()%4)*24;scale=65536+(next()&32767)
 delta=((next()%65536-32768)&mask,(next()%65536-32768)&mask,(-(65536+next()%65536))&mask)
 width=1024>>lod
 heights=bytes(next()>>24 for _ in range(width*width));colors=bytes(next()>>24 for _ in heights)
 table=[next() for _ in range(64)];columns=[]
 for i in range(12):
  pos=[next(),next(),(next()&255)<<20]
  step=[(next()%0x1000000-0x800000)&mask,(next()%0x1000000-0x800000)&mask,(next()%0x800000-0x400000)&mask]
  acceleration=[(next()%65536-32768)&mask,(next()%65536-32768)&mask,(-(0x100000+next()%65536))&mask]
  columns.append([0x3620000-4,1 if i%5==0 else 0,15*24+i*2,*pos,*step,*acceleration])
 columns[1][5]=0x11000000;columns[1][8]=0
 return lod,stop,top,scale,delta,heights,colors,table,columns

for seed in range(30):
 lod,stop,top,scale,delta,heights,colors,table,columns=fixture(seed)
 u.mem_write(0x456ff0,struct.pack('<I',0));u.mem_write(0x3000000,heights);u.mem_write(0x3100000,colors)
 u.mem_write(0x3600000,b'\xaa'*384);u.mem_write(0x3610000,struct.pack('<384H',*[0xa55a]*384))
 u.mem_write(0x3620000,struct.pack('<64I',*table));u.mem_write(0x3630000,b''.join(struct.pack('<12I',*c) for c in columns))
 params=[lod,0x3620000+stop*4,0x3000000,0x3100000,0x3600000,0x3610000,24,top,0,0,0,0,0x3630000,12,0,0,*delta,scale]
 u.mem_write(0x3500000,struct.pack('<20I',*params))
 sp=0x2008000;u.mem_write(sp,struct.pack('<II',0x4000000,0x3500000))
 for reg in (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP):u.reg_write(reg,0)
 u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_EFLAGS,2)
 u.emu_start(0x459635,0x4000000,timeout=10000000,count=1000000)
 assert u.reg_read(UC_X86_REG_EIP)==0x4000000,seed
 result=json.loads(subprocess.check_output([str(args.probe.resolve()),'--raster-fixture',str(seed)]))
 assert result['pixels']==bytes(u.mem_read(0x3600000,384)).hex(),('pixels',seed)
 assert result['depths']==list(struct.unpack('<384H',u.mem_read(0x3610000,768))),('depths',seed)
 expected=[]
 for i in range(12):
  values=list(struct.unpack('<12I',u.mem_read(0x3630000+i*48,48)))
  values[0]=(values[0]-0x3620000)//4
  if values[2]>=2**31:values[2]-=2**32
  expected.append(values)
 assert result['columns']==expected,('state',seed,result['columns'],expected)
report={'source_sha256':hashlib.sha256(source).hexdigest(),'procedure':'0x459635','unicorn_version':unicorn.__version__,'cases':30,'columns':360,'lods':list(range(10)),'frame_pixels_compared':30*384,'depth_values_compared':30*384,'mismatches':0,'scope':'original byte-color normal two-pixel-column kernel; state, pixel and depth outputs'}
(args.report or Path('.local/reports/terrain-raster-differential.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
