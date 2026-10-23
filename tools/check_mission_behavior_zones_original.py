#!/usr/bin/env python3
"""Compare the actual45FCF0/45FD80 memory effects."""
import argparse,json,random,subprocess
from pathlib import Path
import unicorn
from check_projectiles_original import Rig,SOURCE_SHA256
BASE=0x8e48b0;COUNT=0x4f8d44;GATE=0x67f528;MASK=0xffffffff
def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=512);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
 rig=Rig(a.game);rig.stubs.pop(0x45fcf0,None);rng=random.Random(0x45fcf0);rows=[];expected=[];reads=[];writes=[];active=False
 def put(at,n):rig.u.mem_write(at,(n&MASK).to_bytes(4,'little'))
 def get(at):return int.from_bytes(rig.u.mem_read(at,4),'little')
 def memory(u,kind,at,size,value,_):
  if not active:return
  if kind==unicorn.UC_MEM_READ and at in (GATE,COUNT):assert size==4;reads.append(at)
  elif kind==unicorn.UC_MEM_WRITE and 0x400000<=at<0xe00000:assert size==4;writes.append([at,value&MASK])
 rig.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 counts=[0,1,198,199,200,201,0x7fffffff,0x80000000,0x80000001,0xfffffffe,0xffffffff]
 for target,offset in [(COUNT,20),(GATE,24)]:counts.append(0x80000000|(((target-BASE-offset)&MASK)>>5))
 edges=[0,1,0x7fffffff,0x80000000,0xffffffff]
 for mode in (0,1):
  for gate in (0,1,0x80000000):
   for count in counts:
    for radius in edges:rows.append([mode,gate,count,0x7fffffff,0x80000000,0xffffffff,radius,0xff,2])
 for i in range(a.cases):rows.append([i%2,rng.choice([0,1,2]),rng.randrange(-8,208)&MASK,*[rng.getrandbits(32) for _ in range(6)]])
 for mode,gate,count,x,y,z,radius,kind,flags in rows:
  row=(BASE+((count<<5)&MASK))&MASK;put(GATE,gate);put(COUNT,count)
  for i in range(8):
   at=(row+i*4)&MASK
   if at not in (GATE,COUNT):put(at,0xabc00000+i)
  reads=[];writes=[];active=True
  try:rig.call(0x45fd80 if mode else 0x45fcf0,*(() if mode else (x,y,z,radius,kind,flags)))
  finally:active=False
  expected.append(dict(added=int(not mode and len(writes)==9),count=get(COUNT),row=[get((row+i*4)&MASK) for i in range(8)],reads=reads,writes=writes))
 result=subprocess.run([str(a.probe.resolve())],input=''.join(' '.join(map(str,r))+'\n' for r in rows),text=True,capture_output=True,check=True)
 actual=[json.loads(line) for line in result.stdout.splitlines()];assert len(actual)==len(expected)
 differences=[dict(case=i,input=rows[i],want=w,got=g) for i,(w,g) in enumerate(zip(expected,actual)) if w!=g]
 report=dict(source_sha256=SOURCE_SHA256,bodies=['45FCF0','45FD80'],cases=len(rows),differences=len(differences),details=differences[:3],comparison='all record words,current count,ordered reads/stores,wrapping coordinates and counter/gate aliases',foreign_bodies=[])
 a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='details'}));return bool(differences)
if __name__=='__main__':raise SystemExit(main())
