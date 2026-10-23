#!/usr/bin/env python3
"""Compare the DF Netsock bridge with exact original instructions and live cells."""
import argparse,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256

MASK=0xffffffff
ROOTS=[0x497790,0x4977a0,0x4977e0,0x4b9530,0x4b95d0,0x4b96b0,0x4b8ec0,0x4b8ef0]
ARGS={0x4b9530:0,0x4b95d0:2,0x4b96b0:3,0x4b8e10:0}
CELLS=[0x501320,0x501398,0xd0a540,0x501344,0x501354,0x501364,0x50135c,0x501360,0x501328]
def run(a):
 r=Rig(a.game);u=r.u;row=None;trace=[];frames=[];reads=writes=calls=sends=receives=heads=0;active=False;failed=False;first=True;restore=None
 def put(at,value):u.mem_write(at,(value&MASK).to_bytes(4,'little'))
 def get(at):return int.from_bytes(u.mem_read(at,4),'little')
 def norm(at):
  for i,frame in enumerate(frames):
   if frame<=at<frame+4:return 0x30000000+i*0x100+at-frame
  return at
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def called(site,args):
  nonlocal calls
  trace.append('C'+str(site)+''.join(','+str(norm(n))for n in args)+';');calls+=1
  if calls==row[12]:stop();return False
  return True
 def own(uc,at,size,_):
  nonlocal first
  if not active:return
  if at in ROOTS or at in ARGS:
   if first:first=False
   elif at in ARGS:
    sp=u.reg_read(UC_X86_REG_ESP)
    if not called(at,[get(sp+4+i*4)for i in range(ARGS[at])]):return
   if at==0x4b9530:frames.append(u.reg_read(UC_X86_REG_ESP)-0x14)
 u.hook_add(unicorn.UC_HOOK_CODE,own)
 ranges=[(0x497790,0x497799),(0x4977a0,0x4977b6),(0x4977e0,0x4977fd),(0x4b9530,0x4b95c4),(0x4b95d0,0x4b9635),(0x4b96b0,0x4b971e),(0x4b8ec0,0x4b8eea),(0x4b8ef0,0x4b8f18),(0x4b8e10,0x4b8e35)]
 table_words=[table+off for table in [0x501ab8,0x501a28]for off in [0x54,0x58,0x60]]
 def memory(uc,kind,at,width,value,_):
  nonlocal reads,writes,restore
  if not active:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not any(lo<=pc<hi for lo,hi in ranges):return
  if at not in CELLS+table_words+frames:return
  assert width==4
  if kind==unicorn.UC_MEM_WRITE:
   trace.append(f'W{norm(at)},{norm(value&MASK)};');writes+=1
   if writes==row[11]:restore=(at,get(at));stop();return
   if row[9]&512 and at in frames:put(0x501320,get(0x501320)^MASK)
  else:
   trace.append(f'R{norm(at)};');reads+=1
   if reads==row[10]:stop();return
   if row[9]&1 and at==0x501398:put(0x501398,0x501ab8)
   if row[9]&4 and at==0x501344:put(0x501354,get(0x501354)^MASK);put(0xd0a540,get(0xd0a540)^0x2000)
   if row[9]&256 and at in [0x501ab8+0x54,0x501ab8+0x58,0x501ab8+0x60]:put(0x501398,0x501a28)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 foreign_args={0x4c551a:3,0x4c5514:2,0x4c550e:3,0x490880:0,0x4b9400:1,0x4c5562:0,0x4983b0:2,0x4c555c:0,0x498400:1,0x4b8da0:1,0x4b83b0:0,0x4b8470:2,0x4b85b0:3}
 def foreign(site):
  nonlocal sends,receives,heads
  sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(foreign_args[site])]
  if not called(site,args):return
  result=0
  if site==0x4c551a:
   receives+=1
   if row[9]&2:put(0xd0a540,get(0xd0a540)^0x1000)
   if receives<=row[14]or row[4]:put(args[1],row[5]+receives);put(args[2],0 if receives<=row[14]else row[4])
   else:result=12
  elif site in [0x4c5514,0x4c550e]:
   sends+=1;result=0x10 if sends<=row[8]else row[7]
  elif site==0x490880:
   if row[13]in[1,2,3]:put(0x501364,{1:0x800,2:0,3:MASK}[row[13]])
  elif site==0x4b9400:
   if row[9]&16:put(0x501320,0);put(0xd0a540,get(0xd0a540)^0x4000)
  elif site==0x4c5562:
   if row[9]&32:put(0x501320,0);put(0xd0a540,get(0xd0a540)^0x8000)
   result=row[7]
  elif site==0x4983b0:result=row[16]
  elif site==0x4c555c:
   if row[9]&64:put(0xd0a540,get(0xd0a540)^0x10000)
   result=row[7]
  elif site==0x498400:
   if row[9]&128:put(0xd0a540,0x12345678);put(0x501320,7)
  elif site==0x4b8da0:
   heads=max(0,heads-1);put(0x501328,get(0x501328)+0x100 if heads else 0)
  else:result=row[7]
  r.finish(result)
 for site in foreign_args:r.stubs[site]=lambda site=site:foreign(site)
 rng=random.Random(0x4977e0);rows=[]
 def base(op):return [op,1,1,0x1208000,7,93,MASK,0,0,0,0,0,0,0,0,0,0x1210000]
 for op in range(8):
  for enabled in [0,1,MASK]:
   for table in [0,1]:
    for mutation in [0,1,2,4,16,32,64,255,256,512,1023]:
     x=base(op);x[1]=enabled;x[2]=table;x[9]=mutation;x[14]=2;x[15]=3;rows.append(x)
 for op in [1,2,4,5]:
  for attempts in [0,1,2,2048,2049]:
   for pump in [0,1,3]:x=base(op);x[8]=attempts;x[13]=pump;rows.append(x)
 for op in range(8):
  for what,limit in [(10,16),(11,10),(12,7)]:
   for count in range(1,limit):x=base(op);x[what]=count;x[8]=2;x[14]=2;x[15]=2;rows.append(x)
 for _ in range(a.cases):
  x=base(rng.randrange(8));x[1]=rng.choice([0,1,MASK]);x[2]=rng.randrange(2);x[4]=rng.choice([0,1,MASK]);x[5]=rng.getrandbits(32);x[6]=rng.getrandbits(32);x[7]=rng.choice([0,1,0x10,MASK]);x[8]=rng.randrange(4);x[9]=rng.randrange(1024);x[13]=rng.randrange(4);x[14]=rng.randrange(4);x[15]=rng.randrange(4);x[16]=rng.choice([0,0x1210000]);
  if x[7]==0x10 and x[13]in[1,2]:x[12]=30 # Declared host failure stops the source's unbounded counter-reset loop.
  rows.append(x)
 expected=[]
 for row in rows:
  trace=[];frames=[];reads=writes=calls=sends=receives=0;heads=row[15];failed=False;first=True;restore=None
  for table in [0x501ab8,0x501a28]:
   for off,netsock,dp in [(0x54,0x4b9530,0x4b83b0),(0x58,0x4b95d0,0x4b8470),(0x60,0x4b96b0,0x4b85b0)]:put(table+off,netsock if table==0x501ab8 else dp)
  for at,value in zip(CELLS,[row[1],0x501ab8 if row[2]else 0x501a28,row[3],row[4],row[6],0x12345678,0xfefefefe,0xfdfdfdfd,0x1200000 if heads else 0]):put(at,value)
  u.mem_write(0x2000000,bytes([0xa5])*0x8000)
  args=[]if row[0]in[0,3,6,7]else[0x1201000,row[5]]+([0x87654321]if row[0]in[2,5]else[])
  active=True
  try:r.call(ROOTS[row[0]],*args)
  except RuntimeError:
   if not failed:raise
  finally:active=False
  if restore:put(*restore)
  result=0 if failed or row[0]==7 else u.reg_read(UC_X86_REG_EAX)
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} '+''.join(str(get(at))+','for at in CELLS)+' '+''.join(str(get(at))+','for at in frames)+' '+''.join(trace))
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,row))for row in rows)+'\n',text=True,capture_output=True);assert p.returncode==0,p.stderr;lines=p.stdout.splitlines();assert len(lines)==len(expected),(len(lines),len(expected))
 bad=[{'input':rows[i],'actual':x[:10000],'expected':y[:10000]}for i,(x,y)in enumerate(zip(lines,expected))if x!=y]
 a.report.write_text(json.dumps({'source_sha256':SOURCE_SHA256,'bodies':['497790','4977A0','4977E0','4B9530','4B95D0','4B96B0','4B8EC0','4B8EF0','4B8E10'],'cases':len(rows),'differences':len(bad),'mismatches':bad[:3],'comparison':'currenttable dispatch,sourceordered reads/stores/calls,currentD0A540,sourceframe length,systemmessage recursive receive,retry2049/pumpmutations,listdrain,allocationfailure,read/store/callerrorprefix','required_foreign_bodies':list(map(hex,foreign_args)),'device_or_network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
