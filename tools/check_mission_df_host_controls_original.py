#!/usr/bin/env python3
"""Compare DF host/player controls with the exact supported PE instructions."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI
from check_projectiles_original import Rig,STACK
SHA='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff';MASK=0xffffffff
ROOT=[0x4b91c0,0x4b92f0,0x4b9300,0x4b93d0];SIZE=[0x130,4,0xa8,4];LOCAL=[0x10c,0,0x94,0];IMPORTS={0x49a620:3,0x4c5538:1,0x4c552c:3,0x4c5526:1,0x4c5532:0};RANGES=[(0x4b91c0,0x4b92e9),(0x4b92f0,0x4b92f5),(0x4b9300,0x4b93c1),(0x4b93d0,0x4b93ee)]
def run(a):
 assert hashlib.sha256((a.game/'Df.exe').read_bytes()).hexdigest()==SHA;r=Rig(a.game);u=r.u;md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls={i.address for lo,hi in RANGES for i in md.disasm(bytes(u.mem_read(lo,hi-lo)),lo)if i.mnemonic in['push','pop','call','ret']};assert bytes(u.mem_read(0x4b928d,2))==bytes.fromhex('f2a5')and bytes(u.mem_read(0x4b9294,2))==bytes.fromhex('f2a4');active=failed=False;row=[];base=n=reads=writes=calls=trace=payload=0;restore=None;events=[]
 def get(at,count=4):return int.from_bytes(u.mem_read(at,count),'little')
 def put(at,value,count=4):u.mem_write(at,(value&((1<<(8*count))-1)).to_bytes(count,'little'))
 def string(at,count,start):u.mem_write(at,bytes((start+i%23)&255 for i in range(count))+b'\0')
 def norm(at):return 0x30000000+at-base if base<=at<base+n else at
 def event(v):
  nonlocal trace
  for i in range(4):trace=((trace^((v>>(8*i))&255))*1099511628211)&0xffffffffffffffff
 def mapped(at,count,store=False):
  valid=any(lo<=at and at+count<=lo+size for lo,size in[(0x400000,0xa00000),(0x1200000,0x100000),(base,n)])
  gap=0x100 if row[0]==0 else 0x88 if row[0]==2 else 0
  return valid and not(store and gap and at<base+gap+16 and at+count>base+gap)
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def ce(s,args):
  nonlocal calls
  events.append(['C',s,[norm(v)for v in args]]);event(3);event(s);event(len(args))
  for v in args:event(norm(v))
  calls+=1
  if calls==row[10]:stop();return False
  if s==0x49a620:
   if row[7]&1:
    if row[0]==0:put(base+0x118,13);put(base+0x110,0x1201000);put(base+0x11c,0x1203000)
    elif row[0]==2:put(base+0x9c,int(not row[4]))
   if row[7]&2 and row[0]==2:put(base+0x98,0x1201000);put(base+0xa0,0x1205000);put(base+0xa4,3)
   if row[7]&128:put(0x501320,0)
  if s==0x4c5538 and row[7]&8:put(0x501368,77)
  if s==0x4c552c:
   if row[7]&16:put(base+0x9c,1)
   if row[7]&32:put(base+0x9c,0)
  return True
 def foreign(s):
  nonlocal payload
  sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(IMPORTS[s])]
  if not ce(s,args):return
  result=row[6]
  if s==0x49a620:
   for i in range(args[2]):
    if not mapped(args[0]+i,1,True):stop();return
    put(args[0]+i,args[1]&255,1)
   result=args[0]
  elif s in[0x4c5538,0x4c552c]:
   at=args[0]if s==0x4c5538 else args[1];count=0x100 if s==0x4c5538 else 0x98;payload=1469598103934665603
   for i in range(count):
    value=(norm(get(at+0x80))>>(8*(i-0x80)))&255 if s==0x4c552c and 0x80<=i<0x84 else get(at+i,1);payload=((payload^value)*1099511628211)&0xffffffffffffffff
   if s==0x4c552c:put(args[0],row[22])
  r.finish(result)
 for s in IMPORTS:r.stubs[s]=lambda s=s:foreign(s)
 def memory(uc,kind,at,count,value,_):
  nonlocal reads,writes,restore
  if not active or failed:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not any(lo<=pc<hi for lo,hi in RANGES):return
  if 0x2000000<=at<0x2010000 and(pc in controls or not(base<=at and at+count<=base+n)):return
  if kind==unicorn.UC_MEM_WRITE:
   value&=(1<<(8*count))-1;events.append(['W',norm(at),count,norm(value)]);event(2);event(norm(at));event(count);event(norm(value));writes+=1
   if writes==row[9]or not mapped(at,count,True):
    try:restore=(at,get(at,count),count)
    except unicorn.UcError:pass
    stop();return
  else:
   events.append(['R',norm(at),count]);event(1);event(norm(at));event(count);reads+=1
   if reads==row[8]or not mapped(at,count):stop();return
   if row[7]&4 and at==0x4cebc4:put(base+0x110,0x1201000);put(0x4cebcc,0xdecafbad)
   if row[7]&64 and at==0x50133c:put(0x50133c,row[22]+2)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,count,value,user):
  if not failed:memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,count,value,user);stop()
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def default(op):return[op,1,6,2,1,7,0,0,0,0,0,0,0x11112222,0x33334444,0x55556666,0,0,8,0x11223344,0x55667788,0x99aabbcc,0xddeeff00,0x10003,0]
 rows=[]
 for op in range(4):
  for field,values in[(1,[0,1,0xffffffff]),(2,[0,1,2,3,15,63,127,128,135,136,143,255]),(3,[0,1,2,3,63,111,112,113,127,0xffffffff]),(4,[0,1,0xffffffff]),(5,[0,1,7,0xffffffff]),(6,[0,1,9,0xffffffff]),(7,[0,1,2,4,8,16,32,64,128,255]),(11,[0,1]),(12,[0,0xffffffff]),(13,[0,0xffffffff]),(14,[0,0xffffffff]),(15,[0,1,2]),(16,[0,1]),(17,[0,8,0xffffffff])]:
   for v in values:x=default(op);x[field]=v;rows.append(x)
  for field in[8,9]:
   for v in[1,2,3,4,8,16,32,48,64,96,128,160,192,256]:x=default(op);x[field]=v;rows.append(x)
  for v in[1,2,3]:x=default(op);x[10]=v;rows.append(x)
 rng=random.Random(0x4b9300)
 for _ in range(a.cases):
  x=default(rng.randrange(4));x[1]=rng.randrange(2);x[2]=rng.randrange(150);x[3]=rng.choice([0,1,3,63,113,0xffffffff]);x[4]=rng.randrange(2);x[7]=rng.choice([0,1,2,4,8,16,32,64,128,255]);rows.append(x)
 if a.case_file:rows=json.loads(a.case_file.read_text())
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 if a.replay_oracle:
  saved=json.loads(a.replay_oracle.read_text());assert saved['source_sha256']==SHA and saved['checker_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest();rows=saved['inputs'];expected=saved['outputs'];all_events=[[]for _ in rows]
 for index,row in enumerate([]if a.replay_oracle else rows):
  base=STACK-LOCAL[row[0]];n=SIZE[row[0]];reads=writes=calls=0;trace=1469598103934665603;payload=0;failed=False;restore=None;events=[];u.mem_write(0x501320,b'\x5a'*0x80);u.mem_write(0x1200000,b'\x5a'*0x100000);u.mem_write(STACK-0x2000,b'\x5a'*0x4000)
  put(0x501320,row[1]);put(0x50133c,0xdeadbeef);put(0x501340,0xabcdef01);put(0x501368,0);string(0x1200000,row[2],ord('A'));string(0x1201000,3,ord('a'));string(0x1202000,2 if row[3]==MASK else row[3],ord('p'));string(0x1203000,4,ord('k'))
  for i in range(4):put(0x4cebc4+i*4,0x11223344+i*0x11111111)
  name=base+(0x10 if row[0]==0 else 0)if row[15]==1 else 0 if row[15]==2 else 0x1200000;password=0 if row[3]==MASK else base+0x90 if row[16]else 0x1202000
  args=[name,row[23],row[17],password,*row[18:22]]if row[0]==0 else[name,row[4],base+0x10 if row[11]else 0x1204000,row[5]]if row[0]==2 else[]
  u.reg_write(UC_X86_REG_EDI,row[12]);u.reg_write(UC_X86_REG_ESI,row[13]);u.reg_write(UC_X86_REG_EBX,row[14]);active=True;result=0
  try:r.call(ROOT[row[0]],*args,count=500000);result=u.reg_read(UC_X86_REG_EAX)
  except(unicorn.UcError,RuntimeError):
   if not failed:raise RuntimeError((index,row,hex(u.reg_read(UC_X86_REG_EIP)),events[-20:]))
  finally:active=False
  if failed:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for at,count in[(0x501320,0x80),(0x4cebc4,16),(0x1200000,0x6000),(base,0x110 if row[0]==0 else 0x98 if row[0]==2 else 0)]:
   for i in range(count):
    value=(norm(get(base+0x80))>>(8*(i-0x80)))&255 if row[0]==2 and at==base and 0x80<=i<0x84 else get(at+i,1);state=((state^value)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {payload} {state}');all_events.append(events if a.limit==1 else events[:400])
 if not a.replay_oracle:a.report.with_suffix('.oracle.json').write_text(json.dumps({'source_sha256':SHA,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'inputs':rows,'outputs':expected})+'\n')
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,row))for row in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,(p.returncode,p.stderr);actual=p.stdout.splitlines();assert len(actual)==len(rows);bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i]}for i,(x,y)in enumerate(zip(actual,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'cases':len(rows),'differences':len(bad),'mismatches':bad[:8],'mismatch_summary':[{k:v for k,v in m.items()if k!='original_events'}for m in bad],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--cases',type=int,default=128);p.add_argument('--limit',type=int,default=0);p.add_argument('--case-file',type=Path);p.add_argument('--replay-oracle',type=Path);run(p.parse_args())
