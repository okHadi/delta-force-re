#!/usr/bin/env python3
"""Compare43EBA0 shared stores with the original instructions."""
import argparse,json,random,subprocess
from pathlib import Path
import unicorn
from check_projectiles_original import Rig,SOURCE_SHA256
KEYS=[0x6e06a8,0x6e06ac,0x6e06b0,0x6e4708,0x6e4704,0x6e06b4,0x6e4700,0x6e06b8]
def run(args):
 rig=Rig(args.game);rng=random.Random(0x43eba0);active=False;trace=[];rows=[];expected=[]
 def memory(u,kind,at,size,value,_):
  if active and at in KEYS:
   assert size==4;trace.append(f'{at},{size},{value&0xffffffff};')
 rig.u.hook_add(unicorn.UC_HOOK_MEM_WRITE,memory)
 edges=[0,1,0x7fffffff,0x80000000,0xffffffff]
 rows += [[a,b,c,d,e] for a in edges for b in edges for c in edges for d in edges for e in edges]
 rows += [[rng.getrandbits(32) for _ in range(5)] for _ in range(args.cases)]
 for row in rows:
  for at in KEYS:rig.u.mem_write(at,b'\xa5'*4)
  trace=[];active=True
  try:rig.call(0x43eba0,*row)
  finally:active=False
  values=[int.from_bytes(rig.u.mem_read(at,4),'little') for at in KEYS]
  expected.append(' '.join(map(str,values))+' '+''.join(trace))
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,r)) for r in rows)+'\n',text=True,capture_output=True)
 if p.returncode:raise RuntimeError(p.stderr)
 actual=p.stdout.splitlines();assert len(actual)==len(expected)
 mismatches=[{'case':i,'input':rows[i],'actual':a,'expected':b} for i,(a,b) in enumerate(zip(actual,expected)) if a!=b]
 report={'source_sha256':SOURCE_SHA256,'body':'43EBA0','cases':len(rows),'mismatches':mismatches,'comparison':'all8 shared dwords and full ordered store addresses,widths,values','foreign_bodies':[],'audio_output':'none'}
 args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report));assert not mismatches
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
