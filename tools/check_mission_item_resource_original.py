#!/usr/bin/env python3
"""Compare45D6F0 state and ordered parent accesses with the local executable."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from check_projectile_hits_original import Replay,HASH,STACK
D,M,F=0x1300000,0x1302000,0x1303000
REGIONS=[(D,0x1000),(M,0x300),(F,4),(0xc23810,8),(0x503138,4),(0x503228,4)]
class FixtureFault(Exception):pass

def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=2048);p.add_argument('--report',type=Path);a=p.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rng=random.Random(0x45d6f0);rows=[];expected=[];faults=0;counts={};flag=STACK-0x14
 def read(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def put(at,n,w=4):r.u.mem_write(at,(n&((1<<(w*8))-1)).to_bytes(w,'little'))
 tags=[0x62697264,0x65666374,0x65663364,0x66616365,0x66697368,0x676e7263,0x68656c6f,0x6c616d70,0x6e756c6c,0x6f726730,0x70616c6d,0x70656e67,0x70777030,0x7368726b,0x74697265,0x74726565,0xffffffff]
 for case in range(a.cases):
  mode=rng.randrange(512);mutation=rng.randrange(2048);index=0xffffffff if case%19==0 else case%4;offset=(index*0x13c)&0xffffffff;model=D+0x20 if mode&64 else M
  regions=[(flag if b==F else b,n)for b,n in REGIONS]
  for b,n in regions:r.u.mem_write(b,rng.randbytes(n))
  put(0xc23810,(D-offset)&0xffffffff);put(0xc23814,[0,1,4,0x80000000][case%4]);put(0x503138,case%3);put(0x503228,rng.getrandbits(32))
  for base in [D,D+0x800]:
   put(base+0x28,[1,2,3,5,6,8,0,0xffffffff][(case//3)%8]);put(base+0x6c,M if mode&32 else 0);put(base+0x70,0);r.u.mem_write(base+0x2c,b'graphic\0' if mode&1 else bytes(8));r.u.mem_write(base+0x3c,b'husk\0' if mode&2 else bytes(5))
  for base in [M,M+0x100]:
   for j in range(4):put(base+0x28+j*4,tags[(case+j)%len(tags)])
   put(base+0x50,M+0x200 if case%5 else 0)
  put(M+0x240,[0,0xffff0000,0x80000000,0x7fffffff,rng.getrandbits(32)][case%5]);put(M+0x258,rng.getrandbits(32))
  data=b''.join(bytes(r.u.mem_read(b,n))for b,n in regions);rows.append(f'{index} {mode} {mutation} {data.hex()}');trace=[];pending=[];calls=stores=loads=0
  def norm(at):return F if at==flag else at
  def parent():return 0x45d6f0<=r.u.reg_read(UC_X86_REG_EIP)<=0x45d965
  def access(u,kind,at,width,value,user):
   nonlocal stores
   if not parent() or not any(b<=at and at+width<=b+n for b,n in regions):return
   if kind==unicorn.UC_MEM_READ:
    value=read(at,width);trace.append(f'R{norm(at)},{width},{value};')
    if mutation&1 and at==0x503138:pending.append(lambda:put(0xc23810,D+0x800-offset))
    if mutation&2 and at==0x503228:pending.append(lambda:put(0xc23810,D-offset))
   else:
    value&=(1<<(width*8))-1;trace.append(f'W{norm(at)},{width},{value};');stores+=1
    if mutation&4 and at==model+0x3c:pending.append(lambda:put(model+0x2c,0x68656c6f))
    if mutation&8 and at==model+0x40:pending.append(lambda:put(model+0x30,0x6f726730))
    if mutation&16 and at==model+0x44:pending.append(lambda:put(model+0x34,0x74726565))
    if mutation&32 and at==model+0x48:pending.append(lambda:put(model+0x28,0x65666374))
    if mutation&64 and at==D+0x9c:pending.append(lambda:put(0xc23810,D+0x800-offset))
    if mode&128 and stores==((mutation>>8)%9+1):pending.append(lambda:(_ for _ in ()).throw(FixtureFault()))
  sizes={0x4564b0:1,0x486530:3,0x42cbb0:1,0x42d200:2,0x4818f0:1,0x481a00:1}
  def code(u,at,size,user):
   nonlocal calls,loads
   while pending:pending.pop(0)()
   if at not in sizes:return
   args=list(r.read(r.u.reg_read(UC_X86_REG_ESP)+4,sizes[at]));trace.append('C'+str(at)+''.join(','+str(norm(x))for x in args)+';');calls+=1;counts[hex(at)]=counts.get(hex(at),0)+1
   if mode&256 and calls==((mutation>>8)%9+1):raise FixtureFault()
   if at==0x486530:
    loads+=1;put(flag,int(bool(mode&(4 if loads==1 else 8))))
    if mutation&128:put(0xc23810,(D+0x800 if loads&1 else D)-offset)
    r.u.reg_write(UC_X86_REG_EAX,0 if mode&16 else model if loads==1 else M+0x100)
   if at==0x42cbb0:
    if mutation&256:put(0xc23810,D-offset)
    r.u.reg_write(UC_X86_REG_EAX,0x1550000)
   if at==0x42d200:
    if mutation&512:put(0xc23810,D+0x800-offset)
    r.u.reg_write(UC_X86_REG_EAX,0x1551000)
   if at in [0x4818f0,0x481a00] and mutation&1024:put(0xc23810,D+0x800-offset)
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)]
  for at in [0x4564b0,0x486530,0x42cbb0,0x42d200]:r.hook(at,lambda:None)
  failed=False
  try:
   r.call(0x45d6f0,index)
   while pending:pending.pop(0)()
  except (FixtureFault,unicorn.UcError):failed=True;faults+=1
  for h in hooks:r.u.hook_del(h)
  r.clear_hooks();digest=1469598103934665603
  for b,n in regions:
   for byte in bytes(r.u.mem_read(b,n)):digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {digest} '+''.join(trace))
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',capture_output=True,text=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y];report={'source_sha256':HASH,'cases':a.cases,'differences':len(diffs),'fault_prefixes':faults,'calls':counts,'real_bodies':['45D6F0','4818F0','481A00'],'foreign_fixtures':['4564B0 diagnostic','486530 graphic cache','42CBB0 CHR cache','42D200 ANM cache'],'comparison':'Full current item/model/LOD/flag bytes; ordered parent reads/stores/calls; current-table changes, model-item alias, callback mutations and failures. ESP/saved registers/return value excluded.','details':diffs[:1]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items()if k!='details'}))
 if diffs:print(json.dumps(diffs[0]));return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
