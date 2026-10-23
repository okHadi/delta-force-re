#!/usr/bin/env python3
"""Compare4718D0 return values and ordered current shared reads."""
import argparse,json,random,subprocess
import unicorn
from pathlib import Path
from check_projectiles_original import Rig,SOURCE_SHA256
from unicorn.x86_const import UC_X86_REG_EAX
KEYS=[0x67f520,0x67f528,0xbc5d20]
def run(args):
 rig=Rig(args.game);rng=random.Random(0x4718d0);rows=[];expected=[];active=False;trace=[];mutation=0
 def put(at,value):rig.u.mem_write(at,(value&0xffffffff).to_bytes(4,'little'))
 def memory(u,kind,at,size,value,_):
  if not active or at not in KEYS:return
  assert kind==unicorn.UC_MEM_READ and size==4
  trace.append(f'{at},{size};')
  if mutation&1 and at==KEYS[0]:put(KEYS[1],0)
  if mutation&2 and at==KEYS[1]:put(KEYS[2],0x8000000)
 rig.u.hook_add(unicorn.UC_HOOK_MEM_READ,memory)
 edges=[0,1,62,63,124,125,187,188,0x7ffffff,0x8000000,0xfffffff,0x10000000,0x7fffffff,0x80000000,0xffffffff]
 flags=[0,1,2,0x80000000,0xffffffff]
 for a in flags:
  for b in flags:
   for d in edges:
    for m in range(4):rows.append([a,b,d,m])
 rows += [[rng.getrandbits(32),rng.getrandbits(32) if i%3 else 0,rng.getrandbits(32),rng.randrange(4)] for i in range(args.cases)]
 for a,b,d,mutation in rows:
  for at,v in zip(KEYS,[a,b,d]):put(at,v)
  trace=[];active=True
  try:rig.call(0x4718d0)
  finally:active=False
  result=rig.u.reg_read(UC_X86_REG_EAX);result=result if result<0x80000000 else result-0x100000000
  expected.append(f'{result} '+''.join(trace))
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,r)) for r in rows)+'\n',text=True,capture_output=True)
 if p.returncode:raise RuntimeError(p.stderr)
 actual=p.stdout.splitlines();assert len(actual)==len(expected)
 mismatches=[{'case':i,'input':rows[i],'actual':a,'expected':b} for i,(a,b) in enumerate(zip(actual,expected)) if a!=b]
 report={'source_sha256':SOURCE_SHA256,'body':'4718D0','cases':len(rows),'mismatches':mismatches,'comparison':'signed return,complete ordered shared read addresses/widths,current callback mutations','foreign_bodies':[],'audio_output':'none'}
 args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report));assert not mismatches
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
