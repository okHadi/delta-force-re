#!/usr/bin/env python3
"""Compare the original DLL ring init/enqueue/destroy with current raw memory."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,pefile,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP
from check_projectiles_original import Rig,STACK
SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2'
MASK=0xffffffff;Q=0x10019b20;OTHER=0x10019c38;ROOT=[0x10003a60,0x10003b20,0x10003ad0]
def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA
 r=Rig(a.game);u=r.u;image=pefile.PE(data=raw).get_memory_mapped_image();u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,image)
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls={i.address for i in md.disasm(image[0x3a60:0x3c00],0x10003a60)if i.mnemonic in['push','pop','call','ret']}
 row=None;reads=writes=calls=sleeps=0;trace=0;failed=False;active=False;restore=None
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,value,n=4):u.mem_write(at,(value&((1<<(8*n))-1)).to_bytes(n,'little'))
 def tail():return 0x1200000+(0xfff0 if row[4]else 0)
 def norm(at):return 0x30000000+at-STACK if STACK<=at<STACK+0x1c else at
 def event(n):
  nonlocal trace
  for i in range(4):trace=((trace^((n>>(i*8))&255))*1099511628211)&0xffffffffffffffff
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def foreign(site):
  nonlocal calls,sleeps
  sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(2 if site==0x10003aa0 else 1)];event(3);event(site);event(len(args))
  for n in args:event(norm(n))
  calls+=1
  if calls==row[10]:stop();return
  result=0
  if site==0x1100100:
   sleeps+=1
   if row[7]&256:put(0x10006010,0x1100400)
   if sleeps>=row[6]:put(Q+0x10,0)
  elif site==0x1100200:
   if row[7]&1:put(STACK+4,OTHER if row[0]==0 else 0x1211800)
   if row[7]&8:put(Q,0x1220000);put(Q+0xc,row[2]^0x80000000)
   result=0 if row[11]else 0x1200000 if row[0]==0 else 0x1211001 if row[12]==1 else 0x1212000
  elif site==0x1100300:
   if row[7]&2:put(0x10006068,0x1100400)
   if row[7]&128:put(Q,0x1220000)
  r.finish(result)
  if site==0x1100100:u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+4)
 for site in[0x1100100,0x1100200,0x1100300,0x10003aa0]:r.stubs[site]=lambda site=site:foreign(site)
 def memory(uc,kind,at,width,value,_):
  nonlocal reads,writes,restore
  if not active:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not 0x10003a60<=pc<0x10003c00:return
  if 0x2000000<=at<0x2010000 and(pc in controls or not STACK+4<=at<STACK+0x1c):return
  if kind==unicorn.UC_MEM_WRITE:
   event(2);event(norm(at));event(width);event(norm(value&MASK));writes+=1
   if writes==row[9]or at<0x1000:
    if at>=0x1000:restore=(at,get(at,width),width)
    stop();return
   if row[7]&4 and at==tail()+4:put(tail(),0x1210080)
   if row[7]&32 and at==Q+8:put(Q+0xc,0x13579bdf)
   if row[7]&512 and at==Q:put(STACK+4,OTHER)
  else:
   event(1);event(norm(at));event(width);reads+=1
   if reads==row[8]:stop();return
   if row[7]&64 and at==STACK+4 and row[0]==1:put(STACK+8,MASK)
   if row[7]&16 and at in[0x1211000,0x1211800]:put(Q+0xc,row[2]^MASK)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 rng=random.Random(0x10003b20);rows=[]
 def base(op):return [op,7,1,0,0,0,0,0,0,0,0,0,0,13,0,0]
 for op in range(3):
  for mutation in[0,1,2,4,8,16,32,64,128,256,512,1023]:x=base(op);x[7]=mutation;x[5]=1;x[6]=2;x[15]=1;rows.append(x)
 for count in[0,1,0x7ff,0x800,0xff5,0xff6,0x7fffffff,0x80000000,MASK]:
  for priority in[0,1,2,MASK]:
   for length in[0,1,3,4,7,64]:x=base(1);x[2]=count;x[3]=priority;x[1]=length;rows.append(x)
 for op in range(3):
  for field,limit in[(8,20),(9,12),(10,8)]:
   for n in range(1,limit):x=base(op);x[field]=n;x[5]=1;x[6]=2;x[15]=1;rows.append(x)
  x=base(op);x[11]=1;rows.append(x);x=base(op);x[14]=1;rows.append(x)
 for _ in range(a.cases):
  x=base(rng.randrange(3));x[1]=rng.randrange(65);x[2]=rng.choice([0,1,0x800,0xff6,0x80000000,MASK]);x[3]=rng.randrange(3);x[4]=rng.randrange(2);x[5]=rng.randrange(2);x[6]=rng.randrange(4);x[7]=rng.randrange(1024);x[11]=rng.randrange(5)==0;x[12]=rng.randrange(3);x[13]=rng.randrange(256);x[14]=rng.randrange(5)==0;x[15]=rng.randrange(2);rows.append(list(map(int,x)))
 expected=[]
 for row in rows:
  reads=writes=calls=sleeps=0;trace=1469598103934665603;failed=False;restore=None
  u.mem_write(0x1200000,bytes([0xa5 if row[0]==0 else 0])*0x10000);u.mem_write(0x1220000,bytes(0x10000));u.mem_write(0x1210000,bytes(0x3000));u.mem_write(0x2000000,bytes(0x10000))
  for i in range(0x400):put(0x1211000+i,(i+row[13])&255,1);put(0x1211800+i,(i*3+row[13])&255,1);put(0x1212000+i,0xcc,1)
  put(0x10006010,0x1100100);put(0x1000605c,0x1100200);put(0x10006068,0x1100300)
  for q in[Q,OTHER]:
   for offset,value in[(0,0 if row[14]else 0x1200000),(4,0x1200000),(8,tail()),(0xc,row[2]),(0x10,1 if row[6]else 0)]:put(q+offset,value)
  if row[0]!=0:
   for offset,value in[(0,0x1210000 if row[5]else 0),(4,0x12345678),(8,5),(0xc,7)]:put(tail()+offset,value)
   if row[15]:put(0x120fff0,0x1210080)
  args=[tail()+4 if row[12]==2 else 0x1211000,row[1],Q,0x01020304,0x11223344,row[3]]if row[0]==1 else[Q]
  active=True
  try:r.call(ROOT[row[0]],*args)
  except(unicorn.UcError,RuntimeError):
   if not failed:raise
  finally:active=False
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in[(Q,20),(OTHER,20),(0x1200000,64),(0x120ffc0,64),(0x1220000,64),(0x122ffc0,64),(0x1210000,0x3000)]:
   for value in u.mem_read(at,n):state=((state^value)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {reads} {writes} {calls} {trace} {state}')
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',text=True,capture_output=True);assert p.returncode==0,p.stderr;lines=p.stdout.splitlines();assert len(lines)==len(expected)
 bad=[{'input':rows[i],'actual':x,'expected':y}for i,(x,y)in enumerate(zip(lines,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'bodies':['10003A60','10003B20','10003AD0'],'cases':len(rows),'differences':len(bad),'mismatches':bad[:5],'comparison':'orderedcurrentread/store/callhash pluscanonicalstate;full64Kinitwrites,4096slotcleanup,signedgates,wrap,cachedfree/sleep,currentmallocargs/currentbase/count,aliases,nullallocation/failureprefix','network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=128);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
