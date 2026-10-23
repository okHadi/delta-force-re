#!/usr/bin/env python3
"""Replay42A440 and45DD40 with current arguments, metadata and frame aliases."""
import argparse,hashlib,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from check_projectile_hits_original import Replay,HASH,STACK
F,G,D,T,C=0x1304000,0x1306000,0x1302000,0x8e44b0,0xc23810
REGIONS=[(F,0x114),(G,12),(D,0x4f0),(T,68),(C,4)]
class FixtureFault(Exception):pass

def main():
 q=argparse.ArgumentParser();q.add_argument('game',type=Path);q.add_argument('probe',type=Path);q.add_argument('--cases',type=int,default=2048);q.add_argument('--report',type=Path);a=q.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rng=random.Random(0x42a440);rows=[];expected=[];faults=0;counts={}
 def put(at,n,w=4):r.u.mem_write(at,(n&((1<<(w*8))-1)).to_bytes(w,'little'))
 def read(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 for case in range(a.cases):
  mode=case%2;mutation=((case//2)*37+case//8)%2048 if case%7 else 0;seed=rng.getrandbits(32);frame=F if mode else STACK-0x120;arguments=G if mode else STACK+4;regions=[(frame if b==F else arguments if b==G else b,n)for b,n in REGIONS]
  for b,n in regions:r.u.mem_write(b,rng.randbytes(n))
  put(C,D);put(0xbd0060,seed);data=b''.join(bytes(r.u.mem_read(b,n))for b,n in regions);rows.append(f'{mode} {mutation} {seed} {data.hex()}');trace=[];pending=[];draws=loads=0;definition=D
  def norm(at):
   if frame<=at<frame+0x114:return F+at-frame
   if arguments<=at<arguments+12:return G+at-arguments
   return at
  def pointer(value):return F+value-frame if not mode and mutation&1024 and frame-0x13c<=value<=frame+0x114 else value
  def parent():
   pc=r.u.reg_read(UC_X86_REG_EIP);return 0x42a440<=pc<=0x42a518 or 0x45dd40<=pc<=0x45e04b
  def access(u,kind,at,width,value,user):
   if not parent() or not any(b<=at and at+width<=b+n for b,n in regions):return
   if kind==unicorn.UC_MEM_READ:
    value=read(at,width);value=pointer(value)if at==C else value;trace.append(f'R{norm(at)},{width},{value};')
   else:
    value=value&0xffffffff;value=pointer(value)if at==frame+4 else value;trace.append(f'W{norm(at)},{width},{value};')
    if mutation&32 and at==frame+0x110:pending.append(lambda:put(definition+0x90,0x87654321))
    if mutation&512 and at==frame+0x7c:pending.append(lambda:(_ for _ in ()).throw(FixtureFault()))
    if mode and mutation&2 and T<=at<T+68:pending.append(lambda at=at,value=value:put(at,value+77))
  def argv(n):return list(r.read(r.u.reg_read(UC_X86_REG_ESP)+4,n))
  def record(site,args):trace.append('C'+str(site)+''.join(','+str(norm(n))for n in args)+';');counts[hex(site)]=counts.get(hex(site),0)+1
  def lookup():
   nonlocal definition
   args=argv(1);record(0x4828c0,args)
   if mutation&128:raise FixtureFault()
   index=(args[0]^seed)&1023 if mode else seed&1
   if not mode:put(C,frame-index*0x13c if mutation&1024 else D+0x13c if mutation&1 else D);definition=read(C)+index*0x13c
   r.u.reg_write(UC_X86_REG_EAX,index)
  def allocate():
   record(0x45d970,argv(2))
   if mutation&64:raise FixtureFault()
   r.u.reg_write(UC_X86_REG_EAX,0)
  def resource():
   nonlocal loads
   args=argv(1);record(0x45d6f0,args);loads+=1
   if mutation&4 and loads==3:raise FixtureFault()
   if mutation&1:put(T+64,args[0]+100)
  def code(u,at,size,user):
   nonlocal draws
   while pending:pending.pop(0)()
   if at==0x49a620:
    record(at,argv(3))
    if mutation&256:raise FixtureFault()
   if at==0x42a47e:
    if mutation&2:put(arguments,read(arguments)+111);put(arguments+4,read(arguments+4)+222);put(arguments+8,read(arguments+8)+333);put(definition+0x6c,0x777)
    if mutation&4:put(C,D)
   if at==0x477860:
    record(at,[]);draws+=1
    if mutation&16 and draws==2:raise FixtureFault()
    if mutation&8:put(definition+0x84,0x12345678);put(definition+0x90,0x23456789);put(frame+0x24,0x3456789a);put(arguments+8,999)
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)];r.hook(0x4828c0,lookup);r.hook(0x45d970,allocate);r.hook(0x45d6f0,resource);failed=False
  try:
   if mode:r.call(0x45dd40)
   else:r.call(0x42a440,*r.read(arguments,3))
   while pending:pending.pop(0)()
  except FixtureFault:failed=True;faults+=1
  for h in hooks:r.u.hook_del(h)
  r.clear_hooks();digest=1469598103934665603
  for b,n in regions:
   data=bytearray(r.u.mem_read(b,n))
   if not mode and b==frame and mutation&1024:
    v=int.from_bytes(data[4:8],'little');data[4:8]=pointer(v).to_bytes(4,'little')
   if not mode and b==C and mutation&1024:data[:]=pointer(int.from_bytes(data,'little')).to_bytes(4,'little')
   for byte in data:digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {read(0xbd0060)} {digest} '+''.join(trace))
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',capture_output=True,text=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y];report={'source_sha256':HASH,'cases':a.cases,'differences':len(diffs),'fault_prefixes':faults,'calls':counts,'real_bodies':['42A440','45DD40','49A620','477860'],'foreign_fixtures':['4828C0 currentlookup','45D6F0 resourceloader','45D970 currentallocator'],'comparison':'Currentarguments/definitiontable/producerwords/full114frame and ordered reads/stores/calls; definition-framealias/currentcallbackmutation/failureprefix; savedregisters/RET/ESP excluded','details':diffs[:1]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items()if k!='details'}));
 if diffs:print(json.dumps(diffs[0]));return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
