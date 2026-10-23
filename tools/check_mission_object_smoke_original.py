#!/usr/bin/env python3
"""Replay42A520/42A590 current reads, aliases, wrap and failure prefixes."""
import argparse,hashlib,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
from check_projectile_hits_original import Replay,HASH
A,O=0x822e80,0x7dde80
class FixtureFault(Exception):pass

def main():
 q=argparse.ArgumentParser();q.add_argument('game',type=Path);q.add_argument('probe',type=Path);q.add_argument('--cases',type=int,default=2048);q.add_argument('--report',type=Path);a=q.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rng=random.Random(0x42a520);rows=[];expected=[];faults=0;counts={}
 def put(at,n,w=4):r.u.mem_write(at,(n&((1<<(w*8))-1)).to_bytes(w,'little'))
 def read(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 for case in range(a.cases):
  mode=case%2;mutation=((case//2)*37+case//8)%1024 if case%7 else 0;seed=rng.getrandbits(32);r.u.mem_write(A,rng.randbytes(0x114));r.u.mem_write(O,rng.randbytes(0x114));put(0xbd0060,seed)
  owner=[0,O,A,0x100][(case//2)%4];put(A+0xf8,owner);put(O+0x6c,4 if case%5 else 0,1);put(A+0x6c,4 if case%7 else 0,1)
  edges=[0,1,98,99,100,0xffffffff,0x7fffffff,0x80000000,0x1ffff,0x20000,0x20001]
  for offset in (0x24,0x28,0x2c,0x40,0x44,0x7c):put(A+offset,rng.choice(edges))
  actor=bytes(r.u.mem_read(A,0x114));owner_image=bytes(r.u.mem_read(O,0x114));rows.append(f'{mode} {mutation} {seed} {actor.hex()} {owner_image.hex()}');trace=[];pending=[];velocity_read=False
  def parent():
   pc=r.u.reg_read(UC_X86_REG_EIP);return 0x42a520<=pc<=0x42a58b or 0x42a590<=pc<=0x42a5fc
  def access(u,kind,at,width,value,user):
   nonlocal velocity_read
   if not parent() or not any(b<=at and at+width<=b+0x114 for b in (A,O)):return
   if kind==unicorn.UC_MEM_READ:
    trace.append(f'R{at},{width},{read(at,width)};')
    if not velocity_read and mutation&32 and at==A+0x40:velocity_read=True;pending.append(lambda:put(A+0x40,0x12345678))
   else:
    trace.append(f'W{at},{width},{value&((1<<(width*8))-1)};')
    if mutation&64 and at==A+0x24:
     pending.extend([lambda:put(A+0x28,777),lambda:put(A+0x2c,888),lambda:put(A+0x7c,12345),lambda:put(A+0x44,255)])
    if mutation&128 and at==A+0x7c:pending.append(lambda:put(A+0x7c,0x30000))
    if mutation&256 and at==A+0x3b:pending.extend([lambda:put(A+0x7c,0x34567),lambda:put(A+0x40,19),lambda:put(A+0x44,29)])
    if mutation&16 and at==(A+0x24 if mode else A+0x7c):pending.append(lambda:(_ for _ in ()).throw(FixtureFault()))
  def argv(n):return list(r.read(r.u.reg_read(UC_X86_REG_ESP)+4,n))
  def record(site,args):trace.append('C'+str(site)+''.join(','+str(n) for n in args)+';');counts[hex(site)]=counts.get(hex(site),0)+1
  def emit():
   record(0x42a440,argv(3))
   if mutation&1:put(A+0x7c,99)
   if mutation&2:put(A+0xf8,O);put(A+0x24,111)
   if mutation&512:raise FixtureFault()
  def remove():
   record(0x45da30,argv(1))
   if mutation&4:put(A+0xfc,123)
  def code(u,at,size,user):
   while pending:pending.pop(0)()
   if at==0x477860:
    record(at,[])
    if mutation&8:raise FixtureFault()
    if mutation&4:put(A+0xfc,456)
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)];r.hook(0x42a440,emit);r.hook(0x45da30,remove);failed=False
  try:
   r.call(0x42a590 if mode else 0x42a520,A)
   while pending:pending.pop(0)()
  except (FixtureFault,unicorn.UcError):failed=True;faults+=1
  for h in hooks:r.u.hook_del(h)
  r.clear_hooks();digest=1469598103934665603
  for b in (A,O):
   for n in r.u.mem_read(b,0x114):digest=((digest^n)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {read(0xbd0060)} {digest} '+''.join(trace))
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',capture_output=True,text=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y} for i,(x,y) in enumerate(zip(expected,got)) if x!=y];report={'source_sha256':HASH,'cases':a.cases,'differences':len(diffs),'fault_prefixes':faults,'calls':counts,'real_bodies':['42A520','42A590','477860'],'foreign_fixtures':['42A440 emitter','45DA30 remove'],'comparison':'Full sourceactor/owner images, currentbyte/wordreads/stores/calls, RNG, selfowner aliases, signedcounter/velocity and unsignedgrowthcaps','details':diffs[:1]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='details'}));
 if diffs:print(json.dumps(diffs[0]));return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
