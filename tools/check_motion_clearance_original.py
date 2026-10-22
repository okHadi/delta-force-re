"""Check the ported ground clearance scan and entity query against the original.

Replays 0x4876E0-0x487734 (clearance scan) and 0x486FD0 (entity ground query)
in Unicorn with the mesh geometry helpers hooked as explicit inputs, and
compares them with motion_clearance_probe. Exits non-zero on a mismatch, a
missing probe line, an unexpected stop address, or an incomplete section.
"""
import argparse,hashlib,json,random,struct,subprocess,sys
from pathlib import Path
import pefile,unicorn
from unicorn.x86_const import *

p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path)
args=p.parse_args()
raw=(args.game/'Df.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
if sha!='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff':raise SystemExit('Unsupported original executable hash')
pe=pefile.PE(data=raw);u=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
for at,size in [(0x400000,0xa00000),(0x1000000,0x100000),(0x1f00000,0x1000),(0x2000000,0x10000),(0x4000000,0x1000)]:u.mem_map(at,size)
u.mem_write(0x1f00000,bytes([0xc3]))  # callback stub; its call is handled by the hook
for s in pe.sections:
 if s.PointerToRawData and not s.Characteristics&0x80:u.mem_write(0x400000+s.VirtualAddress,raw[s.PointerToRawData:s.PointerToRawData+s.SizeOfRawData])
def put(at,v):u.mem_write(at,struct.pack('<I',v&0xffffffff))
def putb(at,v):u.mem_write(at,struct.pack('<B',v&0xff))
def word(at):return struct.unpack('<I',u.mem_read(at,4))[0]
def signed(v):return v if v<0x80000000 else v-0x100000000
sg=signed
PROBE=str(args.probe.resolve())
def probe(mode,rows):
 rows=list(rows)
 out=subprocess.check_output([PROBE,mode],input=''.join(' '.join(map(str,row))+'\n' for row in rows).encode()).decode().splitlines()
 if len(out)!=len(rows):raise SystemExit(f'{mode}: probe returned {len(out)} lines for {len(rows)} cases')
 return [line.split() for line in out]
STUB=0x1f00000;INSTANCE=0x1090000
state={'stops':set(),'terrain':0,'points':{},'heights':{},'verdicts':{},'last_volume':0,
       'count':0,'eip':0,'calls':0,'instance_call':0,'instance_flags':[]}
def hook(m,at,size,user):
 state['count']+=1
 if at in state['stops']:
  state['eip']=at;u.emu_stop();return
 if at==0x47df40:
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,state['terrain']);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp));return
 if at==STUB:   # definition callback at +0x4C; returns the entity's instance records
  sp=u.reg_read(UC_X86_REG_ESP);entity=word(sp+8)
  index=(entity-ENTITY)//0x200 if ENTITY<=entity<ENTITY+0x1000 else 0
  u.reg_write(UC_X86_REG_EAX,INSTANCE+index*0x100)
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp));return
 if at==0x45d170:
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp));return
 if at==0x45ce60:
  sp=u.reg_read(UC_X86_REG_ESP);out=word(sp+12);instance=word(sp+4)
  point=state['points'].get(instance,(0,0))
  u.mem_write(out,struct.pack('<2i',point[0],point[1]))
  state['calls']+=1
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp));return
 if at==0x42d2a0:
  sp=u.reg_read(UC_X86_REG_ESP);volume=word(sp+8);state['last_volume']=volume;state['calls']+=1
  u.reg_write(UC_X86_REG_EAX,state['verdicts'].get(volume,0))
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp));return
 if at==0x45cda0:
  sp=u.reg_read(UC_X86_REG_ESP);out=word(sp+12);state['calls']+=1
  u.mem_write(out+8,struct.pack('<i',state['heights'].get(state['last_volume'],0)))
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp));return
 if 0x486fd0<=at<0x487400 and u.mem_read(at,1)[0] in (0xc3,0xc2):
  state['eip']=at;u.emu_stop()
u.hook_add(unicorn.UC_HOOK_CODE,hook)
report={'source_sha256':sha,'probe':PROBE,'checks':{}}
def record(name,cases,checked,mismatch,note=''):
 if cases<=0:raise SystemExit(f'{name}: no cases')
 if checked!=cases:raise SystemExit(f'{name}: checked {checked} of {cases} cases')
 entry={'cases':cases,'checked':checked,'mismatches':mismatch}
 if note:entry['note']=note
 report['checks'][name]=entry
# 0x4876E0 scan: [esp+0x3c] instance, [esp+0x40] frame, [esp+0x44] definition, [esp+0x48] 0x1D2B0.
SCAN_STOP=0x487734
cases=[]
for count in (0,1,2,3,8,16,17,33):
 for mask in (0,0xffff0000,0xffffffff,0x30000,0x80000000,1):
  for pattern in (0,1,2):
   base={0:1000,1:0x100000,2:-0x40000000}[pattern]
   wrap=lambda v:(v+0x80000000)%0x100000000-0x80000000
   poses=[(wrap(base+i*17),wrap(base*2-i*13),wrap(-base+i)) for i in range(count)]
   cases.append((count,mask,poses))
native=probe('clearance',[(c[0],c[1],*[v for pose in c[2] for v in pose]) for c in cases])
mismatch=0;checked=0
for (count,mask,poses),line in zip(cases,native):
 u.mem_write(0x1000000,bytes(0x400))
 put(0x1000000+0x8c,0x1010000);u.mem_write(0x1010000,bytes(0x1000))
 for i,pose in enumerate(poses):u.mem_write(0x1010000+i*12,struct.pack('<3i',*pose))
 put(0x1020000+0x8,count);put(0x1030000+0x4,mask)
 sp=0x2008000
 u.mem_write(sp,bytes(0x100))
 put(sp+0x3c,0x1000000);put(sp+0x40,0x1030000);put(sp+0x44,0x1020000);put(sp+0x48,0x1d2b0)
 u.reg_write(UC_X86_REG_ESP,sp)
 state['stops']={SCAN_STOP};state['count']=0;state['eip']=0
 u.emu_start(0x4876e0,0x4000000,count=20000)
 if state['eip']!=SCAN_STOP:raise SystemExit(f'clearance scan stopped at {state["eip"]:#x}')
 if state['count']<=0 or state['count']>20000:raise SystemExit('clearance scan instruction count out of range')
 got=signed(u.reg_read(UC_X86_REG_EBP))
 checked+=1
 if int(line[0])!=got:mismatch+=1
record('clearance_0x4876e0',len(cases),checked,mismatch,'EBP read at 0x487734; EIP and instruction count asserted')
# 0x486FD0 entity query.
ACTOR=0x1000000;POS=0x1001000;ENTITY_LIST=0x1010000;ENTITY=0x1020000;DEFINITION=0x1040000;RUNTIME=0x1050000
PARTS=0x1060000;VOLUMES=0x1070000;MATERIAL=0x1080000;VERTICES=0x10a0000
def build_case(case):
 terrain,px,py,pz,depth,entities=case
 u.mem_write(ACTOR,bytes(0x200))
 put(ACTOR+0x104,ENTITY_LIST);put(ACTOR+0x108,len(entities));put(ACTOR+0x100,0xdeadbeef)
 u.mem_write(POS,struct.pack('<3i',px,py,pz))
 u.mem_write(ENTITY_LIST,bytes(0x100));u.mem_write(ENTITY,bytes(0x1000))
 u.mem_write(DEFINITION,bytes(0x2000));u.mem_write(RUNTIME,bytes(0x100));u.mem_write(PARTS,bytes(0x4000))
 u.mem_write(VOLUMES,bytes(0x8000));u.mem_write(MATERIAL,bytes(0x2000));u.mem_write(INSTANCE,bytes(0x1000))
 u.mem_write(VERTICES,bytes(0x1000))
 for v in range(64):   # 8-byte vertex records: word at +2 keeps the normal count in its high half
  put(VERTICES+v*8,0x01000100);put(VERTICES+v*8+2,0x00400040)
 addresses={}
 for e,entity in enumerate(entities):
  flags,has_definition,radius,has_alternate,alternate_radius,ex,ey,instances=entity
  put(ENTITY_LIST+e*4,ENTITY+e*0x200)
  put(ENTITY+e*0x200+0x6c,flags);put(ENTITY+e*0x200+0x24,ex);put(ENTITY+e*0x200+0x28,ey)
  definition=DEFINITION+e*0x400
  if has_definition:put(ENTITY+e*0x200+0x10,definition);put(definition+0x64,radius)
  if has_alternate:put(ENTITY+e*0x200+0x14,definition+0x200);put(definition+0x264,alternate_radius)
  pieces=entity[7]
  for target in (definition,definition+0x200):
   put(target+0x4c,STUB)
   put(target+0x50,RUNTIME+e*0x40)
   put(RUNTIME+e*0x40+0x98,len(pieces));put(RUNTIME+e*0x40+0x9c,PARTS+e*0x400)
  for k,(piece_flags,bounds,volumes) in enumerate(pieces):
   putb(INSTANCE+e*0x100+k*0x40+0x3c,piece_flags)
   part=PARTS+e*0x400+k*0x70
   put(part+0x40,bounds[0]);put(part+0x44,bounds[1]);put(part+0x48,bounds[2]);put(part+0x4c,bounds[3])
   put(part+0x18,VERTICES)
   put(part+0xc,len(volumes))
   put(part+0x10,VOLUMES+e*0x800+k*0x200)
   for v,(skip,volume_bounds,accepted,height) in enumerate(volumes):
    volume=VOLUMES+e*0x800+k*0x200+v*0x48
    put(volume+0,(v%8)<<16);put(volume+4,0x00400000)
    put(volume+0x2c,volume_bounds[0]);put(volume+0x30,volume_bounds[1])
    put(volume+0x34,volume_bounds[2]);put(volume+0x38,volume_bounds[3])
    material=MATERIAL+((e*7+k*3+v*2)&0x1ff)*0x10
    put(volume+0x44,material);putb(material+0x13,1 if skip else 0)
    addresses[(e,k,v)]=volume
 return addresses
def row_for(case,points,heights,verdicts):
 terrain,px,py,pz,depth,entities=case
 row=[terrain,px,py,pz,depth,len(entities)]
 for e,entity in enumerate(entities):
  flags,has_definition,radius,has_alternate,alternate_radius,ex,ey,instances=entity
  row+=[flags,1 if has_definition else 0,radius,1 if has_alternate else 0,alternate_radius,ex,ey,len(instances)]
  for k,(piece_flags,bounds,volumes) in enumerate(instances):
   row+=[piece_flags,points[(e,k)][0],points[(e,k)][1],bounds[0],bounds[1],bounds[2],bounds[3],len(volumes)]
   for v,volume in enumerate(volumes):
    row+=[1 if volume[0] else 0,volume[1][0],volume[1][1],volume[1][2],volume[1][3],verdicts[(e,k,v)],heights[(e,k,v)]]
 return row
def make_case(index):
 generator=random.Random(2000+index)
 entities=[]
 for _e in range(generator.choice([0,1,2,3])):
  radius=generator.choice([0,10,100,0x10000,0x100000])
  pieces=[]
  for _i in range(generator.choice([0,1,2])):
    xs=sorted([generator.choice([-0x10000,0,0x10000,0x40000]),generator.choice([-0x8000,0,0x8000,0x40000])])
    ys=sorted([generator.choice([-0x10000,0,0x10000,0x40000]),generator.choice([-0x8000,0,0x8000,0x40000])])
    bounds=[xs[0],xs[1],ys[0],ys[1]]
    volumes=[]
    for _v in range(generator.choice([1,2])):
     vx=sorted([generator.choice([-0x10000,0,0x10000,0x40000]),generator.choice([-0x8000,0,0x8000,0x40000])])
     vy=sorted([generator.choice([-0x10000,0,0x10000,0x40000]),generator.choice([-0x8000,0,0x8000,0x40000])])
     volume_bounds=[vx[0],vx[1],vy[0],vy[1]]
     volumes.append((generator.random()<0.3,volume_bounds,generator.random()<0.6,
                     generator.choice([0,1,1000,0x10000,0x7fffffff,-0x80000000])))
    pieces.append((generator.choice([0,1,2,3]),bounds,volumes))
  entities.append((generator.choice([0,8,0x1008]),generator.random()<0.9,radius,generator.random()<0.7,radius,0,0,pieces))
 return (generator.choice([0,1000,0x7fffffff,-0x80000000]),generator.choice([0,100,0x7fffffff,-0x80000000]),
         generator.choice([0,100,0x7fffffff,-0x80000000]),generator.choice([0,0x7fffffff,-0x80000000]),
         generator.choice([0,0x8000,0x7fffffff]),entities)
cases=[make_case(index) for index in range(60)]
# Aligned cases keep the query inside the radius, bounds and depth gates so the
# accept path and the actor+0x100 update are exercised, not only the filters.
for index in range(40):
 generator=random.Random(5000+index)
 pieces=[]
 for _k in range(generator.choice([1,2,3])):
  volumes=[]
  for _v in range(generator.choice([1,2,3])):
   volumes.append((1 if generator.random()<0.15 else 0,
                   [-0x10000,0x10000,-0x10000,0x10000],
                   1 if generator.random()<0.8 else 0,
                   generator.choice([0,1000,0x10000,-1000,0x7fffffff])))
  pieces.append((generator.choice([0,0,0,1,2,3]),[-0x10000,0x10000,-0x10000,0x10000],volumes))
 entities=[]
 for _e in range(generator.choice([1,2])):
  entities.append((generator.choice([0,8]),1,0x100000,generator.random()<0.5,0x100000,0,0,pieces))
 cases.append((generator.choice([0,100]),(0),0,generator.choice([0,1000]),0x7fffffff,entities))
plans=[]
for index,case in enumerate(cases):
 generator=random.Random(3000+index)
 aligned=index>=60
 points={};heights={};verdicts={}
 for e,entity in enumerate(case[5]):
  for k,(piece_flags,bounds,volumes) in enumerate(entity[7]):
   if aligned:
    points[(e,k)]=(generator.choice([0,100,-100]),generator.choice([0,100,-100]))
   else:
    points[(e,k)]=(generator.choice([0,100,0x7fffffff,-0x80000000]),generator.choice([0,-100,0x7fffffff,-0x80000000]))
   for v,volume in enumerate(volumes):
    if aligned:
     heights[(e,k,v)]=generator.choice([0,1000,0x10000,0x7fffffff])
     verdicts[(e,k,v)]=1 if generator.random()<0.8 else 0
    else:
     heights[(e,k,v)]=generator.choice([0,1,1000,0x10000,0x7fffffff,-0x80000000])
     verdicts[(e,k,v)]=1 if generator.random()<0.5 else 0
 plans.append((points,heights,verdicts))
native=probe('entity',[row_for(case,*plan) for case,plan in zip(cases,plans)])
mismatch=0;checked=0;hits=0
for index,((case,plan),line) in enumerate(zip(zip(cases,plans),native)):
 terrain,px,py,pz,depth,entities=case
 points,heights,verdicts=plan
 addresses=build_case(case)
 state['terrain']=terrain
 state['heights']={addresses[key]:heights[key] for key in heights}
 state['verdicts']={addresses[key]:verdicts[key] for key in verdicts}
 state['points']={INSTANCE+e*0x100+k*0x40:points[(e,k)] for (e,k) in points}
 state['last_volume']=0;state['calls']=0;state['count']=0;state['eip']=0;state['stops']=set()
 u.mem_write(0x2008000,struct.pack('<5I',0x4000000,ACTOR,POS,depth,0))
 u.reg_write(UC_X86_REG_ESP,0x2008000)
 try:
  u.emu_start(0x486fd0,0x4000000,count=200000)
 except Exception as error:
  raise SystemExit(f'entity case {index}: {error} eip={u.reg_read(UC_X86_REG_EIP):#x}') from error
 if state['eip']==0:raise SystemExit('entity query did not reach a return instruction')
 if state['count']<=0 or state['count']>200000:raise SystemExit('entity query instruction count out of range')
 got_height=signed(u.reg_read(UC_X86_REG_EAX))
 got_entity=word(ACTOR+0x100)
 got_index=-1
 for e in range(len(entities)):
  if got_entity==ENTITY+e*0x200:got_index=e
 checked+=1
 if int(line[1])!=-1:hits+=1
 if (int(line[0]),int(line[1]))!=(got_height,got_index):
  mismatch+=1
  if mismatch<=3:print('entity mismatch case',index,'native',line,'orig',got_height,got_index)
if hits==0:raise SystemExit('entity section never accepted an entity')
record('entity_0x486fd0',len(cases),checked,mismatch,
       f'0x45D170/0x45CE60/0x45CDA0/0x42D2A0 hooked from the same plan the probe receives; {hits} cases accepted an entity')
expected={'clearance_0x4876e0','entity_0x486fd0'}
if set(report['checks'])!=expected:raise SystemExit('section set changed: '+repr(sorted(report['checks'])))
report['total_mismatches']=sum(c['mismatches'] for c in report['checks'].values())
report['total_cases']=sum(c['cases'] for c in report['checks'].values())
text=json.dumps(report,indent=2);print(text)
if args.report:args.report.write_text(text+'\n')
sys.exit(1 if report['total_mismatches'] else 0)
