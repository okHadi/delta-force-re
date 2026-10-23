#!/usr/bin/env python3
"""Compare host discovery/admission with the exact supported DLL bytes."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,pefile,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,STACK
SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2';MASK=0xffffffff
ROOT=[0x10003f00,0x100040c0,0x10003ed0,0x10003f80,0x10004090];SIZE=dict(zip(ROOT,[16,16,12,20,12]));ARGC=dict(zip(ROOT,[3,3,2,4,2]));RANGES=list(zip(ROOT,[0x10003f7f,0x10004185,0x10003ef5,0x10003fda,0x100040b5]))
for site,size,argc,end in[(0x10003fe0,12,2,0x10004017),(0x10004050,12,2,0x1000408b),(0x10004020,16,3,0x10004041),(0x10005240,20,4,0x10005272)]:SIZE[site]=size;ARGC[site]=argc;RANGES.append((site,end))
IMPORTS={0x10005020:3,0x10004600:1}
def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA;image=pefile.PE(data=raw).get_memory_mapped_image();r=Rig(a.game);u=r.u;u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,image);md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls=set();returns={}
 for lo,hi in RANGES:
  for i in md.disasm(image[lo-0x10000000:hi-0x10000000],lo):
   if i.mnemonic in['push','pop','call','ret']:controls.add(i.address)
   if i.mnemonic=='ret':returns[i.address]=lo
 assert image[0x3f15:0x3f17]==bytes.fromhex('f3a7')and image[0x3f4b:0x3f4d]==bytes.fromhex('f3a5')and image[0x40d5:0x40d7]==bytes.fromhex('f3a7')
 active=failed=False;row=[];frames=[];events=[];serial=reads=writes=calls=0;trace=0;restore=None
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,v,n=4):u.mem_write(at,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
 def input_at():return 0x1220000 if row[8]else 0x1200000
 def frame(s):return next((base for site,base,n,idx in reversed(frames)if site==s),0)
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
  if row[11]&4 and s==0x10003ed0:
   put(0x10019c5c,0x1224000)
   if frame(0x10003f00):put(frame(0x10003f00)+8,0x0500007f);put(frame(0x10003f00)+12,0x99998888)
  if row[11]&8 and s==0x10004090 and frame(0x100040c0):put(frame(0x100040c0)+8,0x0600007f);put(frame(0x100040c0)+12,0x77776666)
  if row[11]&256 and s==0x10003fe0 and frame(0x10004090):put(frame(0x10004090)+4,0x0700007f);put(frame(0x10004090)+8,0x55554444)
  if s==0x10004600:stop();return False
  return True
 def foreign(s):
  sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(IMPORTS[s])]
  if not ce(s,args):return
  if s==0x10005020:r.finish(row[7]);return
  stop()
 for s in IMPORTS:r.stubs[s]=lambda s=s:foreign(s)
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
   if row[11]&16 and at in[0x1220004,0x1224004]:put(0x10019c5c,0x1224000);put(at-2,0x108,2)
  else:
   events.append(['R',norm(at),n]);event(1);event(norm(at));event(n);reads+=1
   if reads==row[12]or not mapped:stop();return
   if row[11]&1 and at==0x10019990:put(input_at()+4,0xdeadbeef)
   if row[11]&2 and at==input_at()+4:put(0x10019990,0xdeadbeef)
   if row[11]&32 and frame(0x10003f80)and at==frame(0x10003f80)+0x10:put(frame(0x10003f80)+0xc,0x0300007f)
   if row[11]&64 and at==0x10019b00:put(0x12600a0,0x20000)
   if row[11]&128 and at==0x12600a0:put(0x10019b00,0)
   if row[11]&1024 and at==input_at()+0x14:put(0x10019940,ord('z'),1)
   if row[11]&2048 and at==0x10019940:put(input_at()+0x14,ord('z'),1)
   if row[11]&4096 and at==0x100198b4 and frame(0x100040c0):put(frame(0x100040c0)+8,0x0400007f);put(frame(0x100040c0)+12,0xabcdef01)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,n,v,user):
  if not failed:memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,n,v,user);stop()
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def base(op):return[op,1,1,1,3,0,1,0,0,1,0,0,0,0,0,0x0100007f,0xa5a51234,0x10000,0,2,0x1020,0,0,0]
 rows=[]
 for op in range(5):
  for field,values in[(1,[0,1,2]),(2,[0,1]),(3,[0,1,2]),(4,[0,1,3,4,31]),(5,[0,1,2,3,4]),(6,[0,1]),(7,[0,1,MASK]),(8,[0,1]),(9,[0,1,2,0x12340001]),(10,[0,1,2,3,0xabcd0002]),(11,[0,1,2,4,8,16,32,64,128,256,1024,2048,4096,4382]),(12,list(range(1,24))+[32,64,128]),(13,list(range(1,24))+[32,64,80]),(14,range(1,8)),(19,[0,1,2,3,15,31,63]),(20,[0,0x1010,0x1020,0xffff])]:
   for v in values:x=base(op);x[field]=v;rows.append(x)
 for mode in[0,16,4,20,2]:
  for field in[12,13]:
   for fail in[100,120,128,130,140,150,160]:x=base(0);x[11]=mode;x[field]=fail;rows.append(x)
 rng=random.Random(0x100040c0)
 for _ in range(a.cases):
  x=base(rng.randrange(5));x[1]=rng.randrange(3);x[2]=rng.randrange(2);x[3]=rng.randrange(3);x[4]=rng.randrange(5);x[5]=rng.randrange(5);x[6]=rng.randrange(2);x[11]=rng.choice([0,2,4,8,16,32,64,128,256,1024,2048,4096,4382]);rows.append(x)
 if a.case_file:rows=json.loads(a.case_file.read_text())
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 if a.replay_oracle:
  saved=json.loads(a.replay_oracle.read_text());assert saved['source_sha256']==SHA and saved['checker_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest();rows=saved['inputs'];expected=saved['outputs'];all_events=[[]for _ in rows]
 for index,row in enumerate([]if a.replay_oracle else rows):
  serial=1;reads=writes=calls=0;trace=1469598103934665603;failed=False;restore=None;events=[];u.mem_write(0x1200000,b'\x5a'*0x100000);u.mem_write(0x10019880,bytes(0x426));u.mem_write(STACK-0x2000,b'\x5a'*0x4000)
  for at,v in[(0x100198b0,0x100),(0x100198b4,8 if row[2]else 0),(0x100198b8,row[4]),(0x10019c5c,0x1220000),(0x10019af8,0x1240000),(0x10019b00,0x1260000 if row[6]else 0),(0x12600a0,row[17])]:put(at,v)
  put(0x10019868,0x4321,2)
  for i in range(row[19]):put(0x10019940+i,ord('a')+i%26,1);put(input_at()+0x14+i,ord('a')+i%26,1)
  put(0x10019940+row[19],0,1);put(input_at()+0x14+row[19],0,1)
  if not row[3]:put(input_at()+0x14,ord('z'),1)
  if row[3]==2:put(input_at()+0x15,ord('z'),1)
  for j in range(4):key=0x11111111*(j+1);put(0x10019990+j*4,key);put(input_at()+4+j*4,key+int(not row[1]and j==0 or row[1]==2 and j==3))
  put(input_at(),row[20],2);put(input_at()+2,0x54,2);u.mem_write(0x1240000,bytes(0x8000))
  for i in range(row[4]):
   at=0x1240000+i*0x18;put(at,int(row[5]==3 or row[5]in[1,2]and i==0 or row[5]==4 and i==1));put(at+4,row[15]+1+i if row[5]==3 else row[15]);put(at+0xc,1 if row[5]==2 else 0);put(at+0x14,row[16])
  op=row[0];s=ROOT[op];frames=[(s,STACK,SIZE[s],0)];args=[input_at(),row[15],row[16]]if op in[0,1]else[row[9],row[10],row[15],row[16]]if op==3 else[row[15],row[16]];active=True;result=0
  try:r.call(s,*args,count=500000);result=u.reg_read(UC_X86_REG_EAX)
  except(unicorn.UcError,RuntimeError):
   if not failed:raise
  finally:active=False
  if failed or op!=4:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in[(0x10019880,0x426),(0x1200000,0x1000),(0x1220000,0x8000),(0x1240000,0x8000),(0x1260000,0x1000)]:
   for v in u.mem_read(at,n):state=((state^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events if a.limit==1 else events[:300])
 if not a.replay_oracle:a.report.with_suffix('.oracle.json').write_text(json.dumps({'source_sha256':SHA,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'inputs':rows,'outputs':expected})+'\n')
 p=subprocess.run([str(a.probe),str(a.game/'Netsocknl.dll')],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,(p.returncode,p.stderr);actual=p.stdout.splitlines();assert len(actual)==len(rows);bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i]}for i,(x,y)in enumerate(zip(actual,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'cases':len(rows),'differences':len(bad),'mismatches':bad[:8],'mismatch_summary':[{k:v for k,v in m.items()if k!='original_events'}for m in bad],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--cases',type=int,default=128);p.add_argument('--limit',type=int,default=0);p.add_argument('--case-file',type=Path);p.add_argument('--replay-oracle',type=Path);run(p.parse_args())
