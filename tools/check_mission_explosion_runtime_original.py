#!/usr/bin/env python3
"""Compare4657D0 gates, queue bytes, limits, and original store order."""
import argparse,json,struct,subprocess
from pathlib import Path
from check_game_messages_original import Harness,SOURCE_HASH
from unicorn import UC_HOOK_MEM_WRITE
BASE=0x9e2e40;COUNT=0xa15898;BASES=(0x7dde80,0x867e80,0x8ace80,0x7cca80)
def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=396);p.add_argument('--report',type=Path);a=p.parse_args()
 import hashlib
 raw=a.game.read_bytes()
 if hashlib.sha256(raw).hexdigest()!=SOURCE_HASH:raise ValueError('Source hash differs')
 rows=subprocess.run([str(a.probe.resolve())],input=''.join(f'{seed}\n' for seed in range(a.cases)),capture_output=True,text=True,check=True);got=[json.loads(line) for line in rows.stdout.splitlines()]
 if len(got)!=a.cases:raise ValueError('Probe reply count differs')
 h=Harness(raw);u=h.machine();writes=[]
 def put(at,v):u.mem_write(at,struct.pack('<I',v&0xffffffff))
 def get(at):return int.from_bytes(u.mem_read(at,4),'little')
 def store(machine,access,at,width,value,user):
  if not BASE<=at<BASE+660 and at!=COUNT:return
  if width!=4:raise ValueError('Original queue width differs')
  writes.append([at,value&0xffffffff])
 hook=u.hook_add(UC_HOOK_MEM_WRITE,store);diffs=[];accepted_count=0
 for seed,row in enumerate(got):
  count=seed%33;mp=(0,1,0xffffffff,0x80000000)[seed%4];host=(seed//4)%3;put(0x67f520,mp);put(0x67f528,host);put(COUNT,count)
  initial=[]
  for i in range(count):initial.append([(seed+i*3)&0xffffffff,(-seed-i)&0xffffffff,seed^i,i,BASES[i%4]+(i%8)*0x114 if i%5 else 0])
  u.mem_write(BASE,b''.join(struct.pack('<5I',*r) for r in initial)+bytes(20));rng=seed+1;args=[]
  for _ in range(4):rng=(rng*1664525+1013904223)&0xffffffff;args.append(rng)
  owner=BASES[(seed//5)%4]+(seed%8)*0x114 if seed%5 else 0;writes.clear();h.call(u,0x4657d0,(*args,owner));accepted=int((not mp or host) and count<30);accepted_count+=accepted
  wanted_writes=[]
  if accepted:
   at=BASE+count*20;wanted_writes=[[at+16,owner],[at+12,args[0]],[at,args[1]],[at+4,args[2]],[at+8,args[3]],[COUNT,count+1]]
  if writes!=wanted_writes:raise ValueError('Original source store sequence differs')
  after=get(COUNT);want=dict(accepted=accepted,count=after,rows=[list(struct.unpack('<5I',u.mem_read(BASE+i*20,20))) for i in range(after)])
  if row!=want:diffs.append(dict(seed=seed,want=want,got=row))
  if len(diffs)>=3:break
 u.hook_del(hook);report=dict(source_sha256=SOURCE_HASH,cases=a.cases,accepted=accepted_count,capacity=30,original_ordered_store_cases=accepted_count,differences=len(diffs),details=diffs)
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='details'}));return bool(diffs)
if __name__=='__main__':raise SystemExit(main())
