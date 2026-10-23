#!/usr/bin/env python3
"""Check close and enum calls against the supplied DLL instructions."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,pefile,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,STACK
SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2';MASK=0xffffffff
ROOT=[0x10001250,0x100012a0,0x100057b0,0x10005780,0x10001990,0x100018d0,0x10003730,0x100034a0,0x100035f0,0x10003680]
SIZE=dict(zip(ROOT,[4,4,4,4,4,0x18,4,8,4,12]));ARGC=dict(zip(ROOT,[0,0,0,0,0,5,0,1,0,0]));LOCAL={s:12 if s==0x10003680 else 0 for s in ROOT};FRAMELESS={s for s in ROOT if SIZE[s]==4};SPANS={s:[(0,12)]if LOCAL[s]else[(4,SIZE[s])]for s in ROOT};RANGES=list(zip(ROOT,[0x1000129c,0x100012db,0x100057ea,0x100057a8,0x100019df,0x10001983,0x10003768,0x1000353b,0x10003625,0x10003730]))
for site,n,loc,argc,end,spans in[(0x100014c0,4,0,0,0x100014ee,[]),(0x10005830,4,4,0,0x10005854,[(0,4)]),(0x10005020,0x30,0x20,3,0x1000511d,[(0x10,0x20),(0x24,0x30)])]:
 SIZE[site]=n;LOCAL[site]=loc;ARGC[site]=argc;SPANS[site]=spans;RANGES.append((site,end))
FRAMELESS.add(0x100014c0)
IMPORTS={0x1100100:3,0x1100200:1,0x1100300:0,0x1100400:1,0x1100500:1,0x1100600:2,0x1100700:1,0x1100800:2,0x1100900:1,0x1000596a:1,0x10005964:0,0x10005916:0,0x10005934:3,0x1000592e:1,0x10005952:6,0x10001880:0,0x100052e0:5}
def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA;image=pefile.PE(data=raw).get_memory_mapped_image();r=Rig(a.game);u=r.u;u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,image);md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls=set();returns={}
 for lo,hi in RANGES:
  for i in md.disasm(image[lo-0x10000000:hi-0x10000000],lo):
   if i.mnemonic in['push','pop','call','ret']:controls.add(i.address)
   if i.mnemonic=='ret':returns[i.address]=lo
 active=failed=False;row=None;frames=[];events=[];serial=reads=writes=calls=sleeps=frees=callbacks=polls=0;trace=0;restore=None
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,v,n=4):u.mem_write(at,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
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
  if calls==row[19]:stop();return False
  if row[15]&128 and s==0x100014c0 and frame(0x100018d0):
   f=frame(0x100018d0);put(f+4,0x1201000);put(f+12,0x1100800);put(f+16,0xdead1234);put(f+20,0xdead5678);put(0x12000e0,0xdeadbeef)
  if row[15]&16 and s==0x10005020:
   put(0x10019c5c,0x1224000);put(0x1220002,0x18,2);put(0x10019868,0x4567,2)
   if frame(0x100034a0):put(frame(0x100034a0)+4,0x0300007f)
  own=s in ROOT;leaf=row[16]&2 and s in[0x10005020,0x10005830,0x10005934,0x1000592e,0x10005952]
  if not own and not leaf and s not in[0x100014c0,0x1100100,0x1100200,0x1100300,0x1100400,0x1100500,0x1100600,0x1100700,0x1100800,0x1100900,0x1000596a,0x10005964,0x10005916]:stop();return False
  return True
 def foreign(s):
  nonlocal sleeps,frees,callbacks,polls
  sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(IMPORTS[s])]
  if not ce(s,args):return
  result=0
  if s==0x1000596a:result=row[10]
  elif s==0x10005964:result=row[11]
  elif s==0x10005916:result=10038
  elif s==0x1100100:result=123
  elif s==0x1100200:
   sleeps+=1
   if row[15]&64:put(0x10006010,0x1100900)
   if sleeps>=row[13]:put(0x100199b8 if args[0]==10 else 0x100199bc,0 if args[0]==10 else 1)
  elif s in[0x1100400,0x1100700]:
   frees+=1
   if frees==1 and row[15]&1:put(0x10006068,0x1100700);put(0x10019c60,0x122c000);put(0x100199dc,0x1244000)
   result=row[12]
  elif s in[0x1100500,0x1100900]:result=row[12]
  elif s in[0x1100600,0x1100800]:
   callbacks+=1
   if callbacks==1 and row[15]&8:put(0x100199dc,0x1244000);put(0x100199c4,100);put(0x100199c0,0x1100800);put(0x1240000+31*0x10e+0x106,1)
   result=1234
  elif s==0x10005934:polls+=1;put(args[2],row[20]if polls==1 else 0)
  elif s==0x1000592e:result=((args[0]&255)<<8)|((args[0]>>8)&255)
  elif s==0x10005952:result=args[2]
  elif s!=0x1100300:stop();return
  r.finish(result)
  if s in[0x1100200,0x1100500,0x1100900,0x1000596a,0x10005964,0x10005934,0x1000592e,0x10005952]:u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+IMPORTS[s]*4)
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
  if pc in returns and len(frames)>1 and frames[-1][0]==returns[pc]:frames.pop()
 u.hook_add(unicorn.UC_HOOK_CODE,code)
 def memory(uc,kind,at,n,v,_):
  nonlocal reads,writes,restore
  if not active or failed:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not any(lo<=pc<hi for lo,hi in RANGES):return
  if 0x2000000<=at<0x2010000:
   if pc in controls:return
   if not any(any(base+lo<=at and at+n<=base+hi for lo,hi in SPANS[s])for s,base,size,idx in frames):return
  mapped=any(base<=at and at+n<=base+size for base,size in[(0x10000000,0x1b000),(0x1200000,0x100000)]+[(base,size)for s,base,size,idx in frames])
  if kind==unicorn.UC_MEM_WRITE:
   v&=(1<<(8*n))-1;events.append(['W',norm(at),n,norm(v)]);event(2);event(norm(at));event(n);event(norm(v));writes+=1
   if writes==row[18]or not mapped:
    try:restore=(at,get(at,n),n)
    except unicorn.UcError:pass
    stop();return
   if row[15]&2 and at==0x10019860:put(0x100096b0,67);put(0x10006000,0x1100900)
   if row[15]&256 and at==0x100199bc:put(0x100199dc,0x1244000)
  else:
   events.append(['R',norm(at),n]);event(1);event(norm(at));event(n);reads+=1
   if reads==row[17]or not mapped:stop();return
   if row[15]&4 and at==0x100199c4:put(0x100199c0,0x1100800)
   if row[15]&32 and at==0x100199cc:put(0x100199d0,0xabcdef01)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,n,v,user):
  if not failed:memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,n,v,user);stop()
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def base(op):return[op,1,1,1,1,0,0,0,0,1,0,0,7,1,2,0,2,0,0,0,0,0,0,0]
 rows=[]
 for op in range(10):
  for field,values in[(1,[0,1]),(2,[0,1]),(3,[0,1]),(4,[0,1]),(5,[0,1]),(6,[0,1]),(7,[0,1]),(8,[0,1]),(9,[0,1]),(10,[0,1,MASK]),(11,[0,1,MASK]),(12,[0,7,MASK]),(13,[1,2]),(14,[0,1,2,32]),(15,[0,1,2,4,8,16,32,64,128,256]),(16,[0,2]),(17,list(range(1,20))+[32,64,128]),(18,list(range(1,12))+[32,64]),(19,list(range(1,12)))]:
   for v in values:x=base(op);x[field]=v;rows.append(x)
 for op in[4,9]:
  for mode in[0,64,8,72]:x=base(op);x[6]=1;x[13]=52 if op==9 else 3;x[15]=mode;rows.append(x)
 for field,values in[(18,[100,1000,2159,2160,2161]),(17,[30,100]),(19,[1,2])]:
  for v in values:x=base(6);x[field]=v;x[15]=256;rows.append(x)
 for mode in[0,32,64]:
  x=base(9);x[20]=1;x[15]=mode;rows.append(x)
 rng=random.Random(0x10001250)
 for _ in range(a.cases):
  x=base(rng.randrange(10));x[6]=rng.randrange(2);x[7]=rng.randrange(2);x[8]=rng.randrange(2);x[14]=rng.randrange(33);x[15]=rng.choice([0,1,2,4,8,16,32,64,128,256]);rows.append(x)
 if a.case_file:rows=json.loads(a.case_file.read_text())
 if a.op is not None:rows=[x for x in rows if x[0]==a.op]
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 if a.replay_oracle:
  saved=json.loads(a.replay_oracle.read_text());assert saved['source_sha256']==SHA and saved['checker_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest();rows=saved['inputs'];expected=saved['outputs'];all_events=[[]for _ in rows]
 for index,row in enumerate([]if a.replay_oracle else rows):
  if index%32==0:a.report.with_suffix('.progress.json').write_text(json.dumps({'case':index,'total':len(rows)})+'\n')
  serial=1;reads=writes=calls=sleeps=frees=callbacks=polls=0;trace=1469598103934665603;failed=False;restore=None;events=[];u.mem_write(0x1200000,b'\x5a'*0x100000);u.mem_write(0x10019880,bytes(0x426));u.mem_write(STACK-0x2000,b'\x5a'*0x4000)
  for at,v in[(0x10019880,row[1]),(0x10019884,row[2]),(0x100198ac,row[5]),(0x100199b8,row[6]),(0x100199bc,row[7]),(0x10019c9e,row[8]),(0x10019860,row[9]),(0x100199dc,0x1240000),(0x10019c5c,0x1220000),(0x10019c60,0x1228000),(0x10019c64,0x10000),(0x10019854,0x1260000),(0x1001985c,0x10000),(0x100096b4,55),(0x100096b0,66),(0x100096a8,77),(0x100096ac,88),(0x100198a8,0x0200007f),(0x1000604c,0x1100100),(0x10006010,0x1100200),(0x10006048,0x1100300),(0x10006068,0x1100400),(0x10006000,0x1100500),(0x100199c0,0x1100600),(0x100199c4,99)]:put(at,v)
  put(0x10019868,0xf123,2)
  for j in range(4):put(0x12000e0+j*4,0x11111111*(j+1));put(0x100199cc+j*4,0x11111111*(j+1))
  u.mem_write(0x1240000,bytes(0x8000))
  for i in range(32):put(0x1240000+i*0x10e+0x106,int(i<row[14]))
  op=row[0];s=ROOT[op];at=STACK-LOCAL[s];frames=[(s,at,SIZE[s],0)];args=[]
  if op==5:args=[0x1200000 if row[3]else 0,0xaabbccdd,0x1100600 if row[4]else 0,99,0x77777777]
  if op==7:args=[0x0200007f]
  active=True;result=0
  try:r.call(s,*args,count=4000000);result=u.reg_read(UC_X86_REG_EAX)
  except(unicorn.UcError,RuntimeError):
   if not failed:raise
  finally:active=False
  if failed:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for base_at,n in[(0x10019880,0x426),(0x100096a8,16),(0x10006000,0x80),(0x1200000,0x2000),(0x1220000,0x10000),(0x1240000,0x8000),(0x1260000,0x10000)]:
   for v in u.mem_read(base_at,n):state=((state^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events if a.limit==1 else events[:300])
 if not a.replay_oracle:a.report.with_suffix('.oracle.json').write_text(json.dumps({'source_sha256':SHA,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'inputs':rows,'outputs':expected})+'\n')
 p=subprocess.run([str(a.probe),str(a.game/'Netsocknl.dll')],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,(p.returncode,p.stderr);actual=p.stdout.splitlines();assert len(actual)==len(rows);bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i]}for i,(x,y)in enumerate(zip(actual,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'cases':len(rows),'differences':len(bad),'mismatches':bad[:8],'mismatch_summary':[{k:v for k,v in m.items()if k!='original_events'}for m in bad],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--cases',type=int,default=128);p.add_argument('--limit',type=int,default=0);p.add_argument('--op',type=int);p.add_argument('--case-file',type=Path);p.add_argument('--replay-oracle',type=Path);run(p.parse_args())
