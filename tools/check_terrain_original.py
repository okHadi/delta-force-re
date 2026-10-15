import hashlib,io,json,random,struct,subprocess,sys
from pathlib import Path
import argparse
parser = argparse.ArgumentParser(description="Compare native terrain with original DF1 x86 code.")
parser.add_argument("game", type=Path)
parser.add_argument("probe", type=Path)
parser.add_argument("--report", type=Path)
args = parser.parse_args()
import pefile,unicorn
from unicorn.x86_const import *
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parent))
from mission import Resolver,terrain_name
ROOT=args.game
PROBE=args.probe.resolve()
source=(ROOT/'Df.exe').read_bytes()
assert hashlib.sha256(source).hexdigest()=='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
pe=pefile.PE(data=source)
u=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
for address,size in ((0x47a000,4096),(0x47d000,8192),(0xbd5000,4096),(0x2000000,65536),(0x3000000,1048576),(0x3100000,1048576),(0x3200000,262144),(0x3300000,4096),(0x3400000,1048576),(0x4000000,4096)):
 u.mem_map(address,size)
for address,size in ((0x47a000,4096),(0x47d000,8192)):
 off=pe.get_offset_from_rva(address-0x400000);u.mem_write(address,source[off:off+size])
u.mem_write(0xbd5284,struct.pack('<I',0x3000000));u.mem_write(0xbd5330,struct.pack('<I',0x3100000));u.mem_write(0xbd531c,struct.pack('<I',0x3200000))

def signed(value):return value if value<0x80000000 else value-0x100000000

def run(fn,*args,count=5000):
 sp=0x2008000
 u.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),0x4000000,*[a&0xffffffff for a in args]))
 for reg in (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP):u.reg_write(reg,0)
 u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_EFLAGS,2)
 u.emu_start(fn,0x4000000,timeout=10000000,count=count)
 assert u.reg_read(UC_X86_REG_EIP)==0x4000000,hex(fn)
 return signed(u.reg_read(UC_X86_REG_EAX))

def compare(heights,characters,detail,points,args):
 u.mem_write(0x3000000,heights);u.mem_write(0x3100000,characters);u.mem_write(0x3200000,detail)
 output=subprocess.check_output([str(PROBE),*args],input=''.join(f'{x} {y}\n' for x,y in points).encode()).decode().splitlines()
 assert len(output)==len(points)
 for (x,y),row in zip(points,output):
  base=run(0x47df40,x,y);surface=run(0x47e000,x,y)
  run(0x47e5d0,x,y,0x3300000,0x3300004)
  slopes=struct.unpack('<2i',u.mem_read(0x3300000,8))
  nearest=heights[((y>>16)&1023)*1024+((x>>16)&1023)]*16384
  actual=tuple(map(int,row.split()));expected=(base,surface,nearest,*slopes)
  assert actual==expected,(args,x,y,actual,expected)

rng=random.Random(0xdf1a)
edges=(-2147483648,2147483647,-1,0,1,4095,4096,65535,65536,-65536,67043327,67108864)
points=[(x,y) for x in edges for y in edges]+[(rng.randrange(-2**31,2**31),rng.randrange(-2**31,2**31)) for _ in range(2048)]
heights=bytes((x*13+y*7+(x^y))&255 for y in range(1024) for x in range(1024))
characters=bytes((x+y*3)&255 for y in range(1024) for x in range(1024))
detail=struct.pack('<65536I',*[((tile*17+at*3)&255)<<16 for tile in range(256) for at in range(256)])
compare(heights,characters,detail,points,['--synthetic'])
# Asymmetric rows establish the original loader's flip direction.
rows=bytes(range(32));u.mem_write(0x3400000,rows);run(0x47a9f0,0x3400000,8,32)
assert bytes(u.mem_read(0x3400000,32))==b''.join(rows[y*8:(y+1)*8] for y in reversed(range(4)))
# The original max-height and rounded sky/water mip kernels.
rows=bytes(range(64));u.mem_write(0x3400000,rows)
for fn,expected in [(0x47d620,bytes(max(rows[y*8+x],rows[y*8+x+1],rows[(y+1)*8+x],rows[(y+1)*8+x+1]) for y in range(0,8,2) for x in range(0,8,2))),(0x47d920,bytes(max(0,((rows[y*8+x]+rows[y*8+x+1]+rows[(y+1)*8+x]+rows[(y+1)*8+x+1]+2)>>2)-1) for y in range(0,8,2) for x in range(0,8,2)))]:
 run(fn,0x3400000,0x3300000,8);assert bytes(u.mem_read(0x3300000,16))==expected
r=Resolver(ROOT);cache={}
def image(name):
 a,e=r.find(name)
 key=(a.path.name,e.name)
 if key not in cache:
  data,_=r.read(a,e);im=Image.open(io.BytesIO(data));im.load();cache[key]=im.copy()
 return cache[key]
raw=image('SNIP_D.PCX').tobytes()
flipped=b''.join(raw[tile*256+y*16:tile*256+(y+1)*16] for tile in range(256) for y in reversed(range(16)))
detail=struct.pack('<65536I',*[v<<16 for v in flipped])
real_points=[(x,y) for x,y in zip(edges,reversed(edges))]+[(rng.randrange(-2**31,2**31),rng.randrange(-2**31,2**31)) for _ in range(48)]
missions=[]
for archive in r.archives:
 for entry in archive.entries:
  if entry.deleted or not entry.name.lower().endswith('.bms'):continue
  data,_=r.read(archive,entry);trn,_=r.read(*r.find(terrain_name(data)));fields={}
  for line in trn.decode('ascii').splitlines():
   words=line.split()
   if words and words[0] in ('elev_map','detail_map'):fields[words[0]]=words[1]+'.PCX'
  heights=image(fields['elev_map']).transpose(Image.Transpose.FLIP_TOP_BOTTOM).tobytes()
  characters=image(fields['detail_map']).transpose(Image.Transpose.FLIP_TOP_BOTTOM).tobytes()
  compare(heights,characters,detail,real_points,[str(ROOT),entry.name]);missions.append(entry.name)
report={'source_sha256':hashlib.sha256(source).hexdigest(),'unicorn_version':unicorn.__version__,'synthetic_cases':len(points),'missions':len(missions),'real_map_cases':len(missions)*len(real_points),'samplers':['0x47df40','0x47e000','0x47e5d0'],'orientation_kernel':'0x47a9f0','mip_kernels':['0x47d620','0x47d920'],'mismatches':0}
(args.report or Path('.local/reports/terrain-differential.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
