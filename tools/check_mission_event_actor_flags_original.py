#!/usr/bin/env python3
"""Compare event actor scans, writes, callbacks and removal packets."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
from check_game_messages_original import Harness,SOURCE_HASH
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_READ,UC_HOOK_MEM_WRITE,UcError
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
BASES={0:0x7dde80,1:0x867e80,3:0x7cca80};COUNTS={0:0x8cfa1c,1:0x8cf904,3:0x8cfa18};DAMAGE=0xa158bc

def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=640);p.add_argument('--report',type=Path);a=p.parse_args();raw=a.game.read_bytes()
 if hashlib.sha256(raw).hexdigest()!=SOURCE_HASH:raise ValueError('Source hash differs')
 rows=[json.loads(x) for x in subprocess.run([str(a.probe.resolve())],input=''.join(f'{i}\n' for i in range(a.cases)),text=True,capture_output=True,check=True).stdout.splitlines()];h=Harness(raw);u=h.machine();seed=0;reads=[];writes=[];calls=[];last=0
 def get(at,width=4):return int.from_bytes(u.mem_read(at,width),'little')
 def put(at,n,width=4):u.mem_write(at,int(n&((1<<(width*8))-1)).to_bytes(width,'little'))
 def ref(at):
  for g,base in BASES.items():
   if base<=at<base+4*0x114:return g,(at-base)//0x114
  raise ValueError('Original actor exceeds fixture')
 def finish():sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EIP,get(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
 def mutate(at):
  g,i=ref(at);put(COUNTS[g],(seed+g+i)%5)
  if seed&4 and i+1<4:put(at+0x114+0x72,seed%5-2,2)
 def callback(machine,at,size,user):
  sp=u.reg_read(UC_X86_REG_ESP);actor=get(sp+4)
  if at in (0x500000,0x500010):
   message=get(sp+8);calls.append([at,actor,message,get(actor+0x74,2),get(DAMAGE)]);mutate(actor);put(DAMAGE,0x100+ref(actor)[1])
  elif at==0x45da30:
   calls.append([at,actor]);u.mem_write(actor,bytes(0x114));mutate(actor)
   if seed&8:put(0x67f528,0)
  else:
   payload=get(sp+8);length=get(sp+12);calls.append([at,actor,length,get(payload,2)])
  finish()
 def read(machine,access,at,width,value,user):
  if at in (*COUNTS.values(),0x67f528):reads.append(at);return
  for base in BASES.values():
   if base<=at<base+4*0x114 and (at-base)%0x114 in (0x70,0x110):reads.append(at)
 def write(machine,access,at,width,value,user):
  nonlocal last
  if at==DAMAGE:
   writes.append([at,width,value&0xffffffff])
   if seed&64:put(last+0x110,0x500010)
  elif width==2 and any(base<=at<base+4*0x114 and (at-base)%0x114==0x74 for base in BASES.values()):
   writes.append([at,width,value&0xffff]);last=at-0x74
   if seed&32:put(last+0x72,9,2)
   put(last+0x76,0xabcd,2)
 hooks=[u.hook_add(UC_HOOK_CODE,callback,begin=site,end=site) for site in (0x500000,0x500010,0x45da30,0x442630)];hooks.extend((u.hook_add(UC_HOOK_MEM_READ,read),u.hook_add(UC_HOOK_MEM_WRITE,write)));diffs=[];total=0
 for seed,row in enumerate(rows):
  for g,base in BASES.items():
   u.mem_write(base,bytes(4*0x114))
   for i in range(4):
    at=base+i*0x114;put(at,(seed+i+g)%2);put(at+0x72,(seed+i+g)%5-2,2);put(at+0x74,0x9876,2);put(at+0x76,0x1234,2);put(at+0x110,0x500000)
  for g,n in ((0,seed//16%4),(1,seed%4),(3,seed//4%4)):put(COUNTS[g],n)
  if seed%17==0:put(COUNTS[1],-1)
  put(DAMAGE,0xa5a5);put(0x67f528,seed%3);reads.clear();writes.clear();calls.clear();h.call(u,0x45f5b0 if seed%2 else 0x45f4c0,(seed%5-2,));total+=len(calls)
  want=dict(damage=get(DAMAGE),host=get(0x67f528),counts=[get(COUNTS[g]) if get(COUNTS[g])<0x80000000 else get(COUNTS[g])-0x100000000 for g in (0,1,3)],rows=[[get(base+i*0x114+offset,width) for offset,width in ((0x72,2),(0x74,2),(0x76,2),(0x110,4),(0,4))] for base in (BASES[1],BASES[3],BASES[0]) for i in range(4)],reads=list(reads),writes=list(writes),calls=list(calls))
  if want!=row:diffs.append(dict(seed=seed,want=want,got=row))
  if len(diffs)>=3:break
 seed=0;put(COUNTS[1],1);put(COUNTS[3],0);put(COUNTS[0],0);put(BASES[1]+0x72,0xffff,2);put(BASES[1]+0x110,0);put(DAMAGE,0x1234);null_fault=False
 try:h.call(u,0x45f4c0,(0xffffffff,))
 except UcError:null_fault=True
 if not null_fault or get(BASES[1]+0x74,2)!=0xffce or get(DAMAGE)!=0:raise ValueError('Original null callback prefix differs')
 for hook in hooks:u.hook_del(hook)
 report=dict(source_sha256=SOURCE_HASH,cases=len(rows),callback_boundaries=total,null_callback_fault=null_fault,differences=len(diffs),details=diffs)
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='details'}));return bool(diffs)
if __name__=='__main__':raise SystemExit(main())
