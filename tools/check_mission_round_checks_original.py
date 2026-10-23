#!/usr/bin/env python3
"""Compare current round checks with the supplied original instructions."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256
BODIES=[0x444630,0x448590,0x447930,0x4423f0]
REGIONS=[(0x67db00,0x1b00),(0x6e5700,0x200),(0x6e8000,0x1a90),(0x6e9a90,17*0x9b84),(0x8ace80,2*0x114),(0x1200000,0x1000),(0x1210000,0x1000),(0x1300000,4)]
SCALARS=[0xc23820,0x8cf900]
def run(a):
 r=Rig(a.game);active=False;trace=[];v=[];seen=0
 def put(at,width,n):r.u.mem_write(at,(n&((1<<(width*8))-1)).to_bytes(width,'little'))
 def get(at,width=4):return int.from_bytes(r.u.mem_read(at,width),'little')
 def tracked(at):return at in SCALARS or any(base<=at<base+size for base,size in REGIONS)
 def memory(u,kind,at,width,n,_):
  nonlocal seen
  if not active or not tracked(at):return
  if kind==unicorn.UC_MEM_READ:
   if at==v[1]:
    if seen==v[2]:put(at,width,v[3])
    seen+=1
   trace.append(f'R{at},{width},{get(at,width)};')
  else:trace.append(f'W{at},{width},{n&((1<<(8*width))-1)};')
 def foreign(site):
  sp=r.u.reg_read(UC_X86_REG_ESP);argc={0x444ca0:1,0x446da0:0,0x447fb0:1,0x4604c0:1,0x4605d0:1,0x4423f0:3,0x477620:1,0x441750:4}[site];args=list(struct.unpack('<'+'I'*argc,r.u.mem_read(sp+4,4*argc))) if argc else []
  if site==0x4423f0:args[1]=get(args[1],1)
  if site==0x441750:args=[(args[0]-0x6e9a90)//0x9b84,args[1],get(args[2],1),args[3]]
  trace.append('C'+','.join(map(str,[site,*args]))+';')
  if site==v[4]:put(v[5],4,v[6])
  if site!=0x477620:r.finish(0)
 for site in [0x444ca0,0x446da0,0x447fb0,0x4604c0,0x4605d0,0x4423f0,0x441750]:r.stubs[site]=lambda site=site:foreign(site)
 r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 r.u.hook_add(unicorn.UC_HOOK_CODE,lambda u,at,size,_:foreign(at) if active and at==0x477620 else None)
 rng=random.Random(0x444630);rows=[];expected=[]
 def fixture(op,mode,mp=1):
  d={}
  def p(at,n,width=4):d[at]=(width,n&((1<<(width*8))-1))
  for at,n in [(0x67dbc0,0),(0x67f520,mp),(0x67f528,1),(0x67f53c,mode),(0x67f544,10),(0x67f554,3),(0x67dbe8,5),(0xc23820,0x1200000),(0x8cf900,2),(0x6e5884,19),(0x6e5768,20),(0x6e5764,20)]:p(at,n)
  for i in range(3):
   client=0x6e9a90+i*0x9b84;actor=0x1200000+i*0x114;definition=0x1210000+i*0x13c
   for off,n,w in [(8,actor,4),(0x18c,1,1),(0x1c4,5,2),(0x19c,4,4),(0x1a0,10,4),(0x1a8,1,4),(0x1b0,1,4),(0x1c0,599,4),(0x9c,i%2,4)]:p(client+off,n,w)
   for off,n,w in [(4,definition,4),(0x6c,0,1),(0x90,0x1200000+((i+1)%3)*0x114,4),(0x24,i*0x10000,4),(0x28,0,4)]:p(actor+off,n,w)
   p(definition+0x20,0xfc3+i%2)
  for i in range(2):
   marker=0x8ace80+i*0x114;definition=0x1210000+(i+3)*0x13c
   for off,n in [(4,definition),(0x24,i*0x10000),(0x28,0),(0x7c,0x20000)]:p(marker+off,n)
   p(definition+0x20,0x1776)
  p(0x1300000,19,1)
  return d
 for op in range(4):
  for mode in range(7):
   for mp in [0,1]:
    for mutation in range(10):
     d=fixture(op,mode,mp);h=[op,0,0,0,0,0,0]
     if mutation==1:h[1:4]=[0x67dbc0,0,1]
     if mutation==2:h[1:4]=[0x120006c,0,4]
     if mutation==3:h[1:4]=[0x67dbe8,0,0]
     if mutation==4:h[1:4]=[0x67f544,1,0xffffffff]
     if mutation==5:h[4:7]=[0x444ca0,0x67dbe8,0]
     if mutation==6:h[4:7]=[0x446da0,0x6e8144,99]
     if mutation==7:h[4:7]=[0x477620,0x8acefc,0]
     if mutation==8:h[4:7]=[0x4604c0,0x6e9a98+0x9b84,0x1200000]
     if mutation==9:h[1:4]=[0x6e9a98,1,0x1200114]
     rows.append((h,d))
 edges=[0,1,4,5,9,10,20,599,600,0x7fffffff,0x80000000,0xffffffff]
 for _ in range(a.cases):
  op=rng.randrange(4);mode=rng.randrange(7);d=fixture(op,mode,rng.randrange(2));h=[op,0,0,0,0,0,0]
  for at in [0x67dbc0,0x67f544,0x67f554,0x67dbe8,0x6e8144,0x6e8154,0x6e8158,0x6e81ac,0x6e81bc,0x6e81c0,0x6e813c,0x6e81a4,0x6e815c,0x6e81c4]:d[at]=(4,rng.choice(edges))
  d[0x67dbc0]=(4,0 if rng.randrange(8) else rng.choice(edges))
  for i in range(3):
   c=0x6e9a90+i*0x9b84
   for off in [0x19c,0x1a0,0x1a8,0x1b0,0x1b4,0x1c0]:d[c+off]=(4,rng.choice(edges))
   d[0x1200000+i*0x114+0x6c]=(1,rng.choice([0,4]));d[c+0x18c]=(1,rng.randrange(2));d[c+0x1c4]=(2,rng.choice([0,5]))
  if mode in [3,4]:
   for i in range(3):
    for axis in [0x24,0x28]:d[0x1200000+i*0x114+axis]=(4,rng.choice(edges+[0x10000,0xffff0000,0x40000000]))
   for i in range(2):
    for off in [0x24,0x28,0x7c]:d[0x8ace80+i*0x114+off]=(4,rng.choice(edges+[0x20000,0x7fff0000]))
  rows.append((h,d))
 for mutation in [0,1,2]:
  d=fixture(3,0);h=[3,0,0,0,0x441750,0x1300000,77] if mutation==1 else [3,0,0,0,0x441750,0x6e9c1c+2*0x9b84,1] if mutation==2 else [3,0,0,0,0,0,0];rows.append((h,d))
 for h,d in rows:
  for base,size in REGIONS:r.u.mem_write(base,b'\0'*size)
  for at in SCALARS:put(at,4,0)
  for at,(width,n) in d.items():put(at,width,n)
  if h[0]==3:r.stubs.pop(0x4423f0,None)
  else:r.stubs[0x4423f0]=lambda:foreign(0x4423f0)
  v=h;seen=0;trace=[];active=True
  try:r.call(BODIES[h[0]],0x30,0x1300000,1) if h[0]==3 else r.call(BODIES[h[0]])
  finally:active=False
  expected.append(''.join(trace))
 payload=[' '.join(map(str,[*h,len(d),*[word for at,(width,n) in d.items() for word in [at,width,n]]])) for h,d in rows]
 p=subprocess.run([str(a.probe)],input='\n'.join(payload)+'\n',capture_output=True,text=True);assert p.returncode==0,p.stderr
 actual=p.stdout.splitlines();assert len(actual)==len(expected);diff=[{'case':i,'input':payload[i],'actual':x,'expected':y} for i,(x,y) in enumerate(zip(actual,expected)) if x!=y];report={'source_sha256':SOURCE_SHA256,'bodies':[hex(x) for x in BODIES],'cases':len(rows),'differences':len(diff),'mismatches':diff[:3],'audio_output':'none'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not diff
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
