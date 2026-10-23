#!/usr/bin/env python3
"""Compare current outgoing drain/selector/port wrappers with original DLL code."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,pefile,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,STACK
SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2';MASK=0xffffffff
ROOT=[0x100048c0,0x10003dd0,0x10005240,0x100058d0];SIZES={ROOT[0]:12,ROOT[1]:20,ROOT[2]:20,ROOT[3]:20,0x10003c00:24}
def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA
 r=Rig(a.game);u=r.u;image=pefile.PE(data=raw).get_memory_mapped_image();u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,image)
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);ranges=[(0x100048c0,0x1000492f),(0x10003dd0,0x10003e13),(0x10005240,0x10005273),(0x100058d0,0x10005903),(0x10003c00,0x10003dd0)];controls=set();returns=set()
 for start,end in ranges:
  for i in md.disasm(image[start-0x10000000:end-0x10000000],start):
   if i.mnemonic in['push','pop','call','ret']:controls.add(i.address)
   if i.mnemonic=='ret':returns.add(i.address)
 row=None;reads=writes=calls=serial=leaves=sleeps=0;trace=0;active=failed=False;restore=None;frames=[];events=[]
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,value,n=4):u.mem_write(at,(value&((1<<(8*n))-1)).to_bytes(n,'little'))
 def norm(at):
  for site,base,size,index in reversed(frames):
   if base<=at<base+size:return 0x30000000+index*0x100+at-base
  return at
 def event(n):
  nonlocal trace
  for i in range(4):trace=((trace^((n>>(i*8))&255))*1099511628211)&0xffffffffffffffff
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def call_event(site,args):
  nonlocal calls
  events.append(('C',site,[norm(n)for n in args]));event(3);event(site);event(len(args))
  for n in args:event(norm(n))
  calls+=1
  if calls==row[8]:stop();return False
  if row[5]&1024 and site in[0x10005020,0x10005860]:put(args[0],0xee,1)
  return True
 def foreign(site):
  nonlocal leaves,sleeps
  sp=u.reg_read(UC_X86_REG_ESP);argc=3 if site in[0x10005020,0x10005860]else 2 if site==0x10003aa0 else 1;args=[get(sp+4+i*4)for i in range(argc)]
  if not call_event(site,args):return
  result=0
  if site in[0x10005020,0x10005860]:
   leaves+=1
   if leaves==row[17]or args[1]>0x1000:stop();return
   total=2166136261
   for i in range(args[1]):total=((total^get(args[0]+i,1))*16777619)&MASK
   at=0x1228000+(leaves-1)*32
   for off,value in[(0,site),(4,args[0]),(8,args[1]),(12,args[2]),(16,get(0x10019868,2)),(20,total)]:put(at+off,value)
   if row[5]&2:put(0x10019c5c,0x1224000 if get(0x10019c5c)==0x1220000 else 0x1220000)
   if row[5]&8:put(0x10019868,row[18],2)
   if row[5]&16:put(0x10019c78,get(0x10019c78)^1)
   result=row[4]
  elif site in[0x1100200,0x1100400]:
   if row[5]&1:put(0x10019c5c,0x1224000)
   if row[5]&32:put(0x10006068,0x1100400)
  elif site in[0x1100300,0x1100500]:
   sleeps+=1
   if row[5]&256:put(0x10006010,0x1100500)
   if sleeps>=2:put(0x10019b30,0)
  r.finish(result)
  if site in[0x1100300,0x1100500]:u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+4)
 for site in[0x10005020,0x10005860,0x1100200,0x1100400,0x1100300,0x1100500,0x10003aa0]:r.stubs[site]=lambda site=site:foreign(site)
 def code(uc,pc,size,_):
  nonlocal serial
  if not active:return
  if pc in SIZES and not(pc==ROOT[row[0]]and u.reg_read(UC_X86_REG_ESP)==STACK):
   sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(SIZES[pc]//4-1)]
   if not call_event(pc,args):return
   frames.append((pc,sp,SIZES[pc],serial));serial+=1
  if pc in returns and len(frames)>1:frames.pop()
 u.hook_add(unicorn.UC_HOOK_CODE,code)
 def memory(uc,kind,at,width,value,_):
  nonlocal reads,writes,restore
  if not active:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not any(start<=pc<end for start,end in ranges):return
  if 0x2000000<=at<0x2010000 and(pc in controls or not any(base+(0 if site==ROOT[0]else 4)<=at and at+width<=base+size for site,base,size,index in frames)):return
  mapped=any(base<=at and at+width<=base+size for base,size in[(0x10000000,0x1b000),(0x1200000,0x30000)]+[(base,size)for site,base,size,index in frames])
  if kind==unicorn.UC_MEM_WRITE:
   value&=(1<<(8*width))-1;events.append(('W',norm(at),width,norm(value)));event(2);event(norm(at));event(width);event(norm(value));writes+=1
   if writes==row[7]or not mapped:
    if at>=0x1000:restore=(at,get(at,width),width)
    stop();return
   if row[5]&4 and at==0x10019868:put(frames[-1][1]+4,0x1224000)
  else:
   events.append(('R',norm(at),width));event(1);event(norm(at));event(width);reads+=1
   if reads==row[6]or not mapped:stop();return
   if row[5]&64 and at==0x10019b2c:put(0x10019c5c,0x1224000)
   if row[5]&128 and at==0x10019c78:put(frames[-1][1]+16,row[18])
   if row[5]&512 and frames[-1][0]in[0x10005240,0x100058d0]and at==frames[-1][1]+12:put(frames[-1][1]+8,row[9]+3)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,width,value,user):
  if failed:return False
  memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,width,value,user);stop();return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def base(op=0):return[op,2,0,0x5678,0x12345678,0,0,0,0,7,0x01020304,0xcafe1234,MASK,0,0,13,0,0,0x4321,0]
 rng=random.Random(0x100048c0);rows=[]
 for op in range(4):
  for mutation in[0,1,2,4,8,16,32,64,128,256,512,1024,2047]:
   for compressed in[0,1]:x=base(op);x[2]=compressed;x[5]=mutation;x[13]=1;rows.append(x)
 for count in[0,1,2,3]:
  for wrap in[0,1]:
   for counter in[0,1,0x7fffffff,0x80000000,MASK]:x=base();x[1]=count;x[19]=wrap;x[12]=counter;rows.append(x)
 for op in range(4):
  for field,positions in[(6,list(range(1,25))+[31,40,64,80,100]),(7,list(range(1,12))),(8,list(range(1,15)))]:
   for n in positions:x=base(op);x[field]=n;rows.append(x)
 for op in range(4):
  for leaf_fail in[1,2,3]:x=base(op);x[17]=leaf_fail;rows.append(x)
 for _ in range(a.cases):
  x=base(rng.randrange(4));x[1]=rng.randrange(4);x[2]=rng.getrandbits(32);x[3]=rng.getrandbits(32);x[4]=rng.getrandbits(32);x[5]=rng.randrange(2048);x[9]=rng.choice([0,1,3,4,7,31,32,33]);x[10]=rng.getrandbits(32);x[11]=rng.getrandbits(32);x[12]=rng.choice([0,1,MASK]);x[13]=rng.randrange(2);x[15]=rng.randrange(256);x[17]=rng.choice([0,0,0,1,2]);x[18]=rng.getrandbits(32);x[19]=rng.randrange(2);rows.append(x)
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 for row in rows:
  reads=writes=calls=leaves=sleeps=0;serial=1;trace=1469598103934665603;failed=False;restore=None;events=[];u.mem_write(0x1200000,bytes(0x30000));u.mem_write(0x1210000,bytes((i+row[15])&255 for i in range(0x3000)));u.mem_write(0x1220000,b'\xaa'*0x4000);u.mem_write(0x1224000,b'\xbb'*0x4000);head=0xfff0 if row[19]else 0
  for i in range(row[1]):
   at=0x1200000+((head+i*16)&65535)
   for off,value in[(0,0x1210000+i*0x100),(4,row[9]+i),(8,row[10]+i),(12,row[11]+i)]:put(at+off,value)
  for at,value in[(0x10019b20,0x1200000),(0x10019b24,0x1200000+head),(0x10019b28,0x1200000+((head+row[1]*16)&65535)),(0x10019b2c,row[1]if row[12]==MASK else row[12]),(0x10019b30,row[13]),(0x10019c5c,0x1220000),(0x10019c78,row[2]),(0x10006068,0x1100200),(0x10006010,0x1100300),(0x10019868,0xa5a50000|(row[3]&65535))]:put(at,value)
  frame=STACK-12 if row[0]==0 else STACK;size=12 if row[0]==0 else 20;frames=[(ROOT[row[0]],frame,size,0)];u.mem_write(frame,b'\x5a'*size);args=[]if row[0]==0 else[0x1210000,row[9],row[10],row[11]];active=True;result=0
  try:r.call(ROOT[row[0]],*args);result=u.reg_read(UC_X86_REG_EAX)if row[0]else 0
  except(unicorn.UcError,RuntimeError):
   if not failed:print('FAIL',row,hex(u.reg_read(UC_X86_REG_EIP)));raise
  finally:active=False
  if failed:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in[(0x1200000,0x10000),(0x1210000,0x3000),(0x1220000,0x8100),(0x10019b20,20),(0x10019c5c,4),(0x10019c78,4),(0x10019868,4),(0x10006068,4),(0x10006010,4)]:
   for value in u.mem_read(at,n):state=((state^value)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events)
 p=subprocess.run([str(a.probe),str(a.game/'Netsocknl.dll')],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',text=True,capture_output=True);assert p.returncode==0,p.stderr;lines=p.stdout.splitlines();assert len(lines)==len(rows),(len(lines),len(rows));bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i][:100]}for i,(x,y)in enumerate(zip(lines,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'bodies':[hex(x)for x in ROOT],'cases':len(rows),'differences':len(bad),'mismatches':bad[:5],'network_output':0,'leaf_sends':'required controlledfixtures; realqueue_pop body executes'},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=128);p.add_argument('--report',type=Path,required=True);p.add_argument('--limit',type=int,default=0);run(p.parse_args())
