#!/usr/bin/env python3
"""Compare the client join/discovery bodies with exact DLL instructions."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,pefile,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,STACK
SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2';MASK=0xffffffff
ROOT=[0x10001420,0x100014c0,0x100038f0,0x100057f0,0x10003a10,0x10003960,0x10003770,0x10003840,0x100037f0,0x10003630,0x100035a0,0x10003540,0x10003570];SIZE=dict(zip(ROOT,[8,4,8,4,4,12,8,12,12,12,16,12,4]));ARGC=dict(zip(ROOT,[1,0,1,0,0,0,1,2,2,2,3,2,0]));LOCAL={s:12 if s==0x10003960 else 0 for s in ROOT};FRAMELESS={s for s in ROOT if SIZE[s]==4};SPANS={s:[(0,12)]if LOCAL[s]else[(4,SIZE[s])]for s in ROOT};RANGES=list(zip(ROOT,[0x100014bb,0x100014ee,0x1000395d,0x1000582f,0x10003a55,0x10003a04,0x100037ee,0x100038e4,0x1000383a,0x10003675,0x100035f0,0x1000356a,0x1000359c]))
for site,n,loc,argc,end,spans in[(0x10003470,4,0,0,0x1000349e,[]),(0x10003a60,8,0,1,0x10003aa0,[(4,8)]),(0x100021d0,0x104,0x104,0,0x1000223c,[(4,0x104)]),(0x10005830,4,4,0,0x10005854,[(0,4)]),(0x10005020,0x30,0x20,3,0x1000511d,[(0x10,0x20),(0x24,0x30)])]:
 SIZE[site]=n;LOCAL[site]=loc;ARGC[site]=argc;SPANS[site]=spans;RANGES.append((site,end))
FRAMELESS.add(0x10003470)
IMPORTS={0x1100100:3,0x1100200:1,0x1100300:0,0x1100400:1,0x1100500:1,0x1100600:1,0x1100700:2,0x1000596a:1,0x10005916:0,0x10005934:3,0x1000592e:1,0x10005952:6,0x10004c80:2}
def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA;image=pefile.PE(data=raw).get_memory_mapped_image();r=Rig(a.game);u=r.u;u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,image);md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls=set();returns={}
 for lo,hi in RANGES:
  for i in md.disasm(image[lo-0x10000000:hi-0x10000000],lo):
   if i.mnemonic in['push','pop','call','ret']:controls.add(i.address)
   if i.mnemonic=='ret':returns[i.address]=lo
 active=failed=False;row=None;frames=[];heaps=[];events=[];serial=reads=writes=calls=allocs=next_at=find_reads=0;trace=0;restore=None
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,v,n=4):u.mem_write(at,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
 def config():return 0 if row[3]==0 else 0x100198ac if row[3]==2 else 0x1200000
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
  if calls==row[21]:stop();return False
  if row[17]&512 and s==0x10003540 and frame(0x100035a0):
   f=frame(0x100035a0);put(f+4,0x1201000);put(f+8,0x0300007f);put(f+12,0xabcd5678)
  own=s in ROOT;full=row[17]&8 and s in[0x10003470,0x10003a60,0x100021d0,0x1100400,0x1100500,0x1100600,0x1100700];loop=row[17]&16 and s in[0x10005830,0x10005020,0x10005934,0x1000592e,0x10005952]
  if not own and not full and not loop and s not in[0x1100100,0x1100200,0x1100300,0x1000596a,0x10005916]:stop();return False
  return True
 def foreign(s):
  nonlocal next_at
  sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(IMPORTS[s])]
  if not ce(s,args):return
  result=0
  if s in[0x1100100,0x1000596a]:result=row[18]
  elif s==0x1100200:put(0x100199e4,0);put(0x100199e8,1)
  elif s==0x10005916:result=10038
  elif s==0x1100400:result=next_at;next_at+=0x10000;heaps.append((result,0x10000))
  elif s==0x1100500:result=12345
  elif s==0x10005934:put(args[2],0)
  elif s==0x1000592e:result=((args[0]&255)<<8)|((args[0]>>8)&255)
  elif s==0x10005952:result=args[2]
  elif s not in[0x1100300,0x1100600,0x1100700]:stop();return
  r.finish(result)
  if s in[0x1100200,0x1000596a,0x10005934,0x1000592e,0x10005952]:u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+IMPORTS[s]*4)
 for s in IMPORTS:r.stubs[s]=lambda s=s:foreign(s)
 def code(uc,pc,n,_):
  nonlocal serial
  if not active or failed:return
  if pc in SIZE and not(pc==ROOT[row[0]]and u.reg_read(UC_X86_REG_ESP)==STACK):
   sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(ARGC[pc])]
   if not ce(pc,args):return
   if pc not in FRAMELESS:
    at=sp-LOCAL[pc]
    if LOCAL[pc]:u.mem_write(at,b'\x5a'*LOCAL[pc])
    frames.append((pc,at,SIZE[pc],serial));serial+=1
  if pc in returns and len(frames)>1:
   if frames[-1][0]==returns[pc]or returns[pc]==0x10003470 and frames[-1][0]==0x100037f0:frames.pop()
 u.hook_add(unicorn.UC_HOOK_CODE,code)
 def memory(uc,kind,at,n,v,_):
  nonlocal reads,writes,restore,find_reads
  if not active or failed:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not any(lo<=pc<hi for lo,hi in RANGES):return
  if 0x2000000<=at<0x2010000:
   if pc in controls:return
   if not any(any(base+lo<=at and at+n<=base+hi for lo,hi in SPANS[s])for s,base,size,idx in frames):return
  mapped=any(base<=at and at+n<=base+size for base,size in[(0x10000000,0x1b000),(0x1200000,0x100000)]+[(base,size)for s,base,size,idx in frames])
  if kind==unicorn.UC_MEM_WRITE:
   v&=(1<<(8*n))-1;events.append(['W',norm(at),n,norm(v)]);event(2);event(norm(at));event(n);event(norm(v));writes+=1
   if writes==row[20]or not mapped:
    try:restore=(at,get(at,n),n)
    except unicorn.UcError:pass
    stop();return
   if row[17]&128 and at==0x100198ac:put(frames[0][1]+4,0x1201000)
   if row[17]&2048 and at==0x100199b0 and frame(0x100037f0):put(frame(0x100037f0)+4,0x0300007f)
   if row[17]&32 and at==0x100199b0:put(0x10019c9e,row[9]^1)
  else:
   events.append(['R',norm(at),n]);event(1);event(norm(at));event(n);reads+=1
   if reads==row[19]or not mapped:stop();return
   if row[17]&64 and frame(0x100038f0)and at==frame(0x100038f0)+4:
    find_reads+=1
    if find_reads==2:put(at,0x1201000)
   if row[17]&256 and frame(0x10003840)and at==frame(0x10003840)+8:put(frame(0x10003840)+4,0x0300007f)
   if row[17]&1024 and at==0x100199cc:put(0x12200e4,0xdeadbeef)
   if row[17]&1 and at==0x1240106:put(0x100199dc,0x1244000)
   if row[17]&2 and at==0x12200e4:put(0x100199cc,0xdeadbeef)
   if row[17]&4 and at==0x10019a80:put(0x10019c5c,0x1224000)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,n,v,user):
  if not failed:memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,n,v,user);stop()
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def base(op):return[op,1,1,1,0x100,0,9,1,0,0,0,1,1,0x1021,12,1,1,0,0,0,0,0,0,0]
 rows=[]
 for op in range(13):
  for field,values in[(1,[0,1]),(2,[0,1]),(3,[0,1,2]),(4,[0,0x100,0x101]),(5,[0,1]),(6,[0,5,9,0xf,0x300,MASK]),(7,[0,1]),(8,[0,1]),(9,[0,1]),(10,[0,0x1234]),(11,[0,1,2,3]),(12,[0,1]),(13,[0,0x1011,0x1021]),(14,[0,12,0x104]),(15,[0,1,2,3]),(16,[0,1,2,3,4,0xffff]),(17,[0,1,2,4,32,39]),(18,[0,1,MASK]),(19,list(range(1,16))+[20,40,80]),(20,list(range(1,12))+[20,40,80]),(21,list(range(1,6)))]:
   for v in values:x=base(op);x[field]=v;rows.append(x)
 for op in[7,8]:
  for dyn in[0,1]:x=base(op);x[9]=dyn;x[17]=8;rows.append(x)
 for cancel in[0,1]:x=base(5);x[7]=0;x[8]=cancel;x[17]=16;rows.append(x)
 for op in[0,2]:
  x=base(op);x[11]=3;x[12]=0;x[17]=64;rows.append(x)
 x=base(0);x[17]=128;rows.append(x)
 x=base(7);x[9]=1;x[17]=8|256;rows.append(x)
 x=base(8);x[9]=1;x[17]=8|2048;rows.append(x)
 x=base(10);x[17]=512;rows.append(x)
 for mode in[0,2,1024]:
  x=base(9);x[13]=0x1011;x[14]=0x104;x[17]=mode;rows.append(x)
 for field,values in[(19,[100,160,200,300]),(20,[100,130,135,200,500])]:
  for value in values:x=base(0);x[field]=value;rows.append(x)
 for field,values in[(20,[1,2,3,5,16390,32780]),(21,range(1,13))]:
  for value in values:x=base(7);x[17]=8;x[field]=value;rows.append(x)
 rng=random.Random(0x10001420)
 for _ in range(a.cases):
  x=base(rng.randrange(13));x[5]=rng.randrange(2);x[6]=rng.choice([0,9,15]);x[9]=rng.randrange(2);x[11]=rng.randrange(4);x[12]=rng.randrange(2);x[15]=rng.randrange(4);x[16]=rng.randrange(5);x[17]=rng.choice([0,1,2,4,32,39]);rows.append(x)
 if a.case_file:rows=json.loads(a.case_file.read_text())
 if a.op is not None:rows=[x for x in rows if x[0]==a.op]
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 if a.replay_oracle:
  saved=json.loads(a.replay_oracle.read_text());assert saved['source_sha256']==SHA and saved['checker_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest();rows=saved['inputs'];expected=saved['outputs'];all_events=[[]for _ in rows]
 for index,row in enumerate([]if a.replay_oracle else rows):
  if index%32==0:a.report.with_suffix('.progress.json').write_text(json.dumps({'case':index,'total':len(rows)})+'\n')
  serial=1;reads=writes=calls=allocs=find_reads=0;next_at=0x1280000;trace=1469598103934665603;failed=False;restore=None;events=[];heaps=[];u.mem_write(0x1200000,b'\x5a'*0x100000);u.mem_write(0x10019880,bytes(0x426));u.mem_write(STACK-0x2000,b'\x5a'*0x4000)
  for at,v in[(0x10019880,row[1]),(0x10019884,row[2]),(0x100199e4,row[5]),(0x100199ec,row[6]),(0x100199e8,row[7]),(0x10019af0,row[8]),(0x10019c9e,row[9]),(0x100199dc,0x1240000),(0x10019c5c,0x1220000),(0x10019c64,0x10000),(0x10019854,0x1260000),(0x1001985c,0x10000),(0x100096b4,55),(0x100198a8,0x0200007f),(0x100199e0,0x0100007f),(0x1000604c,0x1100100),(0x10006010,0x1100200),(0x10006048,0x1100300),(0x1000605c,0x1100400),(0x10006078,0x1100500),(0x10006064,0x1100600),(0x10006060,0x1100700)]:put(at,v)
  put(0x10019858,row[10],2);put(0x10019c70,0xa5a51234);put(0x1200000,row[4]);put(0x1201000,row[4])
  if row[3]==2:put(config(),row[4])
  for j in range(4):v=0x11111111*(j+1);put(0x12000d0+j*4,v);put(0x12000e0+j*4,v);put(0x12010d0+j*4,v+1);put(0x100199cc+j*4,v)
  u.mem_write(0x1240000,bytes(0x21c0))
  for i in range(32):
   at=0x1240000+i*0x10e;put(at,0x0100007f+i);put(at+4,0x1234+i,2);put(at+0x106,int(row[11]==3 or row[11]==1 and i==0 or row[11]==2 and i==1))
   for j in range(4):put(at+0xd6+j*4,0x11111111*(j+1)+(0 if row[12]else 1))
  put(0x1220000,row[13],2);put(0x1220002,row[14],2)
  if row[13]==0x1011:u.mem_write(0x1220004,bytes(u.mem_read(0x1200000,0x100)))
  else:put(0x1220004,row[15],2);put(0x1220006,row[16],2);put(0x1220008,0x10000)
  put(0x10019a80,0,1);op=row[0];s=ROOT[op];at=STACK-LOCAL[s];frames=[(s,at,SIZE[s],0)];args=[]
  if op in[0,2]:args=[config()]
  if op==6:args=[0x0100007f]
  if op in[7,8,9,11]:args=[0x0100007f,0x5a5a1234]
  if op==10:args=[config(),0x0100007f,0x5a5a1234]
  active=True;result=0
  try:r.call(s,*args,count=4000000);result=u.reg_read(UC_X86_REG_EAX)
  except(unicorn.UcError,RuntimeError):
   if not failed:raise
  finally:active=False
  if failed or op in[7,9]:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for base_at,n in[(0x10019880,0x426),(0x1200000,0x2000),(0x1220000,0x8000),(0x1240000,0x8000),(0x1260000,0x10000)]+heaps:
   for v in u.mem_read(base_at,n):state=((state^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events if a.limit==1 else events[:300])
 if not a.replay_oracle:a.report.with_suffix('.oracle.json').write_text(json.dumps({'source_sha256':SHA,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'inputs':rows,'outputs':expected})+'\n')
 p=subprocess.run([str(a.probe),str(a.game/'Netsocknl.dll')],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,(p.returncode,p.stderr);actual=p.stdout.splitlines();assert len(actual)==len(rows);bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i]}for i,(x,y)in enumerate(zip(actual,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'cases':len(rows),'differences':len(bad),'mismatches':bad[:8],'mismatch_summary':[{k:v for k,v in m.items()if k!='original_events'}for m in bad],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--cases',type=int,default=128);p.add_argument('--limit',type=int,default=0);p.add_argument('--op',type=int);p.add_argument('--case-file',type=Path);p.add_argument('--replay-oracle',type=Path);run(p.parse_args())
