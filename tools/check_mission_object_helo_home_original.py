#!/usr/bin/env python3
"""Compare4509F0 and450100 with the supplied original instructions."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256,STACK
A=0x7dde80;P=0x794680;F=0x1300000;S=STACK-44

def run(args):
 r=Rig(args.game);rng=random.Random(0x4509f0);active=False;v=None;trace=[];pending=[];calls=grounds=trigs=0;failed=False
 def put(at,n,w=4):r.u.mem_write(at,(n&((1<<(8*w))-1)).to_bytes(w,'little'))
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def norm(at):return F+at-S if v[0] and S<=at<S+0x40 else at
 def flush(*_):
  while pending:pending.pop(0)()
 def memory(u,kind,at,width,value,_):
  if not active:return
  ranges=[(A,0x114),(P,0xb4)]+([(S,0x1c),(S+0x30,16)] if v[0] else [])
  if not any(start<=at and at+width<=start+size for start,size in ranges):return
  n=get(at,width) if kind==unicorn.UC_MEM_READ else value&((1<<(width*8))-1);trace.append(f'{"R" if kind==unicorn.UC_MEM_READ else "W"}{norm(at)},{width},{n};')
  if kind==unicorn.UC_MEM_WRITE and at==A+0xb0:
   if v[5]&8 and n==0:pending.append(lambda:put(A+0xd4,12))
   if v[5]&16:pending.append(lambda:put(A+0xb0,0x12345678))
 r.u.hook_add(unicorn.UC_HOOK_CODE,flush);r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def stub(site,count):
  def body():
   nonlocal calls,grounds,trigs,failed
   sp=r.u.reg_read(UC_X86_REG_ESP);raw=[get(sp+4+i*4) for i in range(count)];values=[norm(n) for n in raw];trace.append('C'+str(site)+''.join(','+str(n) for n in values)+';');calls+=1
   if calls==v[6]:failed=True;r.u.emu_stop();return
   result=0
   if site==0x477770:
    if v[5]&1:put(A+0x24,11);put(A+0x28,22);put(P+0x54,33)
    result=0x11223344
   elif site==0x477670:result=v[1]
   elif site==0x477820:
    put(raw[1],v[2]);put(raw[2],v[3]);trigs+=1
    if v[5]&4:
     if trigs==1:put(P+0x2c,63)
     else:put(A+0x7c,get(A+0x7c)^0x80000000)
   elif site==0x47df40:
    grounds+=1
    if v[5]&2:put(A+0x24,get(A+0x24)+0x10000);put(A+0x28,get(A+0x28)-0x20000)
    result=0 if v[5]&64 else raw[0]^raw[1]^((grounds*0x12345678)&0xffffffff)
   elif site==0x486fd0:
    if v[5]&32:put(S+0x38,0x20000);put(S+0x3c,31)
    result=0x12345
   elif site==0x450100:put(A+0x2c,0x44556677)
   r.finish(result)
  return body
 for site,count in [(0x477770,2),(0x477670,3),(0x477820,3),(0x47df40,2),(0x486fd0,3),(0x450100,4)]:r.stubs[site]=stub(site,count)
 rows=[];edges=[0,1,0xffffffff,0x80000000,0x7fffffff,0x7ff,0x800,0xfffff800,0xfffff801]
 for body in [0,1]:
  for distance in [0x40000,0x40001,0x1e0000,0x1e0001,0x500000,0x500001,0x80000000]:
   for mutation in [0,1,2,4,8,16,32,63]:
    image=bytearray(rng.randbytes(0x114))
    for at in [0x7c,0xac,0xd4,0xdc,0xe0,0xe4,0xec,0xf0]:struct.pack_into('<I',image,at,rng.choice(edges))
    struct.pack_into('<I',image,0xe8,rng.choice([0,62,63,0xffffffff]));rows.append([body,distance,rng.choice(edges),rng.choice(edges),rng.choice([0,1,2,31,255]),mutation,0,image.hex()])
 for stage in [0,6,7,11,12]:
  for vertical in [0xfffff7ff,0xfffff800,0xffffffff,0,1,0x7ff,0x800]:
   for z in [0,0x400000]:
    for target in [0,0x100000]:
     for band in [0,0x10000]:
      for mutation in [64,80]:
       image=bytearray(0x114)
       for at,n in [(0x2c,z),(0xac,vertical),(0xd4,stage),(0xec,target),(0xf0,band)]:struct.pack_into('<I',image,at,n)
       rows.append([1,0,0,0x400000,0,mutation,0,image.hex()])
 for _ in range(args.cases):rows.append([rng.randrange(2),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.randrange(64),rng.randrange(13),rng.randbytes(0x114).hex()])
 expected=[]
 for v in rows:
  image=bytearray.fromhex(v[7]);struct.pack_into('<I',image,0xc,P);r.u.mem_write(A,bytes(image));r.u.mem_write(P,bytes(0xb4));put(P+0x54,get(A+0xe0));put(P+0x58,get(A+0xe4));put(P+0x2c,get(A+0xe8));r.u.mem_write(S,b'\xa5'*0x40);trace=[];pending=[];calls=grounds=trigs=0;failed=False;active=True
  entry=0x450100 if v[0] else 0x4509f0;saved=r.stubs.pop(entry,None)
  try:r.call(entry,*([A,get(A+0xec),get(A+0xf0),v[4]] if v[0] else [A]));flush()
  except RuntimeError:
   if not failed:raise
  finally:
   active=False
   if saved:r.stubs[entry]=saved
  digest=1469598103934665603
  ranges=[(A,0x114),(P,0xb4)]+([(S,0x1c),(S+0x30,16)] if v[0] else [])
  for at,size in ranges:
   for byte in bytes(r.u.mem_read(at,size)):digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {r.u.reg_read(UC_X86_REG_EAX) if not v[0] and not failed else 0} {digest} '+''.join(trace))
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True);assert not p.returncode,p.stderr;actual=p.stdout.splitlines();assert len(actual)==len(expected)
 diff=[{'case':i,'input':rows[i],'actual':a,'expected':b} for i,(a,b) in enumerate(zip(actual,expected)) if a!=b];report={'source_sha256':SOURCE_SHA256,'bodies':['4509F0','450100'],'cases':len(rows),'differences':len(diff),'mismatches':diff[:3],'coverage':'orderedcurrentactor/person/frame reads andstores; cacheddeltas; signedabsINTMIN; signed64multiply; sourcebandargmutation; RMWforce; currentD4; latepositions; callbackerrorprefix; wrappedheightcomparison','foreign_calls':'explicitmath/terrain/486FD0/450100 fixture contracts; canonical realmath/terrainjoin native separate'};args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(diff)}));assert not diff
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=600);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
