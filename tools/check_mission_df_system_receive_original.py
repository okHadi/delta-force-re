#!/usr/bin/env python3
"""Compare the DF system receiver with exact supplied instructions."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP
from check_projectiles_original import Rig,STACK
SHA='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff';MASK=0xffffffff

def run(a):
 assert hashlib.sha256((a.game/'Df.exe').read_bytes()).hexdigest()==SHA;r=Rig(a.game);u=r.u;md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls={i.address for i in md.disasm(bytes(u.mem_read(0x4b9400,0x112)),0x4b9400)if i.mnemonic in['push','pop','call','ret']};base=STACK-0x94;active=failed=False;row=[];reads=writes=calls=gate_reads=0;trace=0;restore=None;events=[]
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,v,n=4):u.mem_write(at,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
 def norm(at):return 0x30000000+at-base if base<=at<base+0x9c else at
 def event(v):
  nonlocal trace
  for i in range(4):trace=((trace^((v>>(8*i))&255))*1099511628211)&0xffffffffffffffff
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def foreign(s):
  nonlocal calls
  sp=u.reg_read(UC_X86_REG_ESP);n=2 if s==0x4c5520 else 1 if get(0x1200000,2)==0x1053 or row[18]==1 and get(0x1200000,2)==0x1052 else 0
  # The current source block determines callback arguments, including changed jump tables.
  if s!=0x4c5520:n=1 if get(sp)==0x4b9505 else 0
  args=[get(sp+4+i*4)for i in range(n)];events.append(['C',s,[norm(v)for v in args]]);event(3);event(s);event(n)
  for v in args:event(norm(v))
  calls+=1
  if calls==row[16]:stop();return
  if s==0x4c5520:
   if row[11]==1 or not row[10]:put(args[1]+0x80,row[12])
   if row[11]==2 or not row[10]:put(args[1]+0x84,row[13])
   r.finish(row[10]);return
  if s in[0x1100200,0x1100300]:
   if row[17]&16:put(0x501344,0xdeadbeef)
   r.finish(0);return
  stop()
 for s in[0x4c5520,0x1100200,0x1100300,0]:r.stubs[s]=lambda s=s:foreign(s)
 r.stubs[0x1100400]=stop
 def invalid(uc,kind,at,n,v,user):
  if at==0 and not failed:foreign(0)
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_FETCH_UNMAPPED,invalid)
 def memory(uc,kind,at,n,v,_):
  nonlocal reads,writes,restore,gate_reads
  if not active or failed:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not 0x4b9400<=pc<0x4b9512:return
  if 0x2000000<=at<0x2010000:
   if pc in controls:return
   if not(base<=at and at+n<=base+0x88 or base+0x98<=at and at+n<=base+0x9c):return
  if kind==unicorn.UC_MEM_WRITE:
   v&=(1<<(8*n))-1;events.append(['W',norm(at),n,norm(v)]);event(2);event(norm(at));event(n);event(norm(v));writes+=1
   if writes==row[15]:restore=(at,get(at,n),n);stop();return
   if row[17]&1 and at==0x501358:put(base+0x98,0x1200040)
   if row[17]&8 and at==0x501370:put(0x501374,0 if row[19]else 0x1100300)
   if row[17]&32 and at==0x501348:put(0x501378,0 if row[19]else 0x1100300)
  else:
   events.append(['R',norm(at),n]);event(1);event(norm(at));event(n);reads+=1
   if reads==row[14]:stop();return
   if row[17]&2 and at==0x501348:put(0x1200004,0xface1234)
   if row[17]&4 and at==base+0x84:put(0x501374,0x1100200)
   if row[17]&64 and at==0x501380:
    gate_reads+=1
    if gate_reads==2:put(at,0x1100300)
   if row[17]&128 and at==0x501384:put(0x1200004,0xbeef5678)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def baseline(op):return[op,0x10000,3,5,7,1,1,1,1,1,0,0,0x12345678,0xabcdef01,0,0,0,0,0,0,0,0,0,0]
 rows=[]
 for op in[0,0x104f,0x1050,0x1051,0x1052,0x1053,0x1054,0xffff]:
  for field,values in[(1,[0,0x10000,MASK]),(2,[0,1,0x7fffffff,MASK]),(3,[0,MASK]),(4,[0,MASK]),(5,[0,1]),(6,[0,1]),(7,[0,1]),(8,[0,1]),(9,[0,1]),(10,[0,1,0xb]),(11,[0,1,2]),(14,range(1,20)),(15,range(1,8)),(16,[1,2]),(17,[0,1,2,4,8,16,32,64,128,255]),(18,[0,1,2]),(19,[0,1])]:
   for v in values:x=baseline(op);x[field]=v;rows.append(x)
 for op,mode in[(0x1050,8),(0x1051,32)]:x=baseline(op);x[17]=mode;x[19]=1;rows.append(x)
 rng=random.Random(0x4b9400)
 for _ in range(a.cases):
  x=baseline(rng.choice([0x1050,0x1051,0x1052,0x1053]));x[2]=rng.getrandbits(32);x[3]=rng.getrandbits(32);x[4]=rng.getrandbits(32);x[10]=rng.choice([0,1,0xb]);x[11]=rng.randrange(3);x[17]=rng.randrange(256);x[19]=rng.randrange(2);rows.append(x)
 if a.case_file:rows=json.loads(a.case_file.read_text())
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 if a.replay_oracle:
  saved=json.loads(a.replay_oracle.read_text());assert saved['source_sha256']==SHA and saved['checker_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest();rows=saved['inputs'];expected=saved['outputs'];all_events=[[]for _ in rows]
 for index,row in enumerate([]if a.replay_oracle else rows):
  if index%32==0:a.report.with_suffix('.progress.json').write_text(json.dumps({'case':index,'total':len(rows)})+'\n')
  reads=writes=calls=gate_reads=0;trace=1469598103934665603;failed=False;restore=None;events=[];u.mem_write(0x501300,b'\x5a'*0x100);u.mem_write(0x1200000,b'\x5a'*0x100);u.mem_write(base,b'\x5a'*0x9c);targets=[0x4b9432,0x4b94a2,0x4b94d9,0x4b94f2]
  for i in range(4):put(0x4b93f0+i*4,0x1100400 if row[18]==2 else targets[(i+1)%4 if row[18]==1 else i])
  for at,v in[(0x501348,row[2]),(0x50134c,row[3]),(0x501358,row[4]),(0x501368,row[5]),(0x501374,0x1100200 if row[6]else 0),(0x501378,0x1100200 if row[7]else 0),(0x501380,0x1100200 if row[8]else 0),(0x501384,0x1100200 if row[9]else 0),(0x1200004,row[1]),(0x1200044,0xabcdef01),(base+0x98,0x1200000)]:put(at,v)
  put(0x1200000,row[0],2);put(0x1200040,0x1053,2);active=True
  try:r.call(0x4b9400,0x1200000,count=100000)
  except(unicorn.UcError,RuntimeError):
   if not failed:raise
  finally:active=False
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in[(0x501300,0x100),(0x1200000,0x100),(0x4b93f0,16),(base,0x88)]:
   for v in u.mem_read(at,n):state=((state^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {reads} {writes} {calls} {trace} {state}');all_events.append(events)
 if not a.replay_oracle:a.report.with_suffix('.oracle.json').write_text(json.dumps({'source_sha256':SHA,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'inputs':rows,'outputs':expected})+'\n')
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,(p.returncode,p.stderr);actual=p.stdout.splitlines();assert len(actual)==len(rows);bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i]}for i,(x,y)in enumerate(zip(actual,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'cases':len(rows),'differences':len(bad),'mismatches':bad[:8],'mismatch_summary':[{k:v for k,v in m.items()if k!='original_events'}for m in bad],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--cases',type=int,default=128);p.add_argument('--limit',type=int,default=0);p.add_argument('--case-file',type=Path);p.add_argument('--replay-oracle',type=Path);run(p.parse_args())
