#!/usr/bin/env python3
"""Compare host setup and usage bodies with current original DLL instructions."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,pefile,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,STACK
SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2';MASK=0xffffffff
ROOT=[0x100012e0,0x10004050,0x10004020,0x10004a40,0x10002510,0x10002330,0x10002240,0x10002290,0x10002310,0x100021d0,0x100028c0,0x100025a0,0x10002710,0x10002620]
SIZE=dict(zip(ROOT,[8,12,16,4,4,0x218,0x14,0x14,12,0x104,4,4,0x78,0x48]));LOCAL=dict(zip(ROOT,[0,0,0,0,0,0x218,0,0,0,0x104,0,0,0x70,0x3c]));ARGC=dict(zip(ROOT,[1,2,3,0,0,0,4,4,2,0,0,0,1,2]));VOID={ROOT[i]for i in[2,4,8,9,10,12,13]};FRAMELESS={ROOT[i]for i in[3,4,10,11]};SPANS={x:[(4,SIZE[x])]for x in ROOT};SPANS.update({ROOT[5]:[(0x10,0x218)],ROOT[9]:[(4,0x104)],ROOT[12]:[(0x10,0x6e),(0x74,0x78)],ROOT[13]:[(8,0x3c),(0x40,0x48)]})
for x in FRAMELESS:SPANS[x]=[]
for site,size in[(0x10003a60,8),(0x10004fa0,8),(0x10005240,0x14)]:SIZE[site]=size;LOCAL[site]=0;ARGC[site]=1 if size==8 else 4;SPANS[site]=[(4,size)]
def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA
 r=Rig(a.game);u=r.u;image=pefile.PE(data=raw).get_memory_mapped_image();u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,image)
 ranges=list(zip(ROOT,[0x10001415,0x1000408b,0x10004041,0x10004a7d,0x10002591,0x1000250a,0x10002281,0x10002301,0x10002330,0x1000223c,0x10002973,0x10002612,0x100028b7,0x10002703]))+[(0x10003a60,0x10003aa0),(0x10004fa0,0x10005008),(0x10005240,0x10005273)]
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls=set();returns={}
 for lo,hi in ranges:
  for i in md.disasm(image[lo-0x10000000:hi-0x10000000],lo):
   if i.mnemonic in['push','pop','call','ret']:controls.add(i.address)
   if i.mnemonic=='ret':returns[i.address]=lo
 row=None;reads=writes=calls=serial=allocs=next_at=clocks=lines=drop_lines=compares=0;trace=0;active=failed=False;restore=None;frames=[];events=[];heaps=[]
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,v,n=4):u.mem_write(at,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
 def text(at):
  s=bytearray()
  for i in range(1024):
   c=get(at+i,1)
   if not c:return s.decode('latin1')
   s.append(c)
  raise RuntimeError('String exceeds fixture')
 def string(at,s):u.mem_write(at,s.encode('latin1')+b'\0')
 def frame(site):return next((at for s,at,n,idx in reversed(frames)if s==site),0)
 def norm(at):
  for site,base,n,idx in reversed(frames):
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
  events.append(('C',site,[norm(x)for x in args]));event(3);event(site);event(len(args))
  for x in args:event(norm(x))
  calls+=1
  if calls==row[17]:stop();return False
  return True
 def allocate(n,clear=False):
  nonlocal allocs,next_at
  allocs+=1
  if allocs==row[12]:return 0
  if n>0x20000 or next_at+n>0x1270000:raise RuntimeError('Allocation exceeds owner')
  at=next_at;next_at+=(n+255)&~255;heaps.append((at,n))
  if clear:u.mem_write(at,bytes(n))
  return at
 imports={0x1100100:1,0x1101100:1,0x1100200:2,0x1100300:2,0x1100400:2,0x1100500:3,0x1100600:1,0x1100700:1,0x1100800:1,0x1100900:3,0x1101200:3,0x1100a00:2,0x1100b00:3,0x1100c00:4,0x1100d00:1,0x1100e00:1,0x1101400:1,0x1100f00:0,0x1101300:0,0x1101000:3,0x10005946:1,0x10005940:1,0x10005020:3}
 def foreign(site):
  nonlocal clocks,lines,drop_lines,compares
  sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(imports[site])]
  if not call_event(site,args):return
  m=row[14];result=0
  if site in[0x1100100,0x1101100]:
   if m&1:put(0x1000605c,0x1101100);put(0x1280008,row[1]+1)
   result=allocate(args[0])
  elif site==0x1100200:
   if m&2:put(0x100198b8,1);put(0x128109c,1);put(0x1281098,0)
   result=allocate((args[0]*args[1])&MASK,True)
  elif site==0x1100300:
   if not row[13]:
    old=get(0x1000769c);result=allocate(args[1])
    if result:
     if old*0x4a>0x100000:raise RuntimeError("Reallocation copy exceeds owner")
     u.mem_write(result,bytes(u.mem_read(args[0],old*0x4a)))
    if result and m&32:put(0x1000769c,1)
  elif site==0x1100400:
   name=text(args[0]);assert name in['NWUSAGE.CFG','LOSEPCKT.NFO'];result=(1 if row[10]else 0)if name=='NWUSAGE.CFG'else(2 if row[11]else 0)
  elif site==0x1100500:
   if args[2]==1:
    source=['NWUSAGE_PORT = 4567\n','NWUSAGE_IPADDR = 127.0.0.1\n','NWUSAGE_APPID = A?BC????????????\n']if row[10]==1 else['; comment\n','BAD\n','\n']if row[10]==3 else[]
    if lines<len(source):string(args[0],source[lines]);lines+=1;result=args[0]
   elif args[2]==2:
    if drop_lines==0 and row[11]!=2:string(args[0],'-1\n'if row[11]==3 else'3\n');result=args[0]
    drop_lines+=1
   else:raise RuntimeError('Unknown FILE')
  elif site==0x1100600:pass
  elif site==0x1100700:
   s=text(args[0]);i=0
   while i<len(s)and s[i].isspace():i+=1
   neg=False
   if i<len(s)and s[i]in['-','+']:neg=s[i]=='-';i+=1
   while i<len(s)and'0'<=s[i]<='9':result=(result*10+ord(s[i])-48)&MASK;i+=1
   if neg:result=(-result)&MASK
  elif site==0x1100800:
   s=text(args[0]);string(args[0],s.upper())
   if m&512 and frame(0x10002330):put(frame(0x10002330)+0x10,2)
   result=args[0]
  elif site in[0x1100900,0x1101200]:
   compares+=1
   if m&8:put(0x10006054,0x1101200)
   left=text(args[0]);right=text(args[1])
   for i in range(args[2]):
    x=ord(left[i])if i<len(left)else 0;y=ord(right[i])if i<len(right)else 0
    if x!=y:result=(x-y)&MASK;break
    if not x:break
  elif site==0x1100a00:
   s=text(args[0]);needle=text(args[1]);at=s.find(needle)
   if m&1024 and(frame(0x10002240)or frame(0x10002290)):put((frame(0x10002240)or frame(0x10002290))+0x10,0x1284700)
   result=0 if at<0 else args[0]+at
  elif site==0x1100b00:
   s=text(args[1]);u.mem_write(args[0],(s.encode()+bytes(args[2]))[:args[2]]);result=args[0]
  elif site==0x1100c00:
   string(args[0],'00000000:0000');result=13
   if m&16:put(0x1000769c,1);put(0x10006088,0x1101300)
  elif site==0x1100d00:
   if m&8192:put(0x10006064,0x1101400)
   result=0x12345678
  elif site in[0x1100e00,0x1101400]:put(0x12860f0,args[0])
  elif site in[0x1100f00,0x1101300]:
   clocks+=1
   if m&4:put(0x1000769c,1)
   result=(row[8]+clocks-1)&MASK
  elif site==0x1101000:
   put(0x12860f4,args[0]);put(0x12860f8,get(0x100198ac))
   if m&128:put(0x100198ac,9);put(0x100199b0,9)
   result=MASK
  elif site==0x10005946:result=0x0100007f if text(args[0])=='127.0.0.1'else MASK
  elif site==0x10005940:result=0x1285000
  elif site==0x10005020:
   if args[1]>0x100:raise RuntimeError('Capture exceeds owner')
   u.mem_write(0x1286000,bytes(u.mem_read(args[0],args[1])));put(0x12860e0,args[2]);put(0x12860e4,get(0x10019868,2))
   if m&256 and frame(0x10002620):put(frame(0x10002620)+0x40,0x128304a);put(frame(0x10002620)+0x44,1);put(0x10006088,0x1101300)
  r.finish(result)
  if site in[0x10005946,0x10005940]:u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+imports[site]*4)
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
  if not any(lo<=pc<hi for lo,hi in ranges):return
  if 0x2000000<=at<0x2010000:
   if pc in controls:return
   if not any(any(base+lo<=at and at+n<=base+hi for lo,hi in SPANS[s])for s,base,size,idx in frames):return
  mapped=any(base<=at and at+n<=base+size for base,size in[(0x10000000,0x1b000),(0x1200000,0x100000)]+[(base,size)for s,base,size,idx in frames])
  if kind==unicorn.UC_MEM_WRITE:
   v&=(1<<(8*n))-1;events.append(('W',norm(at),n,norm(v)));event(2);event(norm(at));event(n);event(norm(v));writes+=1
   if writes==row[16]or not mapped:
    try:restore=(at,get(at,n),n)
    except unicorn.UcError:pass
    stop();return
   if row[14]&2048 and at==0x100198b0:put(0x1280008,row[1]+1)
  else:
   events.append(('R',norm(at),n));event(1);event(norm(at));event(n);reads+=1
   if reads==row[15]or not mapped:stop();return
   if row[14]&4096 and at==0x1001986c:put(0x1283014,0x8000,2)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,n,v,user):
  if not failed:memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,n,v,user);stop()
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def base(op=0):return[op,3,1,1,0x100,0,3,0,400001,0,0,0,0,0,0,0,0,0,0x5a,0,400001,0,0,7,0xe65ae7d0,2827,0,0,0,0,0,0]
 rows=[];rng=random.Random(0x100012e0)
 for op in range(14):
  for m in[0]+[1<<i for i in range(14)]+[0x3fff]:x=base(op);x[14]=m;x[10]=1 if op in[5,6,7]else 0;rows.append(x)
 for op in range(14):
  for field,values in[(1,[0,1,4,0x80000000]),(2,[0]),(3,[0]),(4,[0,0x100]),(5,[1]),(6,[0,1,4,0x80000000]),(7,[1,2,3]),(8,[0,15000,15001,120001,300000,300001,0x7fffffff,0x80000000,MASK]),(9,[1,400000,MASK]),(10,[1,2,3]),(11,[1,2,3]),(12,[1,2,3,4,5]),(13,[1]),(19,[1,2,3,MASK]),(20,[1000,0x80000000,MASK]),(21,[59,60,MASK]),(24,[0]),(26,[8])]:
   for v in values:
    x=base(op);x[field]=v
    if field==1 and v>0x10000:x[15]=100
    rows.append(x)
  for field,values in[(15,[1,2,3,4,5,8,10,15,20,30,40,60,80,100,200]),(16,[1,2,3,4,5,8,10,20,30,67,68,100,200,1000,16400]),(17,list(range(1,13)))]:
   for n in values:x=base(op);x[field]=n;rows.append(x)
 for _ in range(a.cases):
  x=base(rng.randrange(14));x[1]=rng.randrange(5);x[6]=rng.randrange(5);x[7]=rng.randrange(4);x[8]=rng.getrandbits(32);x[9]=rng.getrandbits(32);x[10]=rng.randrange(4);x[11]=rng.randrange(4);x[12]=rng.randrange(6);x[13]=rng.randrange(2);x[14]=rng.randrange(0x4000);x[18]=rng.randrange(256);x[19]=rng.choice([0,1,2,3,MASK]);x[20]=rng.getrandbits(32);x[21]=rng.randrange(65);x[22]=rng.getrandbits(32);x[23]=rng.getrandbits(32);x[25]=rng.getrandbits(32);rows.append(x)
 if a.case_file:rows=json.loads(a.case_file.read_text())
 if a.late_calls:
  rows=[]
  for op in[0,3]:
   for n in range(13,22):x=base(op);x[17]=n;rows.append(x)
 if a.op is not None:rows=[x for x in rows if x[0]==a.op]
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 if a.replay_oracle:
  saved=json.loads(a.replay_oracle.read_text());assert saved['source_sha256']==SHA and saved['checker_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest();rows=saved['inputs'];expected=saved['outputs'];all_events=[[]for _ in rows]
 for case_index,row in enumerate([]if a.replay_oracle else rows):
  if case_index%32==0:a.report.with_suffix('.progress.json').write_text(json.dumps({'case':case_index,'total':len(rows),'input':row})+'\n')
  reads=writes=calls=allocs=clocks=lines=drop_lines=compares=0;serial=1;next_at=0x1200000;trace=1469598103934665603;failed=False;restore=None;frames=[];events=[];heaps=[]
  u.mem_write(0x1200000,bytes([row[18]])*0x100000);u.mem_write(0x10019880,bytes(0x426))
  for at,v in[(0x1000605c,0x1100100),(0x10006050,0x1100200),(0x10006058,0x1100300),(0x10006060,0x1100400),(0x10006020,0x1100500),(0x10006018,0x1100600),(0x1000601c,0x1100700),(0x10006024,0x1100800),(0x10006054,0x1100900),(0x10006080,0x1100a00),(0x10006074,0x1100b00),(0x1000606c,0x1100c00),(0x10006078,0x1100d00),(0x10006064,0x1100e00),(0x10006088,0x1100f00),(0x1000604c,0x1101000),(0x10019880,row[2]),(0x10019884,row[3]),(0x100198b8,row[1]),(0x10019afc,0x1281000),(0x10019af8,0x1282000),(0x10007698,0x1283000),(0x1000769c,row[6]),(0x10019874,row[9]),(0x10019878,row[20]),(0x1001986c,row[23]),(0x10007060,row[24]),(0x1280000,row[4]),(0x1280004,row[26]),(0x1280008,row[1]),(0x10019854,0x1288000),(0x1001985c,0x1000),(0x10019868,0xa5a51234),(0x128500c,0x1285100),(0x1285100,0x1285200),(0x1285200,0x0100007f)]:put(at,v)
  put(0x10007064,row[25],2);put(0x10019c72,0xdf0,2);u.mem_write(0x128000c,bytes(0xf4));put(0x1280090,row[5],1);u.mem_write(0x1281000,bytes(0x2000));u.mem_write(0x1283000,bytes(0x400))
  for i in range(min(row[6],8)):
   at=0x1283000+i*0x4a
   if row[7]:
    put(at+0x12,1,2);put(at+0x10,3 if row[7]==2 else 1,2);put(at+0x3e,row[22]);put(at+0x42,row[21]);u.mem_write(at,bytes(range(1,17)));u.mem_write(at+0x20,bytes(range(0x80,0x90)))
  if row[7]==3:
   for i in range(min(row[6],8)):
    at=0x1283000+i*0x4a;string(at,'00000000:0000');put(at+14,0x5a5a,2);put(at+0x14,0x8000,2);put(at+0x30,0xffff,2);put(at+0x32,7,2);put(at+0x46,123,2);put(at+0x48,0xabcd,2)
  string(0x10007040,'???????????????');string(0x10007050,'_______________');string(0x1284000,'NWUSAGE_PORT = 4567 ;');string(0x1284500,'??A?BC??????????');u.mem_write(0x1286000,bytes(0x100))
  op=row[0];site=ROOT[op];at=STACK-LOCAL[site];frames=[(site,at,SIZE[site],0)];u.mem_write(at,b'\x5a'*SIZE[site]);args=[]
  if op==0:args=[0x1280000]
  if op==1:args=[MASK,MASK]
  if op==2:args=[0x1282000,0x04030201,0xa5a51234]
  if op in[6,7]:args=[0x1284000,1,0x1284000,0x1284600]
  if op==8:args=[0x1284600,0x1284500]
  if op==12:args=[row[19]]
  if op==13:args=[0x1283000,row[19]]
  active=True;result=0
  try:r.call(site,*args,count=4000000);result=u.reg_read(UC_X86_REG_EAX)
  except(unicorn.UcError,RuntimeError):
   if not failed:print('FAIL',row,hex(u.reg_read(UC_X86_REG_EIP)));raise
  finally:active=False
  if failed or site in VOID:result=0
  if restore:put(*restore)
  state=1469598103934665603
  spans=[(0x10019880,0x426),(0x10007040,0x30),(0x10007698,8),(0x1001986c,16),(0x10006018,0x70),(0x1280000,0x8000)]
  for base_at,n in heaps:spans+=([(base_at,n)]if n<=0x3000 else[(base_at,64),(base_at+n-64,64)])
  for base_at,n in spans:
   for v in u.mem_read(base_at,n):state=((state^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events if a.limit==1 else events[:400])
 if not a.replay_oracle:a.report.with_suffix('.oracle.json').write_text(json.dumps({'source_sha256':SHA,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'inputs':rows,'outputs':expected})+'\n')
 p=subprocess.run([str(a.probe),str(a.game/'Netsocknl.dll')],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',text=True,capture_output=True);assert p.returncode==0,(p.returncode,p.stderr);actual=p.stdout.splitlines();assert len(actual)==len(rows),(len(actual),len(rows));bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i][:40000]}for i,(x,y)in enumerate(zip(actual,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'oracle_mode':'verified retained instruction output'if a.replay_oracle else'original instructions','cases':len(rows),'differences':len(bad),'mismatches':bad[:8],'bodies':[hex(x)for x in ROOT],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--cases',type=int,default=128);p.add_argument('--limit',type=int,default=0);p.add_argument('--late-calls',action='store_true');p.add_argument('--op',type=int,choices=range(14));p.add_argument('--case-file',type=Path);p.add_argument('--replay-oracle',type=Path);run(p.parse_args())
