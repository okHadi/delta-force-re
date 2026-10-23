#!/usr/bin/env python3
"""Check current periodic, ping, removal, and close instruction bodies."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,pefile,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,STACK
SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2';MASK=0xffffffff
ROOT=[0x10004830,0x10002d30,0x10002b80,0x10004820,0x10004600,0x10004490,0x100042e0,0x10005010,0x100020d0,0x10001830,0x10002d90,0x10001880,0x100017f0,0x10003e20,0x10003e90]
SIZE=dict(zip(ROOT,[4,8,0x68,8,8,8,0x14,8,0x10,4,4,4,4,4,4]));LOCAL=dict(zip(ROOT,[0,0,0x5c,0,0,0,8,0,8,0,0,0,0,0,0]));ARGC=dict(zip(ROOT,[0,1,2,1,1,1,2,1,1,0,0,0,0,0,0]));FRAMELESS={ROOT[i]for i in[0,9,10,11,12,13,14]};VOID=set(ROOT)-{ROOT[7],ROOT[11]};SPANS={s:[(4,SIZE[s])]for s in ROOT};SPANS.update({ROOT[2]:[(0x10,0x5a),(0x60,0x68)],ROOT[6]:[(4,8),(12,0x14)],ROOT[8]:[(0,8),(12,16)]})
for s in FRAMELESS:SPANS[s]=[]
for site,n,local,argc,spans in[(0x100025a0,4,0,0,[]),(0x10002620,0x48,0x3c,2,[(8,0x3c),(0x40,0x48)]),(0x10004220,0x14,0,4,[(4,0x14)]),(0x10003b20,0x1c,0,6,[(4,0x1c)]),(0x10005240,0x14,0,4,[(4,0x14)])]:SIZE[site]=n;LOCAL[site]=local;ARGC[site]=argc;SPANS[site]=spans
FRAMELESS.add(0x100025a0)
RANGES=list(zip(ROOT,[0x100048be,0x10002d90,0x10002d28,0x1000482c,0x10004652,0x1000450b,0x10004335,0x1000501b,0x10002176,0x10001878,0x10002dae,0x100018c6,0x1000182b,0x10003e8e,0x10003ec9]))+[(0x100025a0,0x10002612),(0x10002620,0x10002703),(0x10004220,0x10004244),(0x10003b20,0x10003c00),(0x10005240,0x10005273)]
def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA;r=Rig(a.game);u=r.u;image=pefile.PE(data=raw).get_memory_mapped_image();u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,image);md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls=set();returns={}
 for lo,hi in RANGES:
  for i in md.disasm(image[lo-0x10000000:hi-0x10000000],lo):
   if i.mnemonic in['push','pop','call','ret']:controls.add(i.address)
   if i.mnemonic=='ret':returns[i.address]=lo
 row=None;serial=reads=writes=calls=allocs=clocks=sleeps=frees=next_at=0;trace=0;active=failed=False;restore=None;frames=[];heaps=[];events=[]
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,v,n=4):u.mem_write(at,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
 def string(at,s):u.mem_write(at,s.encode()+b'\0')
 def frame(site):return next((at for s,at,n,idx in reversed(frames)if s==site),0)
 def norm(at):
  for s,base,n,idx in reversed(frames):
   if base<=at<base+n:return 0x30000000+idx*0x1000+at-base
  return at
 def event(n):
  nonlocal trace
  for i in range(4):trace=((trace^((n>>(i*8))&255))*1099511628211)&0xffffffffffffffff
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def call_event(site,args):
  nonlocal calls
  # Only close-notice10003E20's port argument has caller-saved unconsumed highbits.
  if site==0x10005240 and get(u.reg_read(UC_X86_REG_ESP))==0x10003e77:args[3]&=0xffff
  events.append(['C',site,[norm(x)for x in args]]);event(3);event(site);event(len(args))
  for x in args:event(norm(x))
  calls+=1
  if calls==row[24]:stop();return False
  return True
 def allocate(n):
  nonlocal allocs,next_at
  allocs+=1
  if allocs==row[25]:return 0
  if n>0x20000 or next_at+n>0x1270000:raise RuntimeError('Allocation exceeds owner')
  at=next_at;next_at+=(n+255)&~255;heaps.append((at,n));return at
 imports={0x1100100:1,0x1100200:2,0x1100300:1,0x1100a00:1,0x1100400:0,0x1100500:1,0x1100b00:1,0x1100600:1,0x1100700:1,0x1100800:1,0x1100900:6,0x1000594c:1,0x10005020:3}
 def foreign(site):
  nonlocal frees,clocks,sleeps
  sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(imports[site])]
  if not call_event(site,args):return
  m=row[21];result=0
  if site==0x1100100:
   if m&8:put(0x12800a4,0x1281018);put(0x1280080,0x1285100);put(0x10019c6c,9)
   result=allocate(args[0])
  elif site==0x1100200:
   if not row[26]:
    count=get(0x1000769c);result=allocate(args[1])
    if result and count:
     if count*0x4a>0x100000:raise RuntimeError('Reallocation copy exceeds owner')
     u.mem_write(result,bytes(u.mem_read(args[0],count*0x4a)))
  elif site in[0x1100300,0x1100a00]:
   put(0x12878c0+frees*4,args[0]);frees+=1
   if frees>16:raise RuntimeError('Free fixture limit')
   if m&2:put(0x10006068,0x1100a00);put(0x10019af8,0x1281800)
   if m&2048:put(0x10007698,0x1282400)
  elif site==0x1100400:
   clocks+=1
   if m&1:put(0x100198b8,1)
   if m&512 and frame(0x10002b80):put(frame(0x10002b80)+0x60,0x12800bc)
   result=(row[13]+clocks-1)&MASK
  elif site in[0x1100500,0x1100b00]:
   sleeps+=1
   if m&64:put(0x10006010,0x1100b00)
   if row[28]and sleeps>=row[28]:put(0x10019c14,1)
   if sleeps>8:raise RuntimeError('Required ack provider failed')
  elif site==0x1100600:put(args[0],0x12345678);result=0x12345678
  elif site==0x1100700:
   if m&16:put(0x12800a0,0x20002);put(0x10019c6c,9)
   result=0x1286100
  elif site==0x1100800:result=0x1286000
  elif site==0x1000594c:
   if m&32:put(0x10019c6c,11)
   result=0x1286200
  elif site==0x1100900:
   for i,v in enumerate(args):put(0x1287800+i*4,v)
   if m&16384:put(0x1280080,0x1285100)
  elif site==0x10005020:
   if args[1]>0x100:raise RuntimeError('Capture exceeds owner')
   u.mem_write(0x1287000,bytes(u.mem_read(args[0],args[1])));put(0x12877e0,args[2]);put(0x12877e4,get(0x10019868,2))
   if m&4:put(0x10019c5c,0x1284000);put(0x1283002,8,2)
  r.finish(result)
  if site in[0x1000594c,0x1100500,0x1100b00]:u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+4)
 def safe_foreign(site):
  try:foreign(site)
  except(unicorn.UcError,RuntimeError):stop()
 for site in imports:r.stubs[site]=lambda site=site:safe_foreign(site)
 def code(uc,pc,n,_):
  nonlocal serial
  if not active or failed:return
  if pc in SIZE and not(pc==ROOT[row[0]]and u.reg_read(UC_X86_REG_ESP)==STACK):
   sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(ARGC[pc])]
   if not call_event(pc,args):return
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
  mapped=any(base<=at and at+n<=base+size for base,size in[(0x10000000,0x1b000),(0x1200000,0x100000)]+[(base,size)for s,base,size,idx in frames]);m=row[21]
  if kind==unicorn.UC_MEM_WRITE:
   v&=(1<<(8*n))-1;events.append(['W',norm(at),n,norm(v)]);event(2);event(norm(at));event(n);event(norm(v));writes+=1
   if writes==row[23]or not mapped:
    try:restore=(at,get(at,n),n)
    except unicorn.UcError:pass
    stop();return
   if m&256 and at==0x10019b08:put(0x100198b8,1)
   if m&32768 and at==0x1281000:put(0x12800a4,0x1281018)
  else:
   events.append(['R',norm(at),n]);event(1);event(norm(at));event(n);reads+=1
   if reads==row[22]or not mapped:stop();return
   if m&128 and at==0x10019c90 and frame(0x100020d0):put(frame(0x100020d0)+12,row[13]+123)
   if m&1024 and at==0x1283002:put(0x10019c5c,0x1284000)
   if m&4096 and frame(0x10002b80)and at==frame(0x10002b80)+0x60 and get(at)<=1:put(at,0x1282010)
   if m&8192 and at==0x10019c9a and frame(0x100020d0):put(frame(0x100020d0)+12,row[13]+456)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,n,v,user):
  if not failed:memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,n,v,user);stop()
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def base(op=0):return[op,2,1,0x04030201,120,1,1,300,3,0,2,0,0,10000,0,0,0,1,1,1,1,0,0,0,0,0,0,0x5a,1,0,7,5]
 rows=[];rng=random.Random(0x10004830)
 for op in range(15):
  for m in[0]+[1<<i for i in range(16)]+[0xffff]:x=base(op);x[21]=m;x[9]=1 if op==2 else 0;x[11]=1 if op in[5,6]else 0;x[14]=1 if op==8 else 0;rows.append(x)
 for op in range(15):
  for field,values in[(1,[0,1,4,0x80000000]),(2,[0]),(3,[MASK]),(4,[0,119,120,MASK]),(5,[0]),(6,[0]),(7,[0,299,300,MASK]),(8,[0,1,4,0x80000000]),(9,[1,2]),(10,[0,1,3,MASK]),(11,[1]),(12,[1]),(13,[0,1500,1501,10000,10001,0x7fffffff,0x80000000,MASK]),(14,[1]),(15,[1,MASK]),(16,[1,MASK]),(17,[0]),(18,[0,2]),(19,[0]),(20,[0]),(25,[1,2]),(26,[1]),(28,[0,2,3]),(29,[0x800,0xff6,MASK]),(30,[MASK]),(31,[MASK])]:
   for v in values:
    x=base(op);x[field]=v
    if field==1 and v>0x10000:x[22]=80
    rows.append(x)
  for field,positions in[(22,[1,2,3,4,5,8,10,15,20,30,50,80,120]),(23,[1,2,3,4,5,8,10,15,20,30,50,100,200]),(24,list(range(1,17)))]:
   for n in positions:x=base(op);x[field]=n;rows.append(x)
 for started in[0,1,0xffff0000,MASK]:
  for elapsed in[0,1500,1501,10000,10001]:
   for host in[0,1]:
    x=base(8);x[14]=1;x[15]=started;x[13]=(started+elapsed)&MASK;x[16]=(x[13]-1501)&MASK;x[17]=host;rows.append(x)
 for op in[0,11,12,14]:
  for n in range(17,33):x=base(op);x[24]=n;rows.append(x)
 for _ in range(a.cases):
  x=base(rng.randrange(15));x[1]=rng.randrange(5);x[2]=rng.randrange(2);x[4]=rng.choice([0,119,120,MASK]);x[5]=rng.randrange(2);x[6]=rng.randrange(2);x[7]=rng.choice([0,299,300,MASK]);x[8]=rng.randrange(5);x[9]=rng.randrange(3);x[10]=rng.randrange(4);x[11]=rng.randrange(2);x[12]=rng.randrange(2);x[13]=rng.getrandbits(32);x[14]=rng.randrange(2);x[15]=rng.getrandbits(32);x[16]=rng.getrandbits(32);x[17]=rng.randrange(2);x[18]=rng.randrange(3);x[21]=rng.randrange(0x10000);x[25]=rng.randrange(4);x[26]=rng.randrange(2);x[27]=rng.randrange(256);x[28]=rng.randrange(4);x[30]=rng.getrandbits(32);x[31]=rng.getrandbits(32);rows.append(x)
 if a.case_file:rows=json.loads(a.case_file.read_text())
 if a.op is not None:rows=[x for x in rows if x[0]==a.op]
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 if a.replay_oracle:
  saved=json.loads(a.replay_oracle.read_text());assert saved['source_sha256']==SHA and saved['checker_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest();rows=saved['inputs'];expected=saved['outputs'];all_events=[[]for _ in rows]
 for index,row in enumerate([]if a.replay_oracle else rows):
  if index%64==0:a.report.with_suffix('.progress.json').write_text(json.dumps({'case':index,'total':len(rows),'input':row})+'\n')
  serial=1;reads=writes=calls=allocs=clocks=sleeps=frees=0;next_at=0x1200000;trace=1469598103934665603;failed=False;restore=None;frames=[];events=[];heaps=[];u.mem_write(0x1200000,bytes([row[27]])*0x100000);u.mem_write(0x10019880,bytes(0x426))
  for at,v in[(0x1000605c,0x1100100),(0x10006058,0x1100200),(0x10006068,0x1100300),(0x10006088,0x1100400),(0x10006010,0x1100500),(0x10006078,0x1100600),(0x1000607c,0x1100700),(0x1000603c,0x1100800),(0x10006070,0x1100900),(0x10019880,row[19]),(0x10019884,row[20]),(0x100198ac,row[17]),(0x100199b0,row[18]),(0x100198b8,row[1]),(0x100198bc,row[1]),(0x10019afc,0x1280000),(0x10019af8,0x1281000),(0x10007698,0x1282000),(0x1000769c,row[8]),(0x10019c5c,0x1283000),(0x10019c68,row[11]),(0x10019c6c,7),(0x10019c88,row[14]),(0x10019c96,row[15]),(0x10019c9a,row[16]),(0x10019c92,0x04030201),(0x10019854,0x128a000),(0x1001985c,0x1000),(0x10019868,0xa5a51234),(0x10007060,0xe65ae7d0),(0x10019b0c,0x128c000),(0x10019b10,0x128c000),(0x10019b14,0x128c000),(0x10019b18,row[29]),(0x10019b1c,0)]:put(at,v)
  put(0x10019c90,0x9876,2);put(0x10019c72,0xdf0,2);put(0x10007064,2827,2);u.mem_write(0x1280000,bytes(0x2000));u.mem_write(0x1282000,bytes(0x1000));u.mem_write(0x128c000,bytes(0x10000));u.mem_write(0x1287000,bytes(0x1000))
  for i in range(4):
   p=0x1280000+i*0xbc;ad=0x1281000+i*0x18
   for off,v in[(0x98,row[6]),(0x9c,row[5]),(0xa4,ad),(0xa0,0x10000+i),(0xac,row[7]),(0xb0,row[30]),(0xb8,row[31]),(0x80,0x1285000+i*0x40 if row[12]else 0)]:put(p+off,v)
   u.mem_write(p+0x88,bytes(j+i*17+1 for j in range(16)))
   for off,v in[(0,row[2]),(4,(row[3]+i)&MASK),(8,row[4]),(0x14,0xa5a51234+i)]:put(ad+off,v)
  for i in range(min(row[8],8)):
   at=0x1282000+i*0x4a
   if row[9]:
    put(at+0x12,1,2);put(at+0x10,1,2);u.mem_write(at,bytes(j+1 if row[9]==1 else j+33 for j in range(16)));put(at+0x14,0x8000,2);put(at+0x30,0xffff,2);put(at+0x32,9,2);put(at+0x46,10,2);put(at+0x48,0xabcd,2)
  string(0x10007050,'_______________');string(0x1286000,'Thu Oct 24 12:34:56 1998\n');string(0x1286200,'1.2.3.4');put(0x1283002,4,2);put(0x1284002,8,2)
  op=row[0];site=ROOT[op];at=STACK-LOCAL[site];frames=[(site,at,SIZE[site],0)];u.mem_write(at,b'\x5a'*SIZE[site]);args=[]
  if op in[1,3,4]:args=[0x1281000]
  if op==2:args=[0x1280000,row[10]]
  if op==5:args=[0x1280000]
  if op==6:args=[0x1280000,0x04030201]
  if op==7:args=[0x04030201]
  if op==8:args=[row[13]]
  active=True;result=0
  try:r.call(site,*args,count=4000000);result=u.reg_read(UC_X86_REG_EAX)
  except(unicorn.UcError,RuntimeError):
   if not failed:print('FAIL',row,hex(u.reg_read(UC_X86_REG_EIP)));raise
  finally:active=False
  if failed or site in VOID:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for base_at,n in[(0x10019880,0x426),(0x10007698,8),(0x10006010,0x7c),(0x1280000,0x8000),(0x128c000,0x100)]+[(at,n)for at,n in heaps if n]:
   for v in u.mem_read(base_at,n):state=((state^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events if a.limit==1 else events[:400])
 if not a.replay_oracle:a.report.with_suffix('.oracle.json').write_text(json.dumps({'source_sha256':SHA,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'inputs':rows,'outputs':expected})+'\n')
 p=subprocess.run([str(a.probe),str(a.game/'Netsocknl.dll')],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',text=True,capture_output=True);assert p.returncode==0,(p.returncode,p.stderr);actual=p.stdout.splitlines();assert len(actual)==len(rows),(len(actual),len(rows));bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i]}for i,(x,y)in enumerate(zip(actual,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'oracle_mode':'SHA-checked retained instruction output'if a.replay_oracle else'original instructions','cases':len(rows),'differences':len(bad),'mismatches':bad[:8],'mismatch_summary':[{k:v for k,v in m.items()if k!='original_events'}for m in bad],'bodies':[hex(x)for x in ROOT],'word_boundary':'Only10003E20->10005240 arg4 normalized to consumedWORD; deterministic ping highbits retained','network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--cases',type=int,default=128);p.add_argument('--limit',type=int,default=0);p.add_argument('--op',type=int,choices=range(15));p.add_argument('--case-file',type=Path);p.add_argument('--replay-oracle',type=Path);run(p.parse_args())
