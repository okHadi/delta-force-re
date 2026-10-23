#!/usr/bin/env python3
"""Compare actual45E0F0 current water, x87 predicate, terrain and call order."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256,STACK

def run(a):
 r=Rig(a.game);u=r.u;row=None;trace=[];active=False;reads=waters=calls=0;failed=False;grid=0
 def put(at,n,value):u.mem_write(at,(value&((1<<(n*8))-1)).to_bytes(n,'little'))
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def source(u,kind,at,n,value,_):
  nonlocal reads,waters,failed
  if not active or at not in [0xbd5248,0x1200024,0x1200028,0x120002c,0x4ccb2c,0xbd52ac,grid,0xbd533b]:return
  trace.append(f'R{at},{n};');reads+=1;mutation=row[8]
  if mutation&128 and reads==3:failed=True;u.emu_stop();return
  if at==0xbd5248:
   waters+=1
   if waters==1 and mutation&1:put(0x1200024,4,get(0x1200024)^0xabcdef01)
   if waters==2 and mutation&2:put(0xbd5248,4,get(0xbd5248)^0xffffffff)
  if at==0x1200028 and mutation&4:put(STACK+8,4,get(STACK+8)^0xffffffff)
  if at==0xbd52ac and mutation&8:put(STACK+8,4,get(STACK+8)^0xffffffff)
 u.hook_add(unicorn.UC_HOOK_MEM_READ,source)
 def foreign(site):
  nonlocal calls,failed
  sp=u.reg_read(UC_X86_REG_ESP);values=[get(sp+4+i*4)for i in range(4)];trace.append('C'+str(site)+''.join(','+str(n)for n in values)+';');calls+=1;mutation=row[8]
  if site==0x45dc70:
   if mutation&16:put(0xbd5248,4,get(0xbd5248)^0xffffffff)
   if mutation&32:put(STACK+8,4,get(STACK+8)^0xffffffff)
   if mutation&64:put(0x1200024,4,get(0x1200024)^0x13579bdf);put(0x1200028,4,get(0x1200028)^0x2468ace0)
  if mutation&256 and calls==1:failed=True;u.emu_stop();return
  r.finish()
 r.stubs[0x43e7d0]=lambda:foreign(0x43e7d0);r.stubs[0x45dc70]=lambda:foreign(0x45dc70)
 actual_bias=0xc0e999999999999a;rng=random.Random(0x45e0f0);rows=[]
 for water in [0,1,0x7fffffff,0x80000000,0xffffffff]:
  for z in [0,52428,52429,0x7fffffff,0x80000000,0xffffffff]:
   for step in [0,1,0xffffffff]:rows.append([rng.getrandbits(32),step,rng.getrandbits(32),rng.getrandbits(32),z,water,rng.randrange(8),actual_bias,rng.randrange(128)])
 for terrain in [0,1,2,3,4,5,6,7,255,256,257]:
  for mutation in [0,1,2,4,8,16,32,64,128,256,511]:rows.append([0,rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),0,rng.choice([0,1]),terrain,actual_bias,mutation])
 for _ in range(a.cases):rows.append([rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.randrange(9),rng.choice([actual_bias,0,0x7ff0000000000000,0xfff0000000000000,0x7ff8000000000000]),rng.randrange(512)])
 expected=[]
 for row in rows:
  first,step,x,y,z,water,terrain,bias,mutation=row;trace=[];reads=waters=calls=0;failed=False;actual_x=x^0xabcdef01 if mutation&1 else x;grid=0x1220000+((actual_x&0x3ff0000)>>16)+((y&0x3ff0000)>>6)
  for at,n,value in [(0xbd5248,4,water),(0x1200024,4,x),(0x1200028,4,y),(0x120002c,4,z),(0x4ccb2c,8,bias),(0xbd52ac,4,0x1220000),(grid,1,7),(0xbd533b,1,terrain)]:put(at,n,value)
  active=True
  try:r.call(0x45e0f0,first,step,0x1200000)
  except RuntimeError:
   if not failed:raise
  finally:active=False
  expected.append(f'{int(failed)} {get(0xbd5248)} {get(STACK+8)} '+''.join(trace))
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,row))for row in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,p.stderr;lines=p.stdout.splitlines();assert len(lines)==len(rows);bad=[{'input':rows[i],'actual':x,'expected':y}for i,(x,y)in enumerate(zip(lines,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SOURCE_SHA256,'body':'45E0F0','cases':len(rows),'differences':len(bad),'mismatches':bad[:2],'comparison':'initialwaterbeforeXYZ,actualdoublebias/x87predicate,integeredges,NaN/infinities,mutablecurrentwater/args,cachedvisualwater/currentaudioWater,dryclassifierbyte,orderedcalls/errorprefix','foreign_bodies':['45DC70 required wet visual','43E7D0 real positional boundary'],'audio_device_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
