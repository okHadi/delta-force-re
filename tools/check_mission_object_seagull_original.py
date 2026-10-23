#!/usr/bin/env python3
"""Replay seagull callbacks and their physical flock scans."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from check_projectile_hits_original import Replay,HASH,STACK
A,D,F=0x7dde80,0x1300000,0x1302000
REGIONS=[(A,4*0x114),(D,0x278),(F,28),(0x67f528,4),(0xbd0060,4)]
class FixtureFault(Exception):pass

def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=2048);p.add_argument('--report',type=Path);a=p.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);r.call(0x477510);rng=random.Random(0x451fd0);rows=[];expected=[];faults=0;counts={};frame=STACK-0x2c
 def read(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def put(at,n,w=4):r.u.mem_write(at,(n&((1<<(w*8))-1)).to_bytes(w,'little'))
 edges=[0,1,0xffffffff,0x80000000,0x7fffffff,0x30000,0xfffff000,0x30000000,0x18000000]
 r.u.mem_write(A,bytes(1024*0x114))
 for case in range(a.cases):
  mode=0 if case%61==0 else case%4 if case%13==0 else case%2;slot=case%4;actor=A+slot*0x114;frame=STACK-0x2c if not mode else F;mutation=rng.randrange(8192);regions=[(frame if b==F else b,n)for b,n in REGIONS]
  for b,n in regions:r.u.mem_write(b,rng.randbytes(n))
  for i in range(4):
   at=A+i*0x114;put(at,1);put(at+4,D);put(at+0x72,0x1234,2);put(at+0x74,1,2);put(at+0x6c,0,2)
   for offset in [0x24,0x28,0x2c,0x30,0x3c,0x40,0x7c,0x88,0x8c,0xe0,0xe4,0xe8,0xec,0xf0,0xf4]:put(at+offset,edges[(case+i+offset)%len(edges)] if case%3==0 else rng.getrandbits(32))
  put(D+0x20,0x7367756c);put(D+0x13c+0x20,0x7367756c if case%7 else 0);put(0x67f528,int(case%17!=0));put(actor+0x6c,4 if case%5==0 else 0,2);put(actor+0x74,0 if case%11==0 else 1,2);put(0xbd0060,rng.getrandbits(32))
  if case%31==0:put(actor+4,D+0x13c);put(D+0x13c+0x20,0)
  if case%61==0:
   for i in range(4):put(A+i*0x114,0)
   put(actor+0x6c,0,2);put(actor+0x74,1,2);put(0x67f528,1)
  data=b''.join(bytes(r.u.mem_read(b,n))for b,n in regions);rows.append(f'{mode} {slot} {mutation} {data.hex()}');trace=[];pending=[];calls=stores=0
  def norm(at):return F+at-frame if frame<=at<frame+28 else at
  def parent():
   pc=r.u.reg_read(UC_X86_REG_EIP);return 0x451fd0<=pc<=0x452300 or 0x452310<=pc<=0x4525c8 or 0x44fd30<=pc<=0x44fd75 or 0x44fd80<=pc<=0x44fdd4
  def access(u,kind,at,width,value,user):
   nonlocal stores
   if not parent() or not (A<=at and at+width<=A+1024*0x114 or any(b<=at and at+width<=b+n for b,n in regions)):return
   if kind==unicorn.UC_MEM_READ:
    value=read(at,width);trace.append(f'R{norm(at)},{width},{value};')
    if mutation&1 and at==actor+0x72:pending.append(lambda:put(actor+4,D+0x13c))
    if mutation&2 and at==actor+0x7c:pending.append(lambda:put(actor+0x88,0x80000000))
   else:
    value&=(1<<(width*8))-1;trace.append(f'W{norm(at)},{width},{value};');stores+=1
    if mutation&4 and at==actor+0xec:pending.append(lambda:put(A+0x24,read(A+0x24)+0x123456))
    if mutation&8 and at==actor+0x30:pending.append(lambda:put(actor+0x7c,0x10000))
    if mutation&16 and at==actor+0xe0:pending.append(lambda:put(actor+0xe8,1))
    if mutation&2048 and stores==((mutation>>8)%9+1):pending.append(lambda:(_ for _ in ()).throw(FixtureFault()))
  sizes={0x477860:0,0x43e7d0:4,0x477820:3,0x477670:3,0x477770:2,0x47df40:2}
  def code(u,at,size,user):
   nonlocal calls
   while pending:pending.pop(0)()
   if at not in sizes:return
   args=list(r.read(r.u.reg_read(UC_X86_REG_ESP)+4,sizes[at]))if sizes[at] else [];trace.append('C'+str(at)+''.join(','+str(norm(x))for x in args)+';');calls+=1;counts[hex(at)]=counts.get(hex(at),0)+1
   if mutation&4096 and calls==((mutation>>8)%5+1):raise FixtureFault()
   if at==0x477860 and mutation&32:put(actor+0x24,123);put(actor+0x28,456);put(actor+0x2c,789)
   if at==0x43e7d0 and mutation&64:put(actor+0xf0,0x80000000);put(actor+0x3c,0x7fffffff);put(actor+0x6d,0xab,1)
   if at==0x477820 and mutation&128:put(actor+0x7c,0x80000000);put(actor+0x6d,0xf0,1)
   if at==0x477670 and mutation&256:put(A+0x30,0xabcdef01);put(A+0x7c,0x80000000);put(frame+0x18,0x30000)
   if at==0x477770 and mutation&512:put(actor+0x30,0x7fffffff);put(frame+0x18,0x50000)
   if at==0x47df40:
    if mutation&1024:put(actor+0x2c,0x80000000);put(actor+0xf0,0xffffe000)
    r.u.reg_write(UC_X86_REG_EAX,read(D+0x100))
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)];r.hook(0x43e7d0,lambda:None);r.hook(0x47df40,lambda:None);failed=False;result=0
  try:
   r.call([0x451fd0,0x452310,0x44fd30,0x44fd80][mode],actor)
   while pending:pending.pop(0)()
   if mode>=2:result=r.u.reg_read(UC_X86_REG_EAX)
  except (FixtureFault,unicorn.UcError):failed=True;faults+=1
  for h in hooks:r.u.hook_del(h)
  r.clear_hooks();digest=1469598103934665603
  for b,n in regions:
   for byte in bytes(r.u.mem_read(b,n)):digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {digest} '+''.join(trace))
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',capture_output=True,text=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y];report={'source_sha256':HASH,'cases':a.cases,'differences':len(diffs),'fault_prefixes':faults,'calls':counts,'real_bodies':['451FD0','452310','44FD30','44FD80','477860','477820','477670','477770','477510 trig initialization'],'foreign_fixtures':['43E7D0 sound','47DF40 current ground'],'comparison':'Current physical actor/definition/frame/global bytes and ordered reads/stores/foreigncalls. Shared RNG, field aliases, current provider changes, failure prefix; savedregisters/ESP/returnABI excluded.','details':diffs[:1]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items()if k!='details'}))
 if diffs:print(json.dumps(diffs[0]));return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
