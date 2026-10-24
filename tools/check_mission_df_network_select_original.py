#!/usr/bin/env python3
"""Compare current DF network selection and TCP/IP control instructions."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,STACK
SHA='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff';MASK=0xffffffff;ROOT=[0x497370,0x4974a0,0x4b8f20,0x4b8f40];RANGES=[(0x497370,0x4973a8),(0x4974a0,0x4974b3),(0x4b8f20,0x4b8f3d),(0x4b8f40,0x4b8f6f)];IMPORTS={0x4b8ef0:0,0x4b7810:0,0x1100000:0,0x4c5556:1,0x4c5550:0}
def run(a):
 assert hashlib.sha256((a.game/'Df.exe').read_bytes()).hexdigest()==SHA;r=Rig(a.game);u=r.u;md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls={i.address for lo,hi in RANGES for i in md.disasm(bytes(u.mem_read(lo,hi-lo)),lo)if i.mnemonic in['push','pop','call','ret']};active=failed=False;row=[];reads=writes=calls=trace=0;restore=None;events=[]
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,v,n=4):u.mem_write(at,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
 def norm(at):return 0x30000000+at-STACK if STACK<=at<STACK+8 else at
 def event(v):
  nonlocal trace
  for i in range(4):trace=((trace^((v>>(8*i))&255))*1099511628211)&0xffffffffffffffff
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def mapped(at,n):return any(lo<=at and at+n<=lo+size for lo,size in[(0x400000,0xa00000),(0x1200000,0x10000),(STACK,8)])
 def ce(s,args):
  nonlocal calls
  events.append(['C',s,[norm(v)for v in args]]);event(3);event(s);event(len(args))
  for v in args:event(norm(v))
  calls+=1
  if calls==row[10]:stop();return False
  if s==0x4974a0:
   if row[7]&1:put(0x501398,0x501a28);put(0x50139c,0x501ab8)
   if row[7]&2:put(0x501320,0)
  elif s==0x4c5556:
   if row[7]&8:put(0x501324,99);put(0x1200000,0x1202000)
  elif s==0x4c5550:
   if row[7]&16:put(0x501320,0);put(0x501324,99)
  elif row[7]&4:put(0x501398,0x501a28);put(0x50139c,0x501a28)
  return True
 def foreign(s):
  sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(IMPORTS[s])]
  if ce(s,args):r.finish(row[6])
 for s in IMPORTS:r.stubs[s]=lambda s=s:foreign(s)
 def code(uc,pc,n,_):
  if active and not failed and pc==0x4974a0 and not(row[0]==1 and u.reg_read(UC_X86_REG_ESP)==STACK):ce(pc,[])
 u.hook_add(unicorn.UC_HOOK_CODE,code)
 def memory(uc,kind,at,n,v,_):
  nonlocal reads,writes,restore
  if not active or failed:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not any(lo<=pc<hi for lo,hi in RANGES):return
  if 0x2000000<=at<0x2010000 and(pc in controls or not(STACK<=at and at+n<=STACK+8)):return
  if kind==unicorn.UC_MEM_WRITE:
   v&=(1<<(8*n))-1;events.append(['W',norm(at),n,norm(v)]);event(2);event(norm(at));event(n);event(norm(v));writes+=1
   if writes==row[9]or not mapped(at,n):
    try:restore=(at,get(at,n),n)
    except unicorn.UcError:pass
    stop();return
  else:
   events.append(['R',norm(at),n]);event(1);event(norm(at));event(n);reads+=1
   if reads==row[8]or not mapped(at,n):stop();return
   if row[7]&32 and at==0x50139c:put(0x501320,0)
   if row[7]&64 and at==0x501398:put(0x501ad4,0x1100000)
   if row[7]&128 and at==0x501ad4:put(0x501398,0x501a28)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,n,v,user):
  if not failed:memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,n,v,user);stop()
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def default(op):return[op,1,1,0,0x501ab8,2,0,0,0,0,0,0,0x4b8ef0,0,0,0,0,0,0,0,0,0,0,0]
 rows=[]
 for op in range(4):
  for field,values in[(1,[0,1,MASK]),(2,[0,1,2,MASK]),(3,[0,0x501ab8,0x501a28,MASK]),(4,[0,0x501ab8,0x501a28]),(5,[0,1,2,MASK]),(6,[0,1,0x100,MASK]),(7,[0,1,2,4,8,16,32,64,128,255]),(11,[0,1,2]),(12,[0x4b8ef0,0x4b7810,0x1100000])]:
   for v in values:x=default(op);x[field]=v;rows.append(x)
  for field in[8,9,10]:
   for v in range(1,10):x=default(op);x[field]=v;rows.append(x)
 rng=random.Random(0x497370)
 for _ in range(a.cases):
  x=default(rng.randrange(4));x[1]=rng.randrange(2);x[2]=rng.randrange(3);x[3]=rng.choice([0,0x501ab8,0x501a28]);x[4]=rng.choice([0x501ab8,0x501a28]);x[5]=rng.randrange(3);x[6]=rng.choice([0,1,MASK]);x[7]=rng.choice([0,1,2,4,8,16,32,64,128,255]);rows.append(x)
 if a.case_file:rows=json.loads(a.case_file.read_text())
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 if a.replay_oracle:
  saved=json.loads(a.replay_oracle.read_text());assert saved['source_sha256']==SHA and saved['checker_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest();rows=saved['inputs'];expected=saved['outputs'];all_events=[[]for _ in rows]
 for index,row in enumerate([]if a.replay_oracle else rows):
  reads=writes=calls=0;trace=1469598103934665603;failed=False;restore=None;events=[];u.mem_write(0x501320,b'\x5a'*0x80);u.mem_write(0x501a28,b'\x5a'*0xc0);u.mem_write(0x1200000,b'\x5a'*0x10000);u.mem_write(STACK-0x100,b'\x5a'*0x200)
  for at,v in[(0x501320,row[1]),(0x501324,row[5]),(0x501398,row[4]),(0x50139c,row[3]),(0x501ad4,row[12]),(0x501a44,0x4b7810),(0x1200000,0x1201000)]:put(at,v)
  args=[row[2]]if row[0]==0 else[0 if row[11]==1 else STACK+4 if row[11]==2 else 0x1200000]if row[0]==2 else[];active=True;result=0
  try:r.call(ROOT[row[0]],*args,count=500000);result=u.reg_read(UC_X86_REG_EAX)
  except(unicorn.UcError,RuntimeError):
   if not failed:raise RuntimeError((index,row,hex(u.reg_read(UC_X86_REG_EIP)),events[-20:]))
  finally:active=False
  if failed or row[0]<2:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in[(0x501320,0x80),(0x501a28,0xc0),(0x1200000,0x3000)]:
   for v in u.mem_read(at,n):state=((state^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events)
 if not a.replay_oracle:a.report.with_suffix('.oracle.json').write_text(json.dumps({'source_sha256':SHA,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'inputs':rows,'outputs':expected})+'\n')
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,(p.returncode,p.stderr);actual=p.stdout.splitlines();assert len(actual)==len(rows);bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i]}for i,(x,y)in enumerate(zip(actual,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'cases':len(rows),'differences':len(bad),'mismatches':bad[:8],'mismatch_summary':[{k:v for k,v in m.items()if k!='original_events'}for m in bad],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--cases',type=int,default=128);p.add_argument('--limit',type=int,default=0);p.add_argument('--case-file',type=Path);p.add_argument('--replay-oracle',type=Path);run(p.parse_args())
