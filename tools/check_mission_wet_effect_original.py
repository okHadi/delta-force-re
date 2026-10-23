#!/usr/bin/env python3
"""Compare45DC70 scratch writes and current source reads with the supplied PE."""
import argparse,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
from check_projectiles_original import Rig,SOURCE_SHA256,STACK

FRAME=STACK-0x11c
DEFINITION=0x1210000
MASK=0xffffffff
def run(a):
 r=Rig(a.game);u=r.u;r.stubs.pop(0x45dc70,None)
 row=None;trace=[];reads=writes=calls=randoms=0;failed=False;active=False;restore=None
 def put(at,value):u.mem_write(at,(value&MASK).to_bytes(4,'little'))
 def get(at):return int.from_bytes(u.mem_read(at,4),'little')
 def fail():
  nonlocal failed
  failed=True;u.emu_stop()
 def memory(uc,kind,at,width,value,_):
  nonlocal reads,writes,restore
  if not active or not 0x45dc70<=u.reg_read(UC_X86_REG_EIP)<=0x45dd33:return
  if kind==unicorn.UC_MEM_WRITE:
   if not FRAME<=at<FRAME+0x114:return
   assert width==4
   trace.append(f'W{at},{value&MASK};');writes+=1
   if writes==row[12]:restore=(at,get(at));fail()
  else:
   valid=[(0x8e44b0+row[0]*4)&MASK,0xc23810,DEFINITION+0x6c,DEFINITION+0x84,DEFINITION+0x90]+[FRAME+0x120+4*i for i in range(4)]
   if at not in valid:return
   assert width==4
   trace.append(f'R{at};');reads+=1
   if reads==row[11]:fail();return
   if row[10]&8 and at==DEFINITION+0x84:put(DEFINITION+0x90,get(DEFINITION+0x90)^0x12345678)
   if row[10]&16 and at==FRAME+0x124:put(FRAME+0x128,get(FRAME+0x128)^0x87654321)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def foreign(site):
  nonlocal calls,randoms
  sp=u.reg_read(UC_X86_REG_ESP)
  args=[get(sp+4+i*4)for i in range(3)]if site==0x49a620 else [get(sp+4),get(sp+8),0]if site==0x45d970 else [0,0,0]
  trace.append('C'+str(site)+''.join(','+str(n)for n in args)+';');calls+=1
  if calls==row[13]:fail();return
  if site==0x49a620:
   u.mem_write(args[0],bytes([args[1]&255])*args[2])
   if row[10]&1:
    for at,xor in [(FRAME+0x124,0xabcdef01),(FRAME+0x128,0x13579bdf),(FRAME+0x12c,0x2468ace0),(DEFINITION+0x6c,0xfedcba98)]:put(at,get(at)^xor)
    put(0xc23810,0x1230000);put((0x8e44b0+row[0]*4)&MASK,17)
   r.finish(args[0])
  elif site==0x477860:
   randoms+=1
   if randoms==1 and row[10]&2:
    put(DEFINITION+0x84,get(DEFINITION+0x84)^0x55555555);put(DEFINITION+0x90,get(DEFINITION+0x90)^0xaaaaaaaa);put(FRAME+4,0x87654321);put(0xc23810,0x1234000)
   if randoms==2 and row[10]&4:
    put(DEFINITION+0x84,get(DEFINITION+0x84)^MASK);put(DEFINITION+0x90,get(DEFINITION+0x90)^MASK);put(FRAME+0x30,0x13579bdf)
   r.finish(row[7+randoms])
  else:
   if row[10]&32:put(FRAME+0x24,get(FRAME+0x24)^MASK)
   r.finish(0x822e80)
 for site in [0x49a620,0x477860,0x45d970]:r.stubs[site]=lambda site=site:foreign(site)
 rng=random.Random(0x45dc70);rows=[]
 for index in [0,2,0xffffffff,0x80000000]:
  for definition_index in [0,1,0xffffffff,0x80000000]:
   for mutation in [0,1,2,4,8,16,32,63]:rows.append([index,definition_index]+[rng.getrandbits(32)for _ in range(8)]+[mutation,0,0,0])
 for what,limit in [(11,10),(12,13),(13,5)]:
  for count in range(1,limit):
   item=[2,1]+[rng.getrandbits(32)for _ in range(8)]+[63,0,0,0];item[what]=count;rows.append(item)
 for _ in range(a.cases):rows.append([rng.choice([0,2,0xffffffff,0x80000000]),rng.getrandbits(32)]+[rng.getrandbits(32)for _ in range(8)]+[rng.randrange(64),rng.choice([0,0,0,rng.randrange(1,10)]),rng.choice([0,0,0,rng.randrange(1,13)]),rng.choice([0,0,0,rng.randrange(1,5)])])
 expected=[]
 for row in rows:
  trace=[];reads=writes=calls=randoms=0;failed=False;restore=None
  u.mem_write(FRAME,bytes([0xa5])*0x114)
  put((0x8e44b0+row[0]*4)&MASK,row[1]);put(0xc23810,DEFINITION-row[1]*0x13c)
  for at,value in [(DEFINITION+0x6c,row[5]),(DEFINITION+0x84,row[6]),(DEFINITION+0x90,row[7])]:put(at,value)
  active=True
  try:r.call(0x45dc70,row[0],row[2],row[3],row[4])
  except RuntimeError:
   if not failed:raise
  finally:active=False
  if restore:put(*restore)
  expected.append(f'{int(failed)} {reads} {writes} {calls} '+''.join(str(get(FRAME+i))+','for i in range(0,0x114,4))+' '+''.join(trace))
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,row))for row in rows)+'\n',text=True,capture_output=True);assert p.returncode==0,p.stderr
 lines=p.stdout.splitlines();assert len(lines)==len(expected),(len(lines),len(expected))
 bad=[{'input':rows[i],'actual':x,'expected':y}for i,(x,y)in enumerate(zip(lines,expected))if x!=y]
 a.report.write_text(json.dumps({'source_sha256':SOURCE_SHA256,'body':'45DC70','cases':len(rows),'differences':len(bad),'mismatches':bad[:3],'comparison':'full114scratch,currentread/store/calltrace,wrappedindex/definition/yaw/scale,cacheddefinition,postclearargs,latecallbacks,randomscratchmutation,read/store/callfailureprefix','foreign_boundaries':['49A620 clear','477860 random','45D970 canonical insert']},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=512);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
