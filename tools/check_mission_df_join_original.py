#!/usr/bin/env python3
"""Compare the complete DF Join parent with exact supplied instructions."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,STACK
SHA='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff';MASK=0xffffffff

def run(a):
 assert hashlib.sha256((a.game/'Df.exe').read_bytes()).hexdigest()==SHA;r=Rig(a.game);u=r.u;assert bytes(u.mem_read(0x4b90f9,2))==bytes.fromhex('f2a5') and bytes(u.mem_read(0x4b911a,2))==bytes.fromhex('f2a5');md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls={i.address for i in md.disasm(bytes(u.mem_read(0x4b9090,0x121)),0x4b9090)if i.mnemonic in['push','pop','call','ret']};base=STACK-0x10c;active=failed=False;row=[];reads=writes=calls=0;trace=0;restore=None;events=[]
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,v,n=4):u.mem_write(at,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
 def selected():return 0 if row[2]==0 else base+0x70 if row[2]==2 else 0x1200100 if row[2]==3 else base+0xbc if row[2]==4 else 0x1200000
 def password():return 0 if row[3]==0 else base+0x40 if row[3]==2 else base+0x90 if row[3]==3 else 0x1200200
 def norm(at):return 0x30000000+at-base if base<=at<base+0x118 else at
 def event(v):
  nonlocal trace
  for i in range(4):trace=((trace^((v>>(8*i))&255))*1099511628211)&0xffffffffffffffff
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def ce(s,args):
  nonlocal calls
  events.append(['C',s,[norm(v)for v in args]]);event(3);event(s);event(len(args))
  for v in args:event(norm(v))
  calls+=1
  if calls==row[12]:stop();return False
  return True
 def foreign(s):
  sp=u.reg_read(UC_X86_REG_ESP);n={0x4c5544:0,0x49a620:3,0x4c553e:1}[s];args=[get(sp+4+i*4)for i in range(n)]
  if not ce(s,args):return
  if s==0x4c5544:
   if row[9]&1:put(base+0x110,0x1200100);put(base+0x114,0)
   if row[9]&2:
    if selected():put(selected()+0x10,0xdeadbeef)
    put(0x1200200,ord('x'),1)
   if row[9]&128:put(0x501320,0)
   r.finish(row[5]);return
  if s==0x49a620:
   for i in range(args[2]):
    if row[13]and i==row[13]:stop();return
    put(args[0]+i,args[1]&255,1)
   if row[9]&4:
    put(0x4cebc4,0xface1234)
    if selected():put(selected()+0x10,0xbeef5678)
   r.finish(args[0]);return
  if row[9]&64:put(0x501368,1)
  r.finish(row[6])
 for s in[0x4c5544,0x49a620,0x4c553e]:r.stubs[s]=lambda s=s:foreign(s)
 def code(uc,pc,n,user):
  if active and not failed and pc==0x4b9080 and row[0]==0:ce(pc,[])
 u.hook_add(unicorn.UC_HOOK_CODE,code)
 def memory(uc,kind,at,n,v,_):
  nonlocal reads,writes,restore
  if not active or failed:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not 0x4b9090<=pc<0x4b91b1:return
  if 0x2000000<=at<0x2010000:
   if pc in controls:return
   if not(base<=at and at+n<=base+0x100 or base+0x110<=at and at+n<=base+0x118):
    # The password copy can reach the saved-register gap. It is current mapped frame memory.
    if not(base<=at and at+n<=base+0x118):
     if at>=base and at<base+0x300:pass
     else:return
  mapped=any(lo<=at and at+n<=lo+size for lo,size in[(base,0x100),(base+0x110,8),(0x1200000,0x400),(0x501300,0x100),(0x4cebc4,16)])
  if kind==unicorn.UC_MEM_WRITE:
   v&=(1<<(8*n))-1;events.append(['W',norm(at),n,norm(v)]);event(2);event(norm(at));event(n);event(norm(v));writes+=1
   if writes==row[11]or not mapped:restore=(at,get(at,n),n);stop();return
   if row[9]&256 and at==base+0xd0 and selected():put(selected()+0x14,0x98765432)
   if row[9]&512 and at==base+0xe0:put(0x4cebc8,0x87654321)
   if row[9]&8 and at==base+0xd0:put(0x4cebc4,0xabcdef01)
   if row[9]&16 and at==base+0xe0:put(0x1200200,ord('z'),1)
  else:
   events.append(['R',norm(at),n]);event(1);event(norm(at));event(n);reads+=1
   if reads==row[10]or not mapped:stop();return
   if row[9]&32 and at==0x4cebc4:put(base+0x114,0)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def baseline(op):return[op,1,1,1,2,8,9,0xe243fc00,0x11223344,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
 rows=[]
 for op in[0,1]:
  for field,values in[(1,[0,1]),(2,[0,1,2,3,4]),(3,[0,1,2,3]),(4,[0,1,2,3,16,127,128,140,255,256]),(5,[0,1,8,0x200,MASK]),(6,[0,1,5,9,0xf,0x300,MASK]),(7,[0,0xe243fc00,MASK]),(8,[0,0x11223344,MASK]),(9,[0,1,2,4,8,16,32,64,128,255,256,512,768]),(10,range(1,25)),(11,range(1,20)),(12,range(1,6)),(13,[0,1,64,128,255])]:
   for v in values:x=baseline(op);x[field]=v;rows.append(x)
 for pw in[127,128,140,255,256]:
  for mode in[1,2,3]:x=baseline(0);x[4]=pw;x[3]=mode;rows.append(x)
 for mode in[4,256,512,768]:
  x=baseline(0);x[2]=4;x[9]=mode;rows.append(x)
 rng=random.Random(0x4b9090)
 for _ in range(a.cases):
  x=baseline(0);x[2]=rng.randrange(5);x[3]=rng.randrange(4);x[4]=rng.randrange(140);x[6]=rng.choice([0,1,5,9,15,0x200]);x[9]=rng.randrange(1024);rows.append(x)
 if a.case_file:rows=json.loads(a.case_file.read_text())
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 if a.replay_oracle:
  saved=json.loads(a.replay_oracle.read_text());assert saved['source_sha256']==SHA and saved['checker_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest();rows=saved['inputs'];expected=saved['outputs'];all_events=[[]for _ in rows]
 for index,row in enumerate([]if a.replay_oracle else rows):
  reads=writes=calls=0;trace=1469598103934665603;failed=False;restore=None;events=[];u.mem_write(0x501300,b'\x5a'*0x100);u.mem_write(0x1200000,b'\x5a'*0x400);u.mem_write(base,b'\x5a'*0x118);put(0x501320,row[1]);put(0x4cebc4,row[7]);put(0x4cebc8,0x11d16b35);put(0x4cebcc,0xc0008787);put(0x4cebd0,0x25af16f0);put(0x1200010,row[8]);put(0x1200110,(row[8]+1)&MASK)
  for i in range(1,4):put(0x1200010+i*4,(0x11223344*(i+1))&MASK);put(0x1200110+i*4,(0x22334455*(i+1))&MASK)
  for i in range(0x100):put(0x1200200+i,ord('a')+i%26,1)
  if row[4]<0x100:put(0x1200200+row[4],0,1)
  put(base+0x110,selected());put(base+0x114,password());active=True;result=0
  try:r.call(0x4b9080 if row[0]else 0x4b9090,*([]if row[0]else[selected(),password()]),count=100000);result=u.reg_read(UC_X86_REG_EAX)
  except(unicorn.UcError,RuntimeError):
   if not failed:raise
  finally:active=False
  if failed:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in[(0x501300,0x100),(0x1200000,0x400),(0x4cebc4,16),(base,0x100)]:
   for v in u.mem_read(at,n):state=((state^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events)
 if not a.replay_oracle:a.report.with_suffix('.oracle.json').write_text(json.dumps({'source_sha256':SHA,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'inputs':rows,'outputs':expected})+'\n')
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,(p.returncode,p.stderr);actual=p.stdout.splitlines();assert len(actual)==len(rows);bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i]}for i,(x,y)in enumerate(zip(actual,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'cases':len(rows),'differences':len(bad),'mismatches':bad[:8],'mismatch_summary':[{k:v for k,v in m.items()if k!='original_events'}for m in bad],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--cases',type=int,default=128);p.add_argument('--limit',type=int,default=0);p.add_argument('--case-file',type=Path);p.add_argument('--replay-oracle',type=Path);run(p.parse_args())
