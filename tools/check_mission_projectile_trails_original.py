#!/usr/bin/env python3
"""Compare463350 and4633C0 with full-ring and current-argument checks."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256
BASE=0x9e3098;COUNTER=0x4f9148

def run(args):
 rig=Rig(args.game);rig.stubs.pop(0x463350,None);u=rig.u;v=None;target=0;trace=[];active=False
 def put(at,n):u.mem_write(at,(n&0xffffffff).to_bytes(4,'little'))
 def read(at):return int.from_bytes(u.mem_read(at,4),'little')
 def store(u,kind,at,size,value,_):
  if not active or v[3]:return
  if at!=COUNTER and not BASE<=at<BASE+65536:return
  assert size==4;trace.append(f'{at},{value&0xffffffff};')
  if v[2] and at==target:
   sp=u.reg_read(UC_X86_REG_ESP);put(sp+8,read(sp+8)^0x89abcdef);put(COUNTER,0x12345678)
  if v[2]&2 and at==target+20:put(u.reg_read(UC_X86_REG_ESP)+28,read(at-4))
 u.hook_add(unicorn.UC_HOOK_MEM_WRITE,store);rng=random.Random(0x463350);rows=[]
 for index in [0,1,511,512,2046,2047,2048,0x7fffffff,0xffffffff]:
  for mutation in [0,1,2,3]:
   for mode in [0,1]:rows.append([index,rng.getrandbits(32),mutation,mode])
 for _ in range(args.cases):rows.append([rng.getrandbits(32),rng.getrandbits(32),rng.randrange(4),rng.randrange(2)])
 expected=[]
 for v in rows:
  index,seed,mutation,mode=v;target=BASE+(((index+1)&0x7ff)<<5);put(COUNTER,index);data=struct.pack('<16384I',*[(seed+i)&0xffffffff for i in range(16384)]);u.mem_write(BASE,data);values=[(seed+i*0x1234567)&0xffffffff for i in range(7)];trace=[];active=True
  rig.call(0x4633c0 if mode else 0x463350,*([] if mode else values));active=False;result=0 if mode else u.reg_read(UC_X86_REG_EAX);out=bytes(u.mem_read(BASE,65536));h=14695981039346656037
  for n in out:h=((h^n)*1099511628211)&0xffffffffffffffff
  expected.append(f'{read(COUNTER)} {result} {h} '+''.join(trace))
 actual=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True);assert actual.returncode==0,actual.stderr;lines=actual.stdout.splitlines();assert len(lines)==len(expected);bad=[{'case':i,'input':rows[i],'actual':a,'expected':b} for i,(a,b) in enumerate(zip(lines,expected)) if a!=b]
 report={'source_sha256':SOURCE_SHA256,'bodies':['463350','4633C0'],'cases':len(rows),'differences':len(bad),'mismatches':bad[:3],'comparison':'all65536ringbytes via FNV1a64,indexreturn/reloads,source-order9stores,wrapboundaries,currentlaterstackargument mutations,destinationalias,callbackchanges toindex,clearfirst4000hexbytes preservesindex/tail'};args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=128);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
