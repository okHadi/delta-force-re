#!/usr/bin/env python3
"""Compare player removal with the exact supported DLL bytes."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,pefile,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EBX
from check_projectiles_original import Rig,STACK
SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2';MASK=0xffffffff
ROOT=[0x10001b10,0x10001b40,0x10001b80,0x10004510,0x10004550,0x100045c0,0x10004660]
SIZE=dict(zip(ROOT,[8,8,8,16,16,16,16]));ARGC=dict(zip(ROOT,[1,1,1,3,3,3,3]));RANGES=list(zip(ROOT,[0x10001b33,0x10001b7f,0x10001bd2,0x10004543,0x100045b6,0x100045f3,0x100046b8]))
for site,size,argc,end in[(0x100019e0,8,1,0x10001a15),(0x10003fe0,12,2,0x10004018),(0x100041d0,12,2,0x1000421c),(0x10005240,20,4,0x10005273),(0x10004220,20,4,0x10004244),(0x10003b20,28,6,0x10003c00),(0x10004490,8,1,0x1000450b),(0x10004600,8,1,0x10004652),(0x10002b80,0x68,2,0x10002d28)]:SIZE[site]=size;ARGC[site]=argc;RANGES.append((site,end))
IMPORTS={0x1100100:1,0x1100b00:1,0x1100600:1,0x1100c00:1,0x10005020:3,0x100042e0:2}
OFF={0x10002b80:-0x5c};SPANS={0x10002b80:[(0x10,0x5a),(0x60,0x68)]}
def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA;image=pefile.PE(data=raw).get_memory_mapped_image();r=Rig(a.game);u=r.u;u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,image);md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls=set();returns={}
 for lo,hi in RANGES:
  for i in md.disasm(image[lo-0x10000000:hi-0x10000000],lo):
   if i.mnemonic in['push','pop','call','ret']:controls.add(i.address)
   if i.mnemonic=='ret':returns[i.address]=lo
 assert image[0x44fc:0x44fe]==bytes.fromhex('f3ab') and image[0x4610:0x4612]==bytes.fromhex('f3ab')
 active=failed=False;row=[];frames=[];events=[];serial=reads=writes=calls=allocs=sleeps=0;trace=0;restore=None
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,v,n=4):u.mem_write(at,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
 def input_at():return 0x1240000 if row[18]==1 else 0x1201000 if row[18]==2 else 0x1200000
 def string(at,text):u.mem_write(at,text.encode()+b'\0')
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
  if row[15]&1 and s==0x100019e0 and frame(0x10001b10):put(frame(0x10001b10)+4,0x10002)
  if row[15]&2 and s==0x100041d0 and frame(0x10004550):put(frame(0x10004550)+4,0x1221000)
  if row[15]&8 and s==0x10002b80:
   put(0x12400a4,0x1260018)
   if frame(0x10004550):put(frame(0x10004550)+8,0x0200007f);put(frame(0x10004550)+0xc,0x77771234)
  if s==0x100042e0:stop();return False
  return True
 def foreign(s):
  nonlocal allocs,sleeps
  sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(IMPORTS[s])]
  if not ce(s,args):return
  result=0
  if s==0x1100100:result=0x12a0000+allocs*0x10000;allocs+=1
  elif s in[0x1100600,0x1100c00]:
   sleeps+=1
   if sleeps>=row[10]:put(0x10019bfc,1)
   if sleeps>10:stop();return
  elif s==0x10005020:result=row[9]
  r.finish(result)
  if s in[0x1100600,0x1100c00]:u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+4)
 for s in IMPORTS:r.stubs[s]=lambda s=s:foreign(s)
 def code(uc,pc,n,_):
  nonlocal serial
  if not active or failed:return
  if pc in SIZE and not(pc==ROOT[row[0]]and u.reg_read(UC_X86_REG_ESP)==STACK):
   sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(ARGC[pc])]
   if not ce(pc,args):return
   frames.append((pc,sp+OFF.get(pc,0),SIZE[pc],serial));serial+=1
  if pc in returns and len(frames)>1 and frames[-1][0]==returns[pc]:frames.pop()
 u.hook_add(unicorn.UC_HOOK_CODE,code)
 def memory(uc,kind,at,n,v,_):
  nonlocal reads,writes,restore
  if not active or failed:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not any(lo<=pc<hi for lo,hi in RANGES):return
  if 0x2000000<=at<0x2010000:
   if pc in controls:return
   if not any(base+lo<=at and at+n<=base+hi for s,base,size,idx in frames for lo,hi in SPANS.get(s,[(4,size)])):return
  mapped=any(base<=at and at+n<=base+size for base,size in[(0x10000000,0x1b000),(0x1200000,0x100000)]+[(base,size)for s,base,size,idx in frames])
  if kind==unicorn.UC_MEM_WRITE:
   v&=(1<<(8*n))-1;events.append(['W',norm(at),n,norm(v)]);event(2);event(norm(at));event(n);event(norm(v));writes+=1
   if writes==row[13]or not mapped:
    try:restore=(at,get(at,n),n)
    except unicorn.UcError:pass
    stop();return
   if row[15]&16 and at==0x1228004:put(0x10019c5c,0x1229000)
   if row[15]&128 and at==0x1240000:put(0x100198b8,1)
  else:
   events.append(['R',norm(at),n]);event(1);event(norm(at));event(n);reads+=1
   if reads==row[12]or not mapped:stop();return
   if row[15]&4 and at==0x12400a0:put(0x1220004,0x10000)
   if row[15]&32 and at==0x100199b0 and frame(0x10001b80):put(frame(0x10001b80)+4,0x10002)
   if row[15]&64 and at==0x10019bfc:put(0x10006010,0x1100c00)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,n,v,user):
  if not failed:memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,n,v,user);stop()
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def base(op):return[op,3,3,1,0x10000,7,0x10000,1,0,0,2,0,0,0,0,0,0x5a,0,0,1,1,1,1,0x1234]
 rows=[]
 for op in range(7):
  for field,values in[(1,[0,1,3]),(2,[0,1,2,3]),(3,[0,1,2]),(4,[0,0xffff,0x10000,0x10001,0x10002,0x10003,0xffffffff]),(6,[0,0x10000,0x10001,0xffffffff]),(7,[0,1]),(9,[0,1,0xffffffff]),(10,[1,2,3]),(11,[0,1,0xffffffff]),(15,[0,1,2,4,8,16,32,64,128]),(17,[0,1]),(19,[0,1,2]),(20,[0,1]),(21,[0,1]),(22,[0,1]),(23,[0x1234,0xa5a51234])]:
   for v in values:x=base(op);x[field]=v;rows.append(x)
  for field in[12,13]:
   for fail in[1,2,3,4,8,16,32,48,64,96,128,160]:x=base(op);x[field]=fail;rows.append(x)
  for fail in range(1,10):x=base(op);x[14]=fail;rows.append(x)
 rng=random.Random(0x10004550)
 for _ in range(a.cases):
  x=base(rng.randrange(7));x[1]=rng.randrange(4);x[2]=rng.randrange(4);x[3]=rng.randrange(3);x[4]=0xffff+rng.randrange(5);x[15]=rng.choice([0,1,2,4,8,16,32,64,128]);rows.append(x)
 if a.case_file:rows=json.loads(a.case_file.read_text())
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 if a.replay_oracle:
  saved=json.loads(a.replay_oracle.read_text());assert saved['source_sha256']==SHA and saved['checker_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest();rows=saved['inputs'];expected=saved['outputs'];all_events=[[]for _ in rows]
 for index,row in enumerate([]if a.replay_oracle else rows):
  serial=1;reads=writes=calls=allocs=sleeps=0;trace=1469598103934665603;failed=False;restore=None;events=[];u.mem_write(0x1200000,b'\x5a'*0x100000);u.mem_write(0x10019880,bytes(0x426));u.mem_write(STACK-0x2000,b'\x5a'*0x4000)
  for at,v in[(0x10019880,row[20]),(0x10019884,row[21]),(0x100198ac,row[22]),(0x100198b8,row[1]),(0x100198bc,2),(0x100199b0,row[3]),(0x10019afc,0x1240000),(0x10019af8,0x1260000),(0x10019c5c,0x1228000),(0x10019c68,row[17]),(0x10019b0c,0x1280000),(0x10019b10,0x1280000),(0x10019b14,0x1280000),(0x1000605c,0x1100100),(0x10006068,0x1100b00),(0x10006010,0x1100600),(0x10007698,0)]:put(at,v)
  u.mem_write(0x1240000,bytes(0x10000));u.mem_write(0x1260000,bytes(0x1000));u.mem_write(0x1280000,bytes(0x10000))
  for i in range(row[1]):
   at=0x1240000+i*0xbc;put(at+0x9c,int(row[2]==2 or row[2]in[1,3]and i==0));put(at+0x98,int(row[2]==3));put(at+0xa0,0x10000+i);put(at+0xa4,0x1260000 if row[19]==2 else 0x1260000+i*0x18);put(at+0x80,0x1204000+i*0x100 if row[7]else 0);put(at+0x84,7)
  for i in range(row[1]):
   at=0x1260000+i*0x18;put(at,int(bool(row[19])));put(at+4,0x0100007f+i);put(at+0xc,1);put(at+0x14,row[23])
  put(0x1220004,row[6]);put(0x1221004,0x10002)
  op=row[0];s=ROOT[op];frames=[(s,STACK,SIZE[s],0)]
  args=[row[4]]if op<3 else[row[11],0x0100007f,row[23]]if op in[3,5]else[0x1220000,0x0100007f,row[23]]
  u.reg_write(UC_X86_REG_EBX,(row[16]<<24)|0x005a5a5a);active=True;result=0
  try:r.call(s,*args,count=500000);result=u.reg_read(UC_X86_REG_EAX)
  except(unicorn.UcError,RuntimeError):
   if not failed:raise RuntimeError((index,row,hex(u.reg_read(UC_X86_REG_EIP)),events[-15:]))
  finally:active=False
  if failed or op>=3:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in[(0x10019880,0x426),(0x1200000,0x6000),(0x1220000,0x10000),(0x1240000,0x10000),(0x1260000,0x1000),(0x1280000,0x1000),(0x12a0000,0x20000)]:
   for v in u.mem_read(at,n):state=((state^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events if a.limit==1 else events[:300])
 if not a.replay_oracle:a.report.with_suffix('.oracle.json').write_text(json.dumps({'source_sha256':SHA,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'inputs':rows,'outputs':expected})+'\n')
 p=subprocess.run([str(a.probe),str(a.game/'Netsocknl.dll')],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,(p.returncode,p.stderr);actual=p.stdout.splitlines();assert len(actual)==len(rows);bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i]}for i,(x,y)in enumerate(zip(actual,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'cases':len(rows),'differences':len(bad),'mismatches':bad[:8],'mismatch_summary':[{k:v for k,v in m.items()if k!='original_events'}for m in bad],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--cases',type=int,default=128);p.add_argument('--limit',type=int,default=0);p.add_argument('--case-file',type=Path);p.add_argument('--replay-oracle',type=Path);run(p.parse_args())
