#!/usr/bin/env python3
"""Compare object destruction calls, mutable local records and RNG."""
import argparse,json,random,struct,subprocess
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EBX
from check_projectiles_original import Rig,SOURCE_SHA256
A=0x1200000;D=0x1210000;M=0x1210800;RNG=0xbd0060

def run(args):
 rig=Rig(args.game);u=rig.u;trace=[];lookups=spawns=calls=0;v=None;failed=False
 def put(at,n,width=4):u.mem_write(at,(n&((1<<(width*8))-1)).to_bytes(width,'little'))
 def read(at,width=4):return int.from_bytes(u.mem_read(at,width),'little')
 def lookup():
  nonlocal lookups
  sp=u.reg_read(UC_X86_REG_ESP);trace.append('L'+str(read(sp+4))+';');lookups+=1
  if v[4]&1:put(A+0x24,read(A+0x24)+0x100)
  if v[4]&2 and lookups==2:put(0xc23810,0x1212000)
  rig.finish(lookups)
 rig.stubs[0x4828c0]=lookup
 def foreign(site):
  nonlocal spawns,calls,failed
  sp=u.reg_read(UC_X86_REG_ESP);values=[0]*6;data=b'';ptr=0
  if site in [0x43e7d0,0x447610]:values[:4]=[read(sp+4+i*4) for i in range(4)]
  elif site==0x42a440:values[:3]=[read(sp+4+i*4) for i in range(3)]
  elif site==0x44fcb0:values=[read(sp+4+i*4) for i in range(6)]
  elif site==0x460a30:
   values[:4]=[read(sp+4),0,read(sp+12),read(sp+16)];ptr=read(sp+8);data=bytes(u.mem_read(ptr,24))
  elif site==0x45d970:ptr=read(sp+4);data=bytes(u.mem_read(ptr,0x114))
  trace.append('C'+str(site)+''.join(','+str(n) for n in values)+':'+data.hex()+';')
  if v[4]&4:
   if calls==0:put(A+0x28,read(A+0x28)+0x200)
   calls+=1
  if site==0x45d970:
   if v[4]&8:spawns+=1;put(ptr+0xe0,spawns,1)
   if v[4]&32:put(RNG,read(RNG)^0x99999999)
  if site==0x460a30 and v[4]&8:put(ptr,read(ptr,1)^0x80,1);put(ptr+10,read(ptr+10,1)^0x40,1)
  if site==0x43e7d0 and v[4]&64:put(A+0x2c,read(A+0x2c)+0x300)
  if site==0x45d970 and v[4]&128:failed=True;u.emu_stop();return
  rig.finish()
 for site in [0x43e7d0,0x447610,0x42a440,0x44fcb0,0x460a30,0x45d970]:rig.stubs[site]=lambda site=site:foreign(site)
 # Read hooks observe current definition/model/item reads at their actual instructions.
 import unicorn
 active=False
 def memory(u,kind,at,size,value,_):
  if not active:return
  if at==A+0x24 and v[4]&256:put(A+0x28,read(A+0x28)+0x100)
  if at==D+0x20:trace.append('D1,32;')
  elif at==M+0x64:trace.append('M0,100;')
  elif 0x1211000<=at<0x1214000:
   off=next((n for n in [0x6c,0x84,0x90] if (at-0x1211000-n)%0x13c==0 or (at-0x1212000-n)%0x13c==0),None)
   if off is not None:trace.append('I'+str(at-off)+','+str(off)+';')
 u.hook_add(unicorn.UC_HOOK_MEM_READ,memory)
 rng=random.Random(0x453270);rows=[]
 ids=[0x3e9,0x3ea,0x40f,0x410,0x411,0x412,0x414,0x416,0x418,0x419,0xbc0,0xfaf,0xfb3,0,0xffffffff]
 for body in range(3):
  for id in (ids if body==0 else [0]):
   for mut in [0,1,2,4,8,16,32,64,128,255,256,511]:rows.append([body,id,rng.getrandbits(32),rng.getrandbits(32),mut])
 for _ in range(args.cases):rows.append([rng.randrange(3),rng.choice(ids),rng.getrandbits(32),rng.getrandbits(32),rng.randrange(512)])
 expected=[]
 for actor in [0,1,0x7cca80,0x7dde80,0x867e80,0xffffffff]+[rng.getrandbits(32) for _ in range(100)]:rows.append([3,rng.getrandbits(32),actor,0,0])
 for v in rows:
  if v[0]==3:
   packet=[]
   def broadcast():
    sp=u.reg_read(UC_X86_REG_ESP);assert read(sp+4)==0x39 and read(sp+12)==4;packet.append(bytes(u.mem_read(read(sp+8),4)).hex());rig.finish()
   rig.stubs[0x442630]=broadcast;rig.call(0x447980,v[2],v[1]);expected.append('B'+packet[0]);continue
  u.mem_write(A,bytes(0x114));put(A,1);put(A+4,D);put(A+0x10,M);put(A+0x24,0x123400);put(A+0x28,0x234500);put(A+0x2c,0x345600);put(A+0x30,v[2]);put(A+0x34,v[3]);put(A+0x38,v[1]);put(A+0x72,0xffff if v[4]&16 else 0,2);put(D+0x20,v[1]);put(M+0x64,0x4000);put(RNG,v[3]);put(0xc23810,0x1211000)
  for base in [0x1211000,0x1212000]:
   for index in [1,2]:
    for off in [0x6c,0x84,0x90]:put(base+index*0x13c+off,(base+index*0x13c)^off)
  trace=[];lookups=spawns=calls=0;failed=False;active=True;u.reg_write(UC_X86_REG_EBX,v[2])
  try:rig.call([0x453270,0x44ed40,0x44f270][v[0]],A)
  except RuntimeError:
   if not failed:raise
  finally:active=False
  expected.append(f'{int(failed)} {read(RNG)} {read(A+0x24)} {read(A+0x28)} {read(A+0x2c)} '+''.join(trace))
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True);assert p.returncode==0,p.stderr
 actual=p.stdout.splitlines();assert len(actual)==len(expected);bad=[]
 for i,(a,b) in enumerate(zip(actual,expected)):
  if a!=b:
   bad.append({'case':i,'input':rows[i],'actual':a,'expected':b})
   if len(bad)<3:args.report.with_name(args.report.stem+f'.failure-{i}.json').write_text(json.dumps(bad[-1],indent=2))
 report={'source_sha256':SOURCE_SHA256,'bodies':['453270','44ED40','44F270','447980'],'cases':len(rows),'differences':len(bad),'mismatches':bad[:2],'comparison':'ordered calls,whole mutable0x114byte debris records,24byte effect pose,RNG,current source positions,item reads,unknown IDs with independent EBX,callback mutation and partial failures','audio_output':'none'};args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
