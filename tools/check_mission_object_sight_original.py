#!/usr/bin/env python3
"""Compare42E2B0 read and call order with current callback mutations."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
from check_game_messages_original import Harness,SOURCE_HASH
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
FROM=0x7cca80;TO=FROM+0x114

def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=512);p.add_argument('--report',type=Path);a=p.parse_args();raw=a.game.read_bytes()
 if hashlib.sha256(raw).hexdigest()!=SOURCE_HASH:raise ValueError('Source hash differs')
 rows=[json.loads(x) for x in subprocess.run([str(a.probe.resolve())],input=''.join(f'{i}\n' for i in range(a.cases)),text=True,capture_output=True,check=True).stdout.splitlines()];h=Harness(raw);u=h.machine();seed=0;reads=[];calls=[]
 def get(at):return int.from_bytes(u.mem_read(at,4),'little')
 def put(at,n):u.mem_write(at,struct.pack('<I',n&0xffffffff))
 def finish(n):sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,n&0xffffffff);u.reg_write(UC_X86_REG_EIP,get(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
 def callback(machine,at,size,user):
  sp=u.reg_read(UC_X86_REG_ESP)
  if at==0x47e4b0:
   origin,direction,length=struct.unpack('<3I',u.mem_read(sp+4,12));calls.append([at,*struct.unpack('<3I',u.mem_read(origin,12)),*struct.unpack('<3I',u.mem_read(direction,12)),length])
   if seed&4:
    put(FROM+0x2c,get(FROM+0x2c)+0x30000);put(TO+0x2c,get(TO+0x2c)-0x20000);put(0x8cf904,4);put(0xbd5248,0x20000)
   finish(-0x1000 if seed%7==1 else 0 if seed%7==2 else 1)
  else:
   base,count,query=struct.unpack('<3I',u.mem_read(sp+4,12));group=1 if base==0x867e80 else 0
   if base not in (0x867e80,0x7dde80):raise ValueError('Original model list differs')
   offsets=(0,4,8,0x10,0x14,0x18,0x20,0x24,0x28,0x30,0x34,0x38,0x3c,0x40,0x44,0x48,0x4c,0x50);calls.append([at,group,count,*[get(query+off) for off in offsets]]);put(query+0x5c,0x12345+group)
   if group==1 and seed&8:
    put(0x8cfa1c,5);put(0xbd5248,0x30000);put(FROM+0x2c,0x20000);put(TO+0x2c,0x40000)
   finish(0 if seed%11==group else 1+(seed+group)%4)
 def memory(machine,access,at,width,value,user):
  if at in [FROM+0x24+i*4 for i in range(3)]+[TO+0x24+i*4 for i in range(3)]+[0x8cf904,0x8cfa1c,0xbd5248]:reads.append(at)
 hooks=[u.hook_add(UC_HOOK_CODE,callback,begin=site,end=site) for site in (0x47e4b0,0x42db80)];hooks.append(u.hook_add(UC_HOOK_MEM_READ,memory));diffs=[];total=0
 for seed,row in enumerate(rows):
  for axis in range(3):
   start=(seed*(axis+3)%31)*65536;end=(start+(seed*(axis+7)%17-8)*65536)&0xffffffff;put(FROM+0x24+axis*4,start);put(TO+0x24+axis*4,end)
  if seed%19==0:
   for axis in range(3):put(TO+0x24+axis*4,get(FROM+0x24+axis*4))
  if seed%23==0:put(TO+0x24,get(FROM+0x24)+0x6400000)
  if seed%29==0:put(TO+0x28,get(FROM+0x28)+0x80000000)
  put(0x8cfa1c,seed%4);put(0x8cf904,seed%3);put(0xbd5248,0 if seed%3 else 0x10000);reads.clear();calls.clear();h.call(u,0x42e2b0,(FROM,FROM+0x24,TO,TO+0x24));result=u.reg_read(UC_X86_REG_EAX);result=result if result<0x80000000 else result-0x100000000;want=dict(result=result,reads=list(reads),calls=list(calls));total+=len(calls)
  if want!=row:diffs.append(dict(seed=seed,want=want,got=row))
  if len(diffs)>=3:break
 for hook in hooks:u.hook_del(hook)
 report=dict(source_sha256=SOURCE_HASH,cases=len(rows),call_boundaries=total,differences=len(diffs),details=diffs)
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='details'}));return bool(diffs)
if __name__=='__main__':raise SystemExit(main())
