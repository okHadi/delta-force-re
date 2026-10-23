#!/usr/bin/env python3
"""Replay selector math, live sight mutations, and current event flags."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
from check_game_messages_original import Harness,SOURCE_HASH
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
G3=0x7cca80;SHOOTER=0x7dde80;PERSON=0x1300000;DEFINITION=0x1301000;FRAMES=0x1302000;OUTPUT=0x1303000

def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=384);p.add_argument('--report',type=Path);a=p.parse_args();raw=a.game.read_bytes()
 if hashlib.sha256(raw).hexdigest()!=SOURCE_HASH:raise ValueError('Source hash differs')
 tail_cases=64;inputs=list(range(a.cases))+list(range(10000,10000+tail_cases))
 rows=[json.loads(x) for x in subprocess.run([str(a.probe.resolve())],input=''.join(f'{i}\n' for i in inputs),text=True,capture_output=True,check=True).stdout.splitlines()]
 h=Harness(raw);u=h.machine();calls=0;seed=0;tail_mode=False;tail_reads=0
 def put(at,n,width=4):u.mem_write(at,int(n&((1<<(width*8))-1)).to_bytes(width,'little'))
 def get(at,width=4):return int.from_bytes(u.mem_read(at,width),'little')
 def sight(machine,at,size,user):
  nonlocal calls
  sp=u.reg_read(UC_X86_REG_ESP);args=struct.unpack('<5I',u.mem_read(sp,20));target=args[3];index=(target-G3)//0x114;calls+=1
  if args[1:]!=(SHOOTER,SHOOTER+0x24,target,target+0x24):raise ValueError('Original sight call arguments differ')
  if tail_mode:
   u.reg_write(UC_X86_REG_EAX,1);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,args[0]);return
  if seed&16:
   put(target+0x6d,get(target+0x6d,1)^0x40,1);put(target+0x6c,get(target+0x6c,1)^1,1);put(SHOOTER+0xc,PERSON+0xb4)
  if seed&32:put(SHOOTER+0x30,get(SHOOTER+0x30)+0x10000000)
  u.reg_write(UC_X86_REG_EAX,(seed+index)%4);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,args[0])
 def frame_read(machine,access,at,width,value,user):
  if not tail_mode and width==4 and (at-FRAMES)%0x1c==0x10 and seed&64:
   index=(at-FRAMES)//0x1c;flag=G3+index*0x114+0x6d;put(flag,get(flag,1)^0x40,1)
 frame_hook=u.hook_add(UC_HOOK_MEM_READ,frame_read,begin=FRAMES,end=FRAMES+8*0x1c-1)
 hook=u.hook_add(UC_HOOK_CODE,sight,begin=0x42e2b0,end=0x42e2b0);diffs=[];totalcalls=0
 for seed,row in enumerate(rows[:a.cases]):
  u.mem_write(G3,bytes(256*0x114));u.mem_write(SHOOTER,bytes(0x114));u.mem_write(PERSON,bytes(0x168));put(SHOOTER+0xc,PERSON);put(SHOOTER+0x70,1,2);put(SHOOTER+0x30,(seed%4)*0x40000000);put(SHOOTER+0x34,(seed%3-1)*0x1000000);put(PERSON+0x74,(seed%5)*0x10000);put(PERSON+0x9c,seed%2,1);put(PERSON+0xb4+0x74,0x18000);put(PERSON+0xb4+0x9c,1,1);put(0xc23820,G3+(seed%8)*0x114);put(0x67dbc9,seed&8,1);put(DEFINITION+0x78,FRAMES)
  for i in range(8):
   at=G3+i*0x114;put(at,int((seed+i)%7!=0));put(at+4,DEFINITION);put(at+0x70,2 if (seed+i)%5 else 1,2);put(at+0x6c,4 if (seed+i)%9==0 else i&1,1);put(at+0x6d,0x40 if (seed+i)%4==0 else 0,1);distance=(seed*17+i*23)%1100;axis=(seed+i)%4;put(at+0x24+(axis&1)*4,distance*65536 if axis<2 else -distance*65536);put(at+0x2c,((seed+i)%9-4)*65536);put(at+0x80,i);put(FRAMES+i*0x1c+0x10,(seed+i)%4*0x8000)
  calls=0;h.call(u,0x44f540,(SHOOTER,OUTPUT));target=u.reg_read(UC_X86_REG_EAX);score=get(OUTPUT);totalcalls+=calls
  u.mem_write(0x6e0050,bytes(512));bit=(seed%192-64)&0xffffffff;region=seed%62+(1 if bit&0x80000000 else 0);h.call(u,0x42edd0,(region,bit));want=dict(target=target,score=score,calls=calls,flags=list(struct.unpack('<128I',u.mem_read(0x6e0050,512))))
  if row!=want:diffs.append(dict(seed=seed,want=want,got=row))
  if len(diffs)>=3:break
 def tail_read(machine,access,at,width,value,user):
  nonlocal tail_reads
  if not tail_mode:return
  target=G3+255*0x114
  if at not in (target+4,target+0x80):tail_reads+=1
  if at in (target+0x24,target+0x28,target+0x2c):
   axis=(at-target-0x24)//4;put(SHOOTER+0x24+axis*4,get(at)-(65536 if axis==0 else 0))
 tail_hook=u.hook_add(UC_HOOK_MEM_READ,tail_read,begin=G3,end=G3+256*0x114-1)
 tail_mode=True;tail_checked=0
 for case,row in enumerate(rows[a.cases:]):
  u.mem_write(G3,bytes(256*0x114));u.mem_write(SHOOTER,bytes(0x114));u.mem_write(PERSON,bytes(0x168));target=G3+255*0x114
  put(SHOOTER+0xc,PERSON);put(SHOOTER+0x70,1,2);put(PERSON+0x74,0x10000);put(PERSON+0x9c,1,1)
  for axis,n in enumerate((10,20,30)):put(SHOOTER+0x24+axis*4,n*65536)
  for axis,n in enumerate((20+case%4,20+case%3,30+case%5)):put(target+0x24+axis*4,n*65536)
  put(target,1);put(target+4,DEFINITION);put(target+0x70,2,2);put(DEFINITION+0x78,FRAMES);put(FRAMES+0x10,(case%3+1)*65536)
  tail_checked+=1;calls=0;tail_reads=0;h.call(u,0x44f540,(SHOOTER,OUTPUT));want=dict(target=u.reg_read(UC_X86_REG_EAX),score=get(OUTPUT),calls=calls,tail_reads=tail_reads,shooter=[get(SHOOTER+0x24+axis*4) for axis in range(3)])
  if want!=row:diffs.append(dict(tail_case=case,want=want,got=row))
  if len(diffs)>=3:break
 u.hook_del(tail_hook)
 u.hook_del(hook);u.hook_del(frame_hook);report=dict(source_sha256=SOURCE_HASH,cases=a.cases,tail_mutation_cases=tail_checked,sight_calls=totalcalls,differences=len(diffs),details=diffs)
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='details'}));return bool(diffs)
if __name__=='__main__':raise SystemExit(main())
