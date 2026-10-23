#!/usr/bin/env python3
"""Compare the client worker's source reads, stores and required-call prefixes."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,pefile,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,STACK
SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2';MASK=0xffffffff
SITES=[0x10001fe0,0x10002e00,0x10002e40,0x10002e70,0x10002ea0,0x10002ec0,0x10002ee0,0x10002f00,0x10002f20,0x10002f50,0x100030a0,0x10003130,0x10003170,0x100031a0,0x100031c0,0x10003210,0x10003260,0x10003290,0x10003300,0x10003470]
SIZE=dict(zip(SITES,[12,8,20,12,8,8,8,8,4,8,4,4,4,4,4,4,8,12,12,4]));ARGC=dict(zip(SITES,[2,1,4,2,1,1,1,1,0,1,0,0,0,0,0,0,1,0,0,0]));LOCAL={s:12 if s in[0x10003290,0x10003300]else 0 for s in SITES};FRAMELESS={s for s in SITES if SIZE[s]==4};SPANS={s:([(0,12)]if LOCAL[s]else[(4,SIZE[s])])for s in SITES}
ENDS=[0x10002095,0x10002e3b,0x10002e64,0x10002e9c,0x10002ebe,0x10002ed8,0x10002efe,0x10002f18,0x10002f4a,0x10003011,0x10003123,0x10003161,0x10003198,0x100031bf,0x10003210,0x10003260,0x10003287,0x100032ff,0x1000346c,0x1000349e]
RANGES=list(zip(SITES,ENDS))+[(0x10005830,0x10005854),(0x10003c00,0x10003cbf),(0x100020d0,0x10002176),(0x10003ad0,0x10003b20),(0x100052e0,0x1000565e)]
for site,n,loc,argc in [(0x10005830,4,4,0),(0x10003c00,0x18,0,5),(0x100020d0,0x10,8,1),(0x10003ad0,8,0,1),(0x100052e0,0x48,0x30,5)]:
 SIZE[site]=n;LOCAL[site]=loc;ARGC[site]=argc;SPANS[site]=[(0,4)]if site==0x10005830 else[(0,8),(12,16)]if site==0x100020d0 else[(0x10,0x30),(0x34,0x48)]if site==0x100052e0 else[(4,n)]
EXTERNAL={0x10003a60:1,0x10003ad0:1,0x10003b20:6,0x10003c00:5,0x10005240:4,0x10005020:3,0x10002180:1,0x100021d0:0,0x100020d0:1,0x10005830:0,0x100052e0:5,0x1100400:0,0x1100401:0,0x1100500:1,0x1100501:1,0x1100600:0,0x10005934:3,0x1000595e:6,0x10005916:0,0x1100700:0,0x1100701:0}
def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA;image=pefile.PE(data=raw).get_memory_mapped_image();r=Rig(a.game);u=r.u;u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,image);md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls=set();returns={}
 for lo,hi in RANGES:
  for i in md.disasm(image[lo-0x10000000:hi-0x10000000],lo):
   if i.mnemonic in['push','pop','call','ret']:controls.add(i.address)
   if i.mnemonic=='ret':returns[i.address]=lo
 active=failed=False;row=None;frames=[];events=[];serial=reads=writes=calls=clocks=packetreads=sleeps=pending=0;trace=0;restore=None
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,v,n=4):u.mem_write(at,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
 def norm(at):
  for s,base,n,idx in reversed(frames):
   if base<=at<base+n:return 0x30000000+idx*0x1000+at-base
  return at
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
  if calls==row[14]:stop();return False
  return True
 def foreign(s):
  nonlocal clocks,sleeps,pending
  sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(EXTERNAL[s])]
  if not ce(s,args):return
  if row[11]&16:
   if s==0x10005934:put(args[2],5 if pending else 0);r.finish(0);u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+12);return
   if s in[0x1100500,0x1100501]:
    if row[11]&64:put(0x10006010,0x1100501)
    if row[11]&256:pending=1
    sleeps+=1
    if sleeps>=1+row[10]%3:put(0x10019b40,1)
    r.finish(0);u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+4);return
   if s==0x1100600:r.finish(0);return
   if s==0x1000595e:pending=0;r.finish(MASK);u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+24);return
   if s==0x10005916:r.finish(0x2733);return
   if s in[0x1100700,0x1100701]:
    if row[11]&1024:put(0x10006044,0x1100701);put(0x10019ca2,MASK)
    r.finish(row[8]);return
  if s not in[0x1100400,0x1100401]:stop();return
  clocks+=1
  if row[11]&16 and row[11]&32:put(0x10006088,0x1100401)
  if row[11]&4:put(0x10019880,row[1]^1);put(0x10019c92,row[8]+7)
  r.finish((row[15]+(clocks-1)*(row[7]if row[11]&16 else 1))&MASK)
 for s in EXTERNAL:
  if s not in SIZE:r.stubs[s]=lambda s=s:foreign(s)
 def code(uc,pc,n,_):
  nonlocal serial
  if not active or failed:return
  if pc in SIZE and not(pc==SITES[row[0]]and u.reg_read(UC_X86_REG_ESP)==STACK):
   sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(ARGC[pc])]
   if not ce(pc,args):return
   if pc not in SITES and not row[11]&16:stop();return
   if pc not in FRAMELESS:
    at=sp-LOCAL[pc]
    if LOCAL[pc]:u.mem_write(at,b'\x5a'*LOCAL[pc])
    frames.append((pc,at,SIZE[pc],serial));serial+=1
  if pc in returns and len(frames)>1 and frames[-1][0]==returns[pc]:frames.pop()
 u.hook_add(unicorn.UC_HOOK_CODE,code)
 def memory(uc,kind,at,n,v,_):
  nonlocal reads,writes,restore,packetreads
  if not active or failed:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not any(lo<=pc<hi for lo,hi in RANGES):return
  if 0x2000000<=at<0x2010000:
   if pc in controls:return
   if not any(any(base+lo<=at and at+n<=base+hi for lo,hi in SPANS[s])for s,base,size,idx in frames):return
  mapped=any(base<=at and at+n<=base+size for base,size in[(0x10000000,0x1b000),(0x1200000,0x100000)]+[(base,size)for s,base,size,idx in frames])
  if kind==unicorn.UC_MEM_WRITE:
   v&=(1<<(8*n))-1;events.append(['W',norm(at),n,norm(v)]);event(2);event(norm(at));event(n);event(norm(v));writes+=1
   if writes==row[13]or not mapped:
    try:restore=(at,get(at,n),n)
    except unicorn.UcError:pass
    stop()
  else:
   events.append(['R',norm(at),n]);event(1);event(norm(at));event(n);reads+=1
   if reads==row[12]or not mapped:stop();return
   if row[11]&1 and at==0x1280004:
    packetreads+=1
    if packetreads==2:put(at,row[9]+1)
   if row[11]&2 and at==0x1280002:put(0x10019c5c,0x1280100)
   if row[11]&8 and at==0x10019c18:put(0x10019c54,1)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,n,v,user):
  if not failed:memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,n,v,user);stop()
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def base(op):return[op,1,1,1,0,1,0,0,0x04030201,0,0,0,0,0,0,0x12345678]
 rows=[]
 for op in range(20):
  for field,values in[(1,[0,1]),(2,[0,1]),(3,[0,1]),(4,[0,1]),(5,[0,1]),(6,[0,1]),(7,[0,25,26,50,51,100,101,0xffff,0x80000000,MASK]),(8,[0,0x04030201,MASK]),(9,[0,74,75,76,MASK]),(10,[0,0x1012,0x1031,0x1033,0x1041,0x1052,0x1053,0x1080,0xffff]),(11,[0,1,2,4,8,15]),(12,[1,2,3,5,10,20,50,100]),(13,[1,2,3,5,10,20]),(14,[1,2,3,4,5])]:
   for value in values:x=base(op);x[field]=value;rows.append(x)
 for delta in[0,1000,1001,0xffffffff]:
  for now in[0,0xfffffff0,0x7ffffff0]:
   for mask in[16,48,80,112]:
    x=base(18);x[5]=0;x[6]=0;x[7]=delta;x[9]=0;x[10]=2;x[11]=mask;x[15]=now;rows.append(x)
 for mask in[272,784,1808]:
  for random_value in[0,1,0x7fffffff,0x80000000,MASK]:
   x=base(18);x[5]=0;x[6]=0;x[7]=1;x[8]=random_value;x[9]=0;x[10]=2;x[11]=mask;rows.append(x)
 for field,values in[(12,range(1,81)),(13,range(1,31)),(14,range(1,41))]:
  for value in values:
   x=base(18);x[5]=0;x[6]=0;x[7]=1;x[9]=0;x[10]=2;x[11]=1808;x[field]=value;rows.append(x)
 for age in[15,16,119,120,MASK]:
  x=base(18);x[5]=0;x[6]=0;x[7]=1001;x[9]=age;x[10]=2;x[11]=16;rows.append(x)
 rng=random.Random(0x10003300)
 for _ in range(a.cases):
  x=base(rng.randrange(20));x[5]=rng.randrange(2);x[6]=rng.randrange(2);x[7]=rng.choice([0,25,26,51,101,MASK]);x[9]=rng.choice([0,75,76,MASK]);x[10]=rng.choice([0,0x1012,0x1031,0x1033,0x1041,0x1052,0x1080]);x[11]=rng.randrange(16);rows.append(x)
 if a.case_file:rows=json.loads(a.case_file.read_text())
 if a.op is not None:rows=[x for x in rows if x[0]==a.op]
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 if a.replay_oracle:
  saved=json.loads(a.replay_oracle.read_text());assert saved['source_sha256']==SHA and saved['checker_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest();rows=saved['inputs'];expected=saved['outputs'];all_events=[[]for _ in rows]
 for row in []if a.replay_oracle else rows:
  serial=1;reads=writes=calls=clocks=packetreads=sleeps=0;pending=1 if row[11]&256 else 0;trace=1469598103934665603;failed=False;restore=None;events=[];u.mem_write(0x1200000,b'\x5a'*0x100000);u.mem_write(0x10019880,bytes(0x426));u.mem_write(STACK-0x1000,b'\x5a'*0x2000)
  for at,v in[(0x10006088,0x1100400),(0x10019880,row[1]),(0x10019884,row[2]),(0x100198ac,row[3]),(0x10019c88,row[4]),(0x10019c5c,0x1280000),(0x100199b4,row[8]),(0x10019b4c,row[5]),(0x10019bf8,row[5]),(0x10019c10,row[5]),(0x10019c54,row[6]),(0x10019bf0,row[7]),(0x10019c00,row[7]),(0x10019c18,row[7]),(0x10019c0c,row[9]),(0x10019c20,row[9]),(0x10019c84,row[6]),(0x10019c4c,row[8]),(0x10019b48,row[8]),(0x10019c04,row[8]),(0x10019bdc,row[10]),(0x10019bd8,0x1281000),(0x10019b40,row[6])]:put(at,v)
  if row[11]&16:
   put(0x10006010,0x1100500);put(0x10006048,0x1100600);put(0x10019c50,row[9]);put(0x10019c58,14);put(0x10006044,0x1100700)
   if row[11]&512:put(0x10019ca2,15)
  put(0x1280000,row[10],2);put(0x1280002,row[7],2);put(0x1280004,row[9]);put(0x1280008,row[8]);op=row[0];s=SITES[op];at=STACK-LOCAL[s];frames=[(s,at,SIZE[s],0)];args=[]
  if op==0:args=[row[8],row[7]]
  elif op==2:args=[0x1280000,row[7],row[8],row[9]]
  elif op in[1,3,5,7]:args=[0x1280000]
  elif op in[4,6,16]:args=[row[9]]
  elif op==9:args=[row[8]]
  active=True;result=0
  try:r.call(s,*args,count=2000000);result=u.reg_read(UC_X86_REG_EAX)
  except(unicorn.UcError,RuntimeError):
   if not failed:raise
  finally:active=False
  if failed or op not in[0,18,19]:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in[(0x10019880,0x426),(0x1280000,0x2000),(0x10006088,4)]:
   for v in u.mem_read(at,n):state=((state^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events if a.limit==1 else events[:300])
 if not a.replay_oracle:a.report.with_suffix('.oracle.json').write_text(json.dumps({'source_sha256':SHA,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'inputs':rows,'outputs':expected})+'\n')
 p=subprocess.run([str(a.probe),str(a.game/'Netsocknl.dll')],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,(p.returncode,p.stderr);actual=p.stdout.splitlines();assert len(actual)==len(rows);bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i]}for i,(x,y)in enumerate(zip(actual,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'cases':len(rows),'differences':len(bad),'mismatches':bad[:8],'mismatch_summary':[{k:v for k,v in m.items()if k!='original_events'}for m in bad],'network_output':0,'scope':'Own twenty instruction bodies; external calls stop before side effects. Native integration proves reached existing bodies.'},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--cases',type=int,default=128);p.add_argument('--limit',type=int,default=0);p.add_argument('--op',type=int);p.add_argument('--case-file',type=Path);p.add_argument('--replay-oracle',type=Path);run(p.parse_args())
