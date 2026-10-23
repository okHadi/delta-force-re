#!/usr/bin/env python3
"""Compare451E70 and44FBA0 with the hash-bound original instructions."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256,STACK
A=0x7dde80;G=0x7cca80;D=0x1100000;W=0x1110000;F=0x1300000;S=STACK-0x124

def run(args):
 r=Rig(args.game);rng=random.Random(0x451e70);active=False;trace=[];pending=[];v=None;calls=clears=0;failed=False;seed=0
 def put(at,n,width=4):r.u.mem_write(at,(n&((1<<(8*width))-1)).to_bytes(width,'little'))
 def get(at,width=4):return int.from_bytes(r.u.mem_read(at,width),'little')
 def frame_at():return F if v[0] else S
 def norm(at):return F+at-S if not v[0] and S<=at<S+0x114 else at
 def watched(at,width):return any(lo<=at and at+width<=lo+n for lo,n in [(A,0x114),(G,256*0x114),(D,4*0x13c),(W,0x100),(frame_at(),0x114)]) or at in [0xc23810,0xc23814,0xc23820,0x67dbc9]
 def flush(*_):
  while pending:pending.pop(0)()
 def memory(u,kind,at,width,value,_):
  if not active or not watched(at,width):return
  if kind==unicorn.UC_MEM_READ:
   n=get(at,width);trace.append(f'R{norm(at)},{width},{n};')
   if v[1]&16 and at==G+0x28:put(A+0x28,0x12345678)
   if v[1]&32 and W<=at<W+0x100:put(G+0x6d,0x40,1)
   if v[1]&64 and at==D+0x20:put(0xc23814,2)
  else:
   n=value&((1<<(8*width))-1);trace.append(f'W{norm(at)},{width},{n};')
   if v[1]&128 and at==A+0xe0:pending.append(lambda:put(A+0xfc,77))
 r.u.hook_add(unicorn.UC_HOOK_CODE,flush);r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def stub(site,nargs):
  def body():
   nonlocal calls,clears,failed,seed
   sp=r.u.reg_read(UC_X86_REG_ESP);vals=[get(sp+4+i*4) for i in range(nargs)];trace.append('C'+str(site)+''.join(','+str(norm(n)) for n in vals)+';');calls+=1
   if calls==v[2]:failed=True;r.u.emu_stop();return
   result=0
   if site==0x44fba0:
    if v[1]&1:put(0xc23810,D+0x13c)
    result=v[3]
   elif site==0x477670:result=v[4]
   elif site==0x477860:seed=(seed*1664525+1013904223)&0xffffffff;result=seed
   elif site==0x43e7d0:
    if v[1]&2:
     for off,n in [(0x24,11),(0x28,22),(0x2c,33)]:put(A+off,n)
   elif site==0x49a620:
    r.u.mem_write(vals[0],bytes([vals[1]&255])*vals[2]);clears+=1;result=vals[0]
    if v[1]&4 and clears==2:put(0xc23810,D+0x13c)
   elif site==0x45d970:
    if v[1]&8:put(A+0x24,44);put(D+2*0x13c+0x84,0xabcdef01)
   r.finish(result)
  return body
 for site,n in [(0x44fba0,1),(0x477670,3),(0x477860,0),(0x43e7d0,4),(0x49a620,3),(0x45d970,2)]:r.stubs[site]=stub(site,n)
 def image(active,side):
  out=bytearray(rng.randbytes(0x114))
  for off,n in [(0,active),(4,D+0x13c),(0x6c,0),(0x70,side),(0x80,rng.randrange(4))]:struct.pack_into('<I',out,off,n)
  return out
 rows=[]
 for mode in [0,1]:
  for cooldown in [0,1,10,0xffffffff]:
   for mutation in [0,1,2,4,8,16,32,64,128,255]:
    for value in ([0,0x100000,0x100001,0x7fffffff] if not mode else [0,0x10000,0x7fff0000,0x80000000,0xffff0000]):
     a=image(1,1);struct.pack_into('<I',a,0xe0,cooldown);rows.append([mode,mutation,0,value,value,rng.getrandbits(32),3,G,0,rng.getrandbits(32),a.hex(),image(1,2).hex(),image(1,3).hex()])
 for _ in range(args.cases):
  mode=rng.randrange(2);a=image(1,rng.choice([0,1,2]));struct.pack_into('<I',a,0xe0,rng.choice([0,0,1,0xffffffff]));b=image(rng.randrange(2),rng.randrange(3));c=image(rng.randrange(2),rng.randrange(3));b[0x6c]=rng.choice([0,4]);b[0x6d]=rng.choice([0,0x40]);rows.append([mode,rng.randrange(256),rng.randrange(15),rng.choice([0,0x100001,0x7fffffff]),rng.getrandbits(32),rng.getrandbits(32),rng.randrange(5),rng.choice([G,G+255*0x114,0]),rng.choice([0,8]),rng.getrandbits(32),a.hex(),b.hex(),c.hex()])
 expected=[]
 for v in rows:
  seed=v[5];calls=clears=0;failed=False;trace=[];pending=[]
  r.u.mem_write(A,bytes.fromhex(v[10]));r.u.mem_write(G,bytes(256*0x114));r.u.mem_write(G,bytes.fromhex(v[11]));r.u.mem_write(G+255*0x114,bytes.fromhex(v[12]));r.u.mem_write(D,bytes(4*0x13c));r.u.mem_write(W,bytes(0x100));r.u.mem_write(frame_at(),b'\xa5'*0x114)
  for at,n in [(0xc23810,D),(0xc23814,v[6]),(0xc23820,v[7]),(0x67dbc9,v[8])]:put(at,n)
  for i in range(4):
   for off,n in [(0x20,0xfd3 if i in [1,2] else 0),(0x6c,0x11110000+i),(0x70,0x22220000+i),(0x78,W),(0x84,0x33330000+i),(0x90,0x44440000+i)]:put(D+i*0x13c+off,n)
   put(W+i*0x1c+0x10,0x10000+i*0x10000)
  # A stub is needed only for the spawner's helper call; signal mode executes real44FBA0.
  saved=r.stubs.pop(0x44fba0) if v[0] else None;active=True;result=0
  try:r.call(0x44fba0 if v[0] else 0x451e70,A,v[9]);flush();result=r.u.reg_read(UC_X86_REG_EAX) if v[0] else 0
  except RuntimeError:
   if not failed:raise
  finally:
   active=False
   if saved:r.stubs[0x44fba0]=saved
  digest=1469598103934665603
  raw=b''.join(bytes(r.u.mem_read(at,n)) for at,n in [(A,0x114),(G,256*0x114),(D,4*0x13c),(W,0x100),(frame_at(),0x114)])+b''.join(struct.pack('<I',get(at)) for at in sorted([0xc23810,0xc23814,0xc23820,0x67dbc9]))+struct.pack('<I',seed)
  for n in raw:digest=((digest^n)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {digest} '+''.join(trace))
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True);assert not p.returncode,p.stderr;actual=p.stdout.splitlines();assert len(actual)==len(expected)
 diffs=[{'case':i,'input':rows[i],'actual':a,'expected':b} for i,(a,b) in enumerate(zip(actual,expected)) if a!=b]
 report={'source_sha256':SOURCE_SHA256,'bodies':['451E70','44FBA0'],'cases':len(rows),'differences':len(diffs),'mismatches':diffs[:3],'coverage':'full physical256 pool, actor, definitions, frame bytes; ordered reads/stores/calls; cooldown, duplicateID last match, count/base/frame/posture/position mutations, signed products and call failures','source':'original PE instructions; retained REA body verified against samehash; current warm REA has no analyzed procedure at either entry','foreign_calls':'explicit477670/math, shared-random, sound, insertion and memset fixture contracts; native canonical allocator/sharedRNG proof separate'};args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(diffs)}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=240);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
