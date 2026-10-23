#!/usr/bin/env python3
"""Compare 43F830 instructions, source access order and current client bytes."""
import argparse,json,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
from check_projectiles_original import Rig,SOURCE_SHA256
SCRATCH=0x2007f60
COUNTS={0x493a80:3,0x4564b0:1,0x49a620:3,0x442450:1,0x45da30:1,0x4423f0:3,0x497870:1,0x45d520:2,0x4406e0:2,0x43f610:1,0x404cf0:2,0x43f770:2}
class FixtureFault(Exception):pass
REGIONS=[(0x6e9a90,17*0x9b84),(SCRATCH,0x200),(0x7cca80,0x114),(0x1200000,0x100),(0x1300000,0x3000)]
def fixture(seed):
 d={}
 def put(at,value,width=4):d[at]=(width,value&((1<<(width*8))-1))
 def text(at,value):
  for i,n in enumerate(value+b'\0'):put(at+i,n,1)
 for at,value in [(0x501344,77),(0x50136c,0 if seed&4096 else 0x1301000),(0x67f528,0 if seed&2048 else 1),(0x67f530,[0,1,2,3,17,0xffffffff][(seed>>13)%6]),(0x6e5588,seed&1),(0xad3ed4,0x1300000)]:put(at,value)
 # Default short fixtures take a populated host path.
 if seed<8192:put(0x67f530,3)
 for i in range(17):
  base=0x6e9a90+i*0x9b84;put(base,77 if i==1 and seed&1 else 10+i);put(base+8,0x7cca80 if i==1 and seed&1 else 0);put(base+0x94,5 if i==1 else 8);put(base+0x18c,0 if i in [1,2] else 1,1)
 put(0x7cca80+0x0c,0x1200000)
 for i in range(32):
  for j in range(5):put(0x1300000+i*20+j*4,100*i+j)
 put(0x1300000+20+16,3);put(0x1300000+40+16,4);put(0x1300000+60+16,2)
 for i,n in enumerate([0x82,1,2,3,2]):put(0x1301000+i,n,1)
 text(0x4cc160,b'unknown');text(0x1302000,b'new player');text(0x1302100,b'%s joined');text(0x1302200,b'new player joined')
 return d
def run(a):
 rig=Rig(a.game);u=rig.u;active=False;trace=[];seed=0
 def get(at,width=4):return int.from_bytes(u.mem_read(at,width),'little')
 def put(at,width,value):u.mem_write(at,(value&((1<<(width*8))-1)).to_bytes(width,'little'))
 def text(at,value):u.mem_write(at,value+b'\0')
 def memory(m,kind,at,width,value,_):
  ip=m.reg_read(UC_X86_REG_EIP)
  if not active or not 0x43f830<=ip<=0x43fd55:return
  if 0x2000000<=at<0x2010000 and not (SCRATCH<=at and at+width<=SCRATCH+0x90):return
  trace.append(f'{"R" if kind==unicorn.UC_MEM_READ else "W"}{at},{width},{get(at,width) if kind==unicorn.UC_MEM_READ else value&((1<<(8*width))-1)};')
 diagnostics=0
 def foreign(site):
  nonlocal diagnostics
  sp=u.reg_read(UC_X86_REG_ESP);n=COUNTS[site];args=list(struct.unpack('<'+'I'*n,u.mem_read(sp+4,n*4)));trace.append('C'+','.join(map(str,[site,*args]))+';');result=0
  if seed&0x100000 and site==0x497870:raise FixtureFault()
  if seed&0x200000 and site==0x4406e0:raise FixtureFault()
  if site==0x493a80:text(args[0],b'join');result=4
  elif site==0x4564b0:
   diagnostics+=1
   if diagnostics==1 and seed&8:put(0x501344,4,99);put(0x50136c,4,0);put(0x67f528,4,1)
  elif site==0x49a620:
   u.mem_write(args[0],bytes([args[1]&255])*args[2]);result=args[0]
   if seed&16:put(0x67f530,4,3)
  elif site==0x442450:
   if seed&32:put(args[0]+0x94,4,999)
  elif site==0x45da30:
   if seed&64:put(0x6e9a98+0x9b84,4,0)
  elif site==0x4423f0:
   if seed&128:put(0x1301000,1,0x85)
  elif site==0x497870:
   if seed&256:put(0x6e5588,4,1)
   result=0 if seed&2 else 0x1302000
  elif site==0x45d520:
   if seed&512:put(SCRATCH+0x6c,4,88)
   result=0 if seed&4 else 0x7cca80
  elif site==0x4406e0:put(args[0]+0x9c,4,args[1]+10)
  elif site==0x43f610:
   if seed&1024:text(args[0]+0x0c,b'changed')
  elif site==0x404cf0:result=0x1302100
  elif site==0x43f770:result=0x1302200
  rig.finish(result)
 for site in COUNTS:rig.stubs[site]=lambda site=site:foreign(site)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 seeds=list(dict.fromkeys(list(range(a.cases))+[1<<i for i in range(16)]+[8192*i+4095 for i in range(6)]+[0x7ff,0xfff,0x1fff,0x100000,0x100001,0x100100,0x200000,0x200001,0x200100]))
 payload=[];expected=[]
 for seed in seeds:
  for base,size in REGIONS:u.mem_write(base,b'\0'*size)
  d=fixture(seed)
  for at,(width,value) in d.items():put(at,width,value)
  diagnostics=0;trace=[];active=True
  failed=False
  try:rig.call(0x43f830)
  except FixtureFault:failed=True
  finally:active=False
  trace.append(f'F{int(failed)};')
  for at,(width,value) in d.items():trace.append(f'V{at},{width},{get(at,width)};')
  h=1469598103934665603
  for base,size in [(0x6e9a90,17*0x9b84),(SCRATCH,0x90),(0x7cca80,0x114),(0x1200000,0x100)]:
   for value in u.mem_read(base,size):h=((h^value)*1099511628211)&0xffffffffffffffff
  trace.append(f'H{h};');expected.append(''.join(trace));payload.append(' '.join(map(str,[seed,len(d),*[v for at,(width,value) in d.items() for v in [at,width,value]]])))
 proc=subprocess.run([str(a.probe)],input='\n'.join(payload)+'\n',text=True,capture_output=True);assert proc.returncode==0,proc.stderr
 actual=proc.stdout.splitlines();assert len(actual)==len(expected)
 diffs=[{'seed':seed,'expected':y,'actual':x} for seed,x,y in zip(seeds,actual,expected) if x!=y]
 report={'source_sha256':SOURCE_SHA256,'cases':len(seeds),'differences':len(diffs),'mismatches':diffs[:2],'foreign_services':'Explicit mutable fixtures; no nested body proof','audio_output':'none'}
 a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=128);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
