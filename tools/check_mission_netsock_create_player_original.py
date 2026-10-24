#!/usr/bin/env python3
"""Compare player creation with the exact supported DLL bytes."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,pefile,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EBX
from check_projectiles_original import Rig,STACK
SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2';MASK=0xffffffff
ROOT=[0x100014f0,0x10001530,0x10001630,0x10001660,0x10001700,0x10001780,0x10004190,0x10004250,0x10004280,0x10004340]
SIZE=dict(zip(ROOT,[8,0x1c,0x10,0x1c,0x10,0x10,0x14,8,0x10,0x10]));ARGC=dict(zip(ROOT,[1,5,3,1,3,3,4,1,2,3]))
RANGES=list(zip(ROOT,[0x10001528,0x10001630,0x10001656,0x100016f8,0x1000177a,0x100017ec,0x100041ca,0x10004277,0x100042d5,0x1000441f]))
for site,size,argc,end in[(0x10003fe0,12,2,0x10004018),(0x100041d0,12,2,0x1000421c),(0x10005240,20,4,0x10005273),(0x10004220,20,4,0x1000424a),(0x10003b20,28,6,0x10003bfe),(0x10005010,8,1,0x1000501b)]:SIZE[site]=size;ARGC[site]=argc;RANGES.append((site,end))
IMPORTS={0x1100100:1,0x1100200:4,0x1100300:2,0x1100400:3,0x1100500:1,0x1100600:1,0x1100700:1,0x1100800:1,0x1100900:1,0x1100a00:6,0x1000594c:1,0x10005020:3,0x10002b80:2}
OFF={0x10001530:-4,0x10001660:-0x14,0x10004280:-4}
SPANS={0x10001530:[(0,4),(8,0x1c)],0x10001660:[(0,0x14),(0x18,0x1c)],0x10004280:[(0,4),(8,0x10)]}
def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA;image=pefile.PE(data=raw).get_memory_mapped_image();r=Rig(a.game);u=r.u;u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,image);md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls=set();returns={}
 for lo,hi in RANGES:
  for i in md.disasm(image[lo-0x10000000:hi-0x10000000],lo):
   if i.mnemonic in['push','pop','call','ret']:controls.add(i.address)
   if i.mnemonic=='ret':returns[i.address]=lo
 for at,raw_prefix in[(0x155f,'f3ab'),(0x1568,'f3a5'),(0x1597,'f3a5'),(0x159e,'f3a4'),(0x172c,'f3a5'),(0x16b7,'f2ae'),(0x16dd,'f3a5'),(0x16e4,'f3a4')]:assert image[at:at+2]==bytes.fromhex(raw_prefix)
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
  if row[15]&1 and s==0x100014f0 and frame(0x10001530):put(frame(0x10001530)+0xc,0x1201000)
  if row[15]&128 and s==0x10004190 and frame(0x10004340):put(frame(0x10004340)+0xc,0x10002)
  if row[15]&256 and s==0x10001530 and frame(0x10004340):put(frame(0x10004340)+4,0x1221000)
  if s==0x10002b80:stop();return False
  return True
 def foreign(s):
  nonlocal allocs,sleeps
  sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(IMPORTS[s])]
  if not ce(s,args):return
  result=0
  if s==0x1100100:
   result=0x12a0000+allocs*0x10000;allocs+=1
   if row[15]&2 and frame(0x10001530):put(0x1240084,3);put(input_at()+0x80,0x1205000)
   if row[9]or args[0]>0x10000:result=0
  elif s==0x1100200:
   text=f'{args[2]:08x}:{args[3]:04x}';string(args[0],text);result=len(text)
  elif s==0x1100300:result=9 if row[8]else 0
  elif s==0x1100400:
   if row[8]!=5:string(args[0],''if row[8]==2 else 'abc\n'if row[8]==3 else 'abcdefghijklmno');result=args[0]
  elif s==0x1100600:
   sleeps+=1
   if sleeps>=row[10]:put(0x10019b50,1);put(0x10019b54,row[11])
   if sleeps>10:stop();return
  elif s==0x1100700:put(args[0],1700000000);result=1700000000
  elif s==0x1100800:result=0x12f0000
  elif s==0x1100900:result=0x12e1000
  elif s in[0x1000594c,0x1100600]:result=0x12e0000
  r.finish(result)
  if s in[0x1000594c,0x1100600]:u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+4)
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
   if row[15]&4 and at==0x1240080:put(input_at()+0x80,0x1205000)
   if row[15]&8 and at==0x1203000 and frame(0x10001530):put(frame(0x10001530)+0x10,1,1)
  else:
   events.append(['R',norm(at),n]);event(1);event(norm(at));event(n);reads+=1
   if reads==row[12]or not mapped:stop();return
   if row[15]&16 and at==0x1240088:put(0x12400a4,0x1260100)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,n,v,user):
  if not failed:memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,n,v,user);stop()
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def base(op):return[op,3,0,1,1,7,1,1,0,0,2,0,0,0,0,0,0x5a,0,0,1,1,1,1,0x1234]
 rows=[]
 for op in range(10):
  for field,values in[(1,[0,1,3]),(2,[0,1,2,3]),(3,[0,1,2]),(4,[0,1,2,0xffffffff]),(5,[0,1,3,4,7,16]),(7,[0,1]),(8,[0,1,2,3,4,5]),(9,[0,1]),(10,[1,2,3]),(11,[0,1,2,0xffffffff]),(15,[0,1,2,4,8,16,128,256]),(16,[0x5a,10]),(17,[0,1]),(18,[0,1,2]),(19,[0,1,2]),(20,[0,1]),(21,[0,1]),(22,[0,1]),(23,[0x1234,0xa5a51234])]:
   for v in values:x=base(op);x[field]=v;rows.append(x)
  for field in[12,13]:
   for fail in[1,2,3,4,8,16,32,48,64,96,128,160]:x=base(op);x[field]=fail;rows.append(x)
  for fail in range(1,8):x=base(op);x[14]=fail;rows.append(x)
 rng=random.Random(0x10001530)
 for _ in range(a.cases):
  x=base(rng.randrange(10));x[1]=rng.randrange(4);x[2]=rng.randrange(4);x[5]=rng.randrange(16);x[8]=rng.randrange(6);x[15]=rng.choice([0,1,2,4,8,16,128,256]);rows.append(x)
 if a.case_file:rows=json.loads(a.case_file.read_text())
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 if a.replay_oracle:
  saved=json.loads(a.replay_oracle.read_text());assert saved['source_sha256']==SHA and saved['checker_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest();rows=saved['inputs'];expected=saved['outputs'];all_events=[[]for _ in rows]
 for index,row in enumerate([]if a.replay_oracle else rows):
  serial=1;reads=writes=calls=allocs=sleeps=0;trace=1469598103934665603;failed=False;restore=None;events=[];u.mem_write(0x1200000,b'\x5a'*0x100000);u.mem_write(0x10019880,bytes(0x426));u.mem_write(STACK-0x2000,b'\x5a'*0x4000)
  for at,v in[(0x10019880,row[20]),(0x10019884,row[21]),(0x100198ac,row[22]),(0x100198b8,row[1]),(0x100199b0,row[3]),(0x10019afc,0x1240000),(0x10019af8,0x1260000),(0x10019c5c,0x1228000),(0x10019c68,row[17]),(0x10019c6c,7),(0x10019b54,row[11]),(0x10019bf4,0x10005),(0x10019b0c,0x1280000),(0x1000605c,0x1100100),(0x1000606c,0x1100200),(0x10006060,0x1100300),(0x10006020,0x1100400),(0x10006018,0x1100500),(0x10006010,0x1100600),(0x10006078,0x1100700),(0x1000607c,0x1100800),(0x1000603c,0x1100900),(0x10006070,0x1100a00)]:put(at,v)
  u.mem_write(0x1240000,bytes(0x10000));u.mem_write(0x1260000,bytes(0x1000));u.mem_write(0x1280000,bytes(0x10000));put(0x10019b10,0x1280000);put(0x10019b14,0x1280000)
  for i in range(row[1]):
   at=0x1240000+i*0xbc;put(at+0x9c,int(row[2]==2 or row[2]in[1,3]and i==0));put(at+0x98,int(row[2]==3));put(at+0xa0,0x10000+i);put(at+0xa4,0x1260000 if row[2]==3 else 0)
  for at in[0x1200000,0x1201000,0x1220008]:
   u.mem_write(at,bytes((i*7+3)&255 for i in range(0x98)));string(at,'Player');put(at+0x80,0x1204000);put(at+0x84,row[5]);string(at+0x88,'')
  u.mem_write(0x1204000,bytes(i&255 for i in range(0x1000)));u.mem_write(0x1205000,bytes(i*3&255 for i in range(0x1000)))
  put(0x1260000,int(bool(row[19])));put(0x1260004,0x0100007f);put(0x126000c,int(row[19]==2));put(0x1260014,row[23]);put(0x100198b8,row[1]);put(0x1220000,0x1030,2);put(0x1220002,0xa0,2)
  op=row[0];s=ROOT[op];frames=[(s,STACK+OFF.get(s,0),SIZE[s],0)]
  args=[0x1203000]if op==0 else[0x1203000,input_at(),row[4],row[6],0x1260000 if row[7]else 0]if op==1 else[0x1203000,input_at(),row[4]]if op in[2,4,5]else[input_at()]if op==3 else[0x0100007f,row[23],row[11],0x10001]if op==6 else[0x10001]if op==7 else[0x1240000,0x0100007f]if op==8 else[0x1220000,0x0100007f,row[23]]
  u.reg_write(UC_X86_REG_EBX,(row[16]<<24)|0x005a5a5a);active=True;result=0
  try:r.call(s,*args,count=500000);result=u.reg_read(UC_X86_REG_EAX)
  except(unicorn.UcError,RuntimeError):
   if not failed:raise RuntimeError((index,row,hex(u.reg_read(UC_X86_REG_EIP)),events[-15:]))
  finally:active=False
  if failed or op==3 or op>=6:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in[(0x10019880,0x426),(0x1200000,0x6000),(0x1220000,0x10000),(0x1240000,0x10000),(0x1260000,0x1000),(0x1280000,0x1000),(0x12a0000,0x20000)]:
   for v in u.mem_read(at,n):state=((state^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events if a.limit==1 else events[:300])
 if not a.replay_oracle:a.report.with_suffix('.oracle.json').write_text(json.dumps({'source_sha256':SHA,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'inputs':rows,'outputs':expected})+'\n')
 p=subprocess.run([str(a.probe),str(a.game/'Netsocknl.dll')],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,(p.returncode,p.stderr);actual=p.stdout.splitlines();assert len(actual)==len(rows);bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i]}for i,(x,y)in enumerate(zip(actual,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'cases':len(rows),'differences':len(bad),'mismatches':bad[:8],'mismatch_summary':[{k:v for k,v in m.items()if k!='original_events'}for m in bad],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--cases',type=int,default=128);p.add_argument('--limit',type=int,default=0);p.add_argument('--case-file',type=Path);p.add_argument('--replay-oracle',type=Path);run(p.parse_args())
