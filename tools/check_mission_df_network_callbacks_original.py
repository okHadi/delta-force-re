#!/usr/bin/env python3
"""Compare current DF network callback instructions and failure prefixes."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP
from check_projectiles_original import Rig,STACK
SHA='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff';MASK=0xffffffff
ROOT=[0x43fd60,0x43fef0,0x43ff20];RANGES=[(0x43fd60,0x43feec),(0x43fef0,0x43ff13),(0x43ff20,0x43ff2b)]
IMPORTS={0x493a80:3,0x4564b0:1,0x404cf0:2,0x43f770:2,0x45da30:1,0x4423f0:3,0x49a620:3,0x442450:1,0x479f00:2}
def run(a):
 assert hashlib.sha256((a.game/'Df.exe').read_bytes()).hexdigest()==SHA;r=Rig(a.game);u=r.u;md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls={i.address for lo,hi in RANGES for i in md.disasm(bytes(u.mem_read(lo,hi-lo)),lo)if i.mnemonic in['push','pop','call','ret']};frame=STACK-0xf8;active=failed=False;row=[];reads=writes=calls=trace=0;restore=None;events=[]
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,v,n=4):u.mem_write(at,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
 def norm(at):return 0x30000000+at-frame if frame<=at<frame+0xe8 else at
 def event(v):
  nonlocal trace
  for i in range(4):trace=((trace^((v>>(8*i))&255))*1099511628211)&0xffffffffffffffff
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def mapped(at,n):return any(lo<=at and at+n<=lo+size for lo,size in[(0x400000,0xa00000),(0x1200000,0x1000),(frame,0xe8)])
 def foreign(s):
  nonlocal calls
  sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(IMPORTS[s])];events.append(['C',s,[norm(v)for v in args]]);event(3);event(s);event(len(args))
  for v in args:event(norm(v))
  calls+=1
  if calls==row[10]:stop();return
  if s==0x493a80:
   put(frame,0,1)
   if row[7]&1:put(0x501344,999)
   if row[7]&2:put(0x67f528,not row[2])
  elif s==0x404cf0:
   if row[7]&4:
    put(0x67db40,999)
    if row[4]<17:put(0x6e9a90+row[4]*0x9b84+8,0x7ccb94)
  elif s==0x43f770:
   if row[7]&8:put(0x1200000,0,1)
  elif s==0x45da30:
   if row[7]&16:put(0x6e9a90+row[4]*0x9b84+8,0);put(frame+0x80,0x7777,2)
  elif s==0x4423f0:
   if row[7]&32:put(0x6e9a90+row[4]%17*0x9b84+0x18c,1,1)
  elif s==0x49a620:u.mem_write(args[0],bytes([args[1]&255])*args[2])
  elif s==0x442450:
   if row[7]&64:put(0x6e00ed+row[4]+0x9b84,99,1)
  r.finish(0x1200000 if s==0x43f770 else 0x1200800 if s==0x404cf0 else 0)
 for s in IMPORTS:r.stubs[s]=lambda s=s:foreign(s)
 def memory(uc,kind,at,n,v,_):
  nonlocal reads,writes,restore
  if not active or failed:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not any(lo<=pc<hi for lo,hi in RANGES):return
  if 0x2000000<=at<0x2010000 and(pc in controls or not(frame<=at and at+n<=frame+0xe8)):return
  if kind==unicorn.UC_MEM_WRITE:
   v&=(1<<(8*n))-1;events.append(['W',norm(at),n,norm(v)]);event(2);event(norm(at));event(n);event(norm(v));writes+=1
   if writes==row[9]or not mapped(at,n):
    try:restore=(at,get(at,n),n)
    except unicorn.UcError:pass
    stop();return
  else:
   events.append(['R',norm(at),n]);event(1);event(norm(at));event(n);reads+=1
   if reads==row[8]or not mapped(at,n):stop();return
   if row[7]&128 and 0x6e9c1c<=at<=0x78ed40 and n==1 and row[4]<17:put(0x6e9a90+row[4]*0x9b84,row[1])
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,n,v,user):
  if not failed:memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,n,v,user);stop()
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def default(op):return[op,123,1,0x98765432,3,1,0x7cca80+3*0x114,0,0,0,0,13]
 rows=[]
 for op in range(3):
  for field,values in[(1,[0,123,MASK]),(2,[0,1,MASK]),(4,[0,1,3,16,17,MASK]),(5,[0,1,2,255]),(6,[0,0x7cca80,0x7ccb94,0x7ccba8]),(7,[0,1,2,4,8,16,32,64,128,255]),(11,[0,1,2,3,12,13,50,80])]:
   for v in values:x=default(op);x[field]=v;rows.append(x)
  for field in[8,9,10]:
   for v in range(1,95 if field!=10 else 13):x=default(op);x[field]=v;rows.append(x)
 rng=random.Random(0x43fd60)
 for _ in range(a.cases):
  x=default(rng.randrange(3));x[2]=rng.randrange(2);x[4]=rng.randrange(18);x[5]=rng.randrange(2);x[6]=rng.choice([0,0x7cca80,0x7ccd00]);x[7]=rng.choice([0,1,2,4,8,16,32,64,128,255]);x[11]=rng.randrange(81);rows.append(x)
 if a.case_file:rows=json.loads(a.case_file.read_text())
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 if a.replay_oracle:
  saved=json.loads(a.replay_oracle.read_text());assert saved['source_sha256']==SHA and saved['checker_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest();rows=saved['inputs'];expected=saved['outputs'];all_events=[[]for _ in rows]
 for index,row in enumerate([]if a.replay_oracle else rows):
  reads=writes=calls=0;trace=1469598103934665603;failed=False;restore=None;events=[]
  for at,n in[(0x501340,0x40),(0x67db40,0x40),(0x6e9a90,0xa53c4),(0x1200000,0x1000),(frame,0xe8)]:u.mem_write(at,b'\x5a'*n)
  for at,v in[(0x501344,row[1]),(0x67f528,row[2]),(0x67db40,row[3])]:put(at,v)
  for i in range(17):c=0x6e9a90+i*0x9b84;put(c,100+i);put(c+0x18c,0,1);put(c+8,0)
  if row[4]<17:c=0x6e9a90+row[4]*0x9b84;put(c,row[1]);put(c+0x18c,row[5],1);put(c+8,row[6])
  for i in range(row[11]+1):put(0x1200000+i,65+i%26 if i<row[11]else 0,1)
  active=True
  try:r.call(ROOT[row[0]],count=500000)
  except(unicorn.UcError,RuntimeError):
   if not failed:raise RuntimeError((index,row,hex(u.reg_read(UC_X86_REG_EIP)),events[-20:]))
  finally:active=False
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in[(0x501340,0x40),(0x67db40,0x40),(0x6e9a90,0xa53c4),(frame,0xe8),(0x1200000,0x100)]:
   for v in u.mem_read(at,n):state=((state^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {reads} {writes} {calls} {trace} {state}');all_events.append(events)
 if not a.replay_oracle:a.report.with_suffix('.oracle.json').write_text(json.dumps({'source_sha256':SHA,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'inputs':rows,'outputs':expected})+'\n')
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,(p.returncode,p.stderr);actual=p.stdout.splitlines();assert len(actual)==len(rows);bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i]}for i,(x,y)in enumerate(zip(actual,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'cases':len(rows),'differences':len(bad),'mismatches':bad[:8],'mismatch_summary':[{k:v for k,v in m.items()if k!='original_events'}for m in bad],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--cases',type=int,default=128);p.add_argument('--limit',type=int,default=0);p.add_argument('--case-file',type=Path);p.add_argument('--replay-oracle',type=Path);run(p.parse_args())
