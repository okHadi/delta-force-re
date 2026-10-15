"""Check the ported terrain ground queries against the original DF1 instructions.

Replays 0x47DF40, 0x47E5D0 and the 0x48775F height-source branch in Unicorn and
compares them with motion_queries_probe. Exits non-zero on a mismatch, a missing
probe line, or an incomplete section.
"""
import argparse,hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile,unicorn
from unicorn.x86_const import *

p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);p.add_argument('probe',type=Path)
p.add_argument('--report',type=Path);p.add_argument('--maps',type=Path,default=Path('.local/opencode/motion-step-work'));args=p.parse_args()
raw=(args.game/'Df.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
if sha!='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff':raise SystemExit('Unsupported original executable hash')
pe=pefile.PE(data=raw);u=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
for at,size in [(0x400000,0xa00000),(0x1000000,0x100000),(0x2000000,0x10000),(0x3000000,0x200000),(0x4000000,0x1000)]:u.mem_map(at,size)
for s in pe.sections:
 if s.PointerToRawData and not s.Characteristics&0x80:u.mem_write(0x400000+s.VirtualAddress,raw[s.PointerToRawData:s.PointerToRawData+s.SizeOfRawData])
def put(at,v):u.mem_write(at,struct.pack('<I',v&0xffffffff))
def word(at):return struct.unpack('<I',u.mem_read(at,4))[0]
def signed(v):return v if v<0x80000000 else v-0x100000000
PROBE=str(args.probe.resolve())
def probe(mode,map_path,rows):
 rows=list(rows)
 out=subprocess.check_output([PROBE,mode,str(map_path)],input=''.join(' '.join(map(str,row))+'\n' for row in rows).encode()).decode().splitlines()
 if len(out)!=len(rows):raise SystemExit(f'{mode}: probe returned {len(out)} lines for {len(rows)} cases')
 return [line.split() for line in out]
state={'stops':set(),'height':0,'entity_height':0,'query_hooks':False}
def hook(m,at,size,user):
 if at in state['stops']:u.emu_stop();return
 if at==0x486fd0 and state['query_hooks']:
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,state['entity_height']);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp));return
 if u.mem_read(at,1)[0] in (0xc3,0xc2):u.emu_stop()
u.hook_add(unicorn.UC_HOOK_CODE,hook)
def make_maps(folder):
 folder.mkdir(parents=True,exist_ok=True)
 random_rows=bytearray()
 generator=random.Random(7)
 for _ in range(1024*1024//4):random_rows+=struct.pack('<I',generator.getrandbits(32))
 maps=[('map-random.bin',bytes(random_rows))]
 stripe=bytearray(1024*1024)
 for y in range(1024):
  for x in range(1024):stripe[y*1024+x]=(x*3+y*5)&0xff
 maps.append(('map-stripe.bin',bytes(stripe)))
 paths=[]
 for name,data in maps:
  path=folder/name
  if not path.exists() or path.read_bytes()!=data:path.write_bytes(data)
  paths.append(path)
 return paths
def map_names(paths):return [p.name for p in paths]
report={'source_sha256':sha,'probe':PROBE,'maps':map_names(make_maps(args.maps)),'checks':{}}
def record(name,cases,checked,mismatch,note=''):
 if cases<=0:raise SystemExit(f'{name}: no cases')
 if checked!=cases:raise SystemExit(f'{name}: checked {checked} of {cases} cases')
 entry={'cases':cases,'checked':checked,'mismatches':mismatch}
 if note:entry['note']=note
 report['checks'][name]=entry
positions=[(0,0),(0xffff,0xffff),(0x10000,0x10000),(0x3ff0000,0x3ff0000),(0x4000000,0x4000000),(0x3ffffff,0x3ffffff),
           (-1,-1),(-0x10000,-0x10000),(0x7fffffff,0x7fffffff),(-0x80000000,-0x80000000),(0x7fff0000,0x80000000),
           (0xf2345678,0x089abcde),(0x100000001&0xffffffff,12345),(12345,0x100000001&0xffffffff)]
for seed in range(40):
 generator=random.Random(1000+seed)
 positions.append((generator.getrandbits(32),generator.getrandbits(32)))
for path in make_maps(args.maps):
 data=path.read_bytes()
 u.mem_write(0x3000000,data)
 put(0xbd5284,0x3000000)   # the original keeps the map pointer at 0xBD5284
 label=path.stem
 native=probe('height',path,[(signed(x),signed(y)) for x,y in positions])
 mismatch=0;checked=0
 for (x,y),line in zip(positions,native):
  u.mem_write(0x2008000,struct.pack('<4I',0x4000000,x&0xffffffff,y&0xffffffff,0))
  u.reg_write(UC_X86_REG_ESP,0x2008000);state['stops']=set()
  u.emu_start(0x47df40,0x4000000,count=20000)
  checked+=1
  if int(line[0])!=signed(u.reg_read(UC_X86_REG_EAX)):
   mismatch+=1
   if mismatch<=4:print('height mismatch',label,hex(x&0xffffffff),hex(y&0xffffffff),'native',line[0],'orig',signed(u.reg_read(UC_X86_REG_EAX)))
 record(f'height_0x47df40_{label}',len(positions),checked,mismatch)
 native=probe('slope',path,[(signed(x),signed(y)) for x,y in positions])
 mismatch=0;checked=0
 for (x,y),line in zip(positions,native):
  u.mem_write(0x2008000,struct.pack('<6I',0x4000000,x&0xffffffff,y&0xffffffff,0x2009000,0x2009004,0))
  u.reg_write(UC_X86_REG_ESP,0x2008000);state['stops']=set()
  u.emu_start(0x47e5d0,0x4000000,count=20000)
  got=(signed(word(0x2009000)),signed(word(0x2009004)))
  checked+=1
  if (int(line[0]),int(line[1]))!=got:mismatch+=1
 record(f'slope_0x47e5d0_{label}',len(positions),checked,mismatch)
rows=[]
for x,y in positions[:20]:
 for has_entity in (0,1):
  for entity_height in (0,0x1234,-0x5678,0x7fffffff,-0x80000000):rows.append((signed(x),signed(y),has_entity,entity_height))
native=probe('actor_height',make_maps(args.maps)[0],rows)
mismatch=0;checked=0
state['query_hooks']=True
# The actor window compares 47DF40 in place (native base_height) against the
# explicit 486FD0 input, so the map pointer must match the probe map.
u.mem_write(0x3000000,make_maps(args.maps)[0].read_bytes());put(0xbd5284,0x3000000)
for (x,y,has_entity,entity_height),line in zip(rows,native):
 u.mem_write(0x1000000,bytes(0x200))
 put(0x1000000+0x108,1 if has_entity else 0)
 state['height']=0x11112222;state['entity_height']=entity_height
 state['stops']={0x487773}
 # 0x48774A window: ebx = actor, [esp] = x, [esp+4] = y
 u.mem_write(0x2008000,struct.pack('<4I',x&0xffffffff,y&0xffffffff,0,0))
 u.reg_write(UC_X86_REG_ESP,0x2008000);u.reg_write(UC_X86_REG_EBX,0x1000000)
 u.emu_start(0x48774a,0x4000000,count=2000)
 got=signed(u.reg_read(UC_X86_REG_EAX))
 checked+=1
 if int(line[0])!=got:
  mismatch+=1
  if mismatch<=4:print('actor mismatch',hex(x&0xffffffff),hex(y&0xffffffff),'entity',has_entity,entity_height,'native',line[0],'orig',got)
state['query_hooks']=False
record('actor_height_0x48775f',len(rows),checked,mismatch,'47DF40 runs in place; only 486FD0 is hooked as an explicit input')
expected={'height_0x47df40_map-random','height_0x47df40_map-stripe','slope_0x47e5d0_map-random','slope_0x47e5d0_map-stripe','actor_height_0x48775f'}
if set(report['checks'])!=expected:raise SystemExit('section set changed: '+repr(sorted(report['checks'])))
report['total_mismatches']=sum(c['mismatches'] for c in report['checks'].values())
report['total_cases']=sum(c['cases'] for c in report['checks'].values())
text=json.dumps(report,indent=2);print(text)
if args.report:args.report.write_text(text+'\n')
sys.exit(1 if report['total_mismatches'] else 0)
