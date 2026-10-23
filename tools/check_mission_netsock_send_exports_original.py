#!/usr/bin/env python3
"""Compare the raw send exports, player lookup and actual queue instructions."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,pefile,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,STACK
SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2';MASK=0xffffffff
ROOT=[0x10001cf0,0x10001dc0,0x100019e0];SIZE={ROOT[0]:16,ROOT[1]:12,ROOT[2]:8,0x10003b20:0x1c};ARGC={ROOT[0]:3,ROOT[1]:2,ROOT[2]:1,0x10003b20:6};RANGES=[(ROOT[0],0x10001dbc),(ROOT[1],0x10001e6b),(ROOT[2],0x10001a15),(0x10003b20,0x10003c00)]
def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA;image=pefile.PE(data=raw).get_memory_mapped_image();r=Rig(a.game);u=r.u;u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,image);md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls=set();returns={}
 for lo,hi in RANGES:
  for i in md.disasm(image[lo-0x10000000:hi-0x10000000],lo):
   if i.mnemonic in['push','pop','call','ret']:controls.add(i.address)
   if i.mnemonic=='ret':returns[i.address]=lo
 active=failed=False;row=None;frames=[];events=[];heaps=[];serial=reads=writes=calls=allocs=next_at=0;trace=0;restore=None
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,v,n=4):u.mem_write(at,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
 def source():return 0 if row[5]==0 else 0x1220000 if row[5]==2 else 0x1220004 if row[5]==3 else 0x1200000
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
  if row[11]&2 and s==0x10003b20:put(0x1220002,1,2)
  if row[11]&64 and s==0x100019e0:put(0x10019afc,0x12400bc)
  return True
 imports={0x1100100:1,0x1100200:1,0x1100300:1,0x10003aa0:2}
 def foreign(s):
  nonlocal allocs,next_at
  sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(imports[s])]
  if not ce(s,args):return
  result=0
  if s==0x1100100:
   allocs+=1
   if allocs!=row[15]:
    n=args[0]
    if n>0x10000 or next_at+n>0x12f0000:stop();return
    result=next_at;next_at+=(n+255)&~255;heaps.append((result,n))
    if row[11]&32:put(0x1220008,0xdeadbeef)
  elif s==0x1100300:put(0x10019b30,0);put(0x10019c48,0)
  elif s!=0x1100200:stop();return
  r.finish(result)
  if s==0x1100300:u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+4)
 for s in imports:r.stubs[s]=lambda s=s:foreign(s)
 def code(uc,pc,n,_):
  nonlocal serial
  if not active or failed:return
  if pc in SIZE and not(pc==ROOT[row[0]]and u.reg_read(UC_X86_REG_ESP)==STACK):
   sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(ARGC[pc])]
   if not ce(pc,args):return
   frames.append((pc,sp,SIZE[pc],serial));serial+=1
  if pc in returns and len(frames)>1 and frames[-1][0]==returns[pc]:frames.pop()
 u.hook_add(unicorn.UC_HOOK_CODE,code)
 def memory(uc,kind,at,n,v,_):
  nonlocal reads,writes,restore
  if not active or failed:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not any(lo<=pc<hi for lo,hi in RANGES):return
  if 0x2000000<=at<0x2010000:
   if pc in controls:return
   if not any(base+4<=at and at+n<=base+size for s,base,size,idx in frames):return
  mapped=any(base<=at and at+n<=base+size for base,size in[(0x10000000,0x1b000),(0x1200000,0x100000)]+[(base,size)for s,base,size,idx in frames])
  if kind==unicorn.UC_MEM_WRITE:
   v&=(1<<(8*n))-1;events.append(['W',norm(at),n,norm(v)]);event(2);event(norm(at));event(n);event(norm(v));writes+=1
   if writes==row[13]or not mapped:
    try:restore=(at,get(at,n),n)
    except unicorn.UcError:pass
    stop();return
   if row[11]&128 and at==0x1220002:put(frames[0][1]+8,row[6]+7);put(frames[0][1]+4,source()+64)
  else:
   events.append(['R',norm(at),n]);event(1);event(norm(at));event(n);reads+=1
   if reads==row[12]or not mapped:stop();return
   if row[11]&1 and at==source()+4:put(0x10019c60,0x1230000);put(0x12400a4,0x1250018)
   if row[11]&4 and at==0x1250014:put(0x1220002,8,2)
   if row[11]&8 and at==0x10019b38:put(0x10019c70,row[10]^MASK)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,n,v,user):
  if not failed:memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,n,v,user);stop()
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def base(op):return[op,1,1,1,2 if op==1 else 1,1,17,0x10000,2,1,0xabcd1234,0,0,0,0,0,0,0,0,0x5a]
 rows=[]
 for op in range(3):
  for field,values in[(1,[0,1]),(2,[0,1]),(3,[0,1]),(4,[0,1,2,3,MASK]),(5,[0,1,2,3]),(6,[0,1,2,3,4,5,7,8,15,16,17,0xfff3,0xfff4,0xfff5,0x10000,MASK]),(7,[0,0xffff,0x10000,0x10001,0x10002,MASK]),(8,[0,1,2,MASK]),(9,[0,1,0x80000000]),(10,[0,0x1234,0xabcd1234,MASK]),(11,[0,1,2,4,8,32,64,128,255]),(12,list(range(1,25))+[30,50,100]),(13,list(range(1,20))+[30,50]),(14,list(range(1,6))),(15,[1]),(16,[0,0x800,0xff5,0xff6,0x80000000,MASK]),(17,[0,1])]:
   for v in values:x=base(op);x[field]=v;rows.append(x)
 rng=random.Random(0x10001cf0)
 for _ in range(a.cases):
  x=base(rng.randrange(3));x[5]=rng.randrange(4);x[6]=rng.randrange(65);x[7]=rng.choice([0x10000,0x10001,0x10002,MASK]);x[8]=rng.randrange(4);x[9]=rng.randrange(2);x[10]=rng.getrandbits(32);x[11]=rng.randrange(256);rows.append(x)
 if a.case_file:rows=json.loads(a.case_file.read_text())
 if a.op is not None:rows=[x for x in rows if x[0]==a.op]
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 if a.replay_oracle:
  saved=json.loads(a.replay_oracle.read_text());assert saved['source_sha256']==SHA and saved['checker_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest();rows=saved['inputs'];expected=saved['outputs'];all_events=[[]for _ in rows]
 for index,row in enumerate([]if a.replay_oracle else rows):
  if index%32==0:a.report.with_suffix('.progress.json').write_text(json.dumps({'case':index,'total':len(rows)})+'\n')
  serial=1;reads=writes=calls=allocs=0;next_at=0x1280000;trace=1469598103934665603;failed=False;restore=None;events=[];heaps=[];u.mem_write(0x1200000,bytes([row[19]])*0x100000);u.mem_write(0x10019880,bytes(0x426));u.mem_write(STACK-0x1000,b'\x5a'*0x2000)
  for at,v in[(0x10019880,row[1]),(0x10019884,row[2]),(0x100198ac,row[3]),(0x100199b0,row[4]),(0x10019c60,0x1220000),(0x100198b8,row[8]),(0x10019afc,0x1240000),(0x10019c70,row[10]),(0x10019b38,0x0100007f),(0x1000605c,0x1100100),(0x10006068,0x1100200),(0x10006010,0x1100300)]:put(at,v)
  for q in[0x10019b20,0x10019c38]:
   for off,v in[(0,0x1260000),(4,0x1260000),(8,0x1260000),(12,row[16]),(16,row[17])]:put(q+off,v)
  u.mem_write(0x1260000,bytes(0x10000))
  for i in range(4):put(0x1240000+i*0xbc+0x9c,row[9]);put(0x1240000+i*0xbc+0xa4,0x1250000+i*0x18);put(0x1250000+i*0x18+4,0x0100007f+i);put(0x1250000+i*0x18+0x14,row[10]+i)
  u.mem_write(0x1200000,bytes((i*17+3)&255 for i in range(0x10000)));op=row[0];site=ROOT[op];frames=[(site,STACK,SIZE[site],0)];args=[row[7]]if op==2 else[source(),row[6]]if op==1 else[source(),row[6],row[7]];active=True;result=0
  try:r.call(site,*args,count=4000000);result=u.reg_read(UC_X86_REG_EAX)
  except(unicorn.UcError,RuntimeError):
   if not failed:raise
  finally:active=False
  if failed:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in[(0x10019880,0x426),(0x1220000,0x20000),(0x1240000,0x1000),(0x1250000,0x100),(0x1260000,0x10000)]+heaps:
   for v in u.mem_read(at,n):state=((state^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events if a.limit==1 else events[:300])
 if not a.replay_oracle:a.report.with_suffix('.oracle.json').write_text(json.dumps({'source_sha256':SHA,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'inputs':rows,'outputs':expected})+'\n')
 p=subprocess.run([str(a.probe),str(a.game/'Netsocknl.dll')],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,(p.returncode,p.stderr);actual=p.stdout.splitlines();assert len(actual)==len(rows);bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i]}for i,(x,y)in enumerate(zip(actual,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'cases':len(rows),'differences':len(bad),'mismatches':bad[:8],'mismatch_summary':[{k:v for k,v in m.items()if k!='original_events'}for m in bad],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--cases',type=int,default=128);p.add_argument('--limit',type=int,default=0);p.add_argument('--op',type=int);p.add_argument('--case-file',type=Path);p.add_argument('--replay-oracle',type=Path);run(p.parse_args())
