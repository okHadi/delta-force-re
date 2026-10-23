#!/usr/bin/env python3
"""Replay450AE0 and45C6A0 with current memory, aliases and failure prefixes."""
import argparse,hashlib,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from check_projectile_hits_original import Replay,HASH,STACK
A,P,M,L,S,T,F=0x7dde80,0x794680,0x8ace80,0x78f280,0x793600,0x867e80,0x1304000
REGIONS=[(A,0x114),(P,0x168),(M,0x33c),(L,0x220),(S,0x300),(T,0x114),(F,16)]
class FixtureFault(Exception):pass

def main():
 q=argparse.ArgumentParser();q.add_argument('game',type=Path);q.add_argument('probe',type=Path);q.add_argument('--cases',type=int,default=1024);q.add_argument('--report',type=Path);args=q.parse_args()
 raw=(args.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rng=random.Random(0x450ae0);rows=[];expected=[];faults=0;calls={}
 def put(at,n):r.write(at,n&0xffffffff)
 def read(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 for case in range(args.cases):
  mode=case%2;mutation=(case//2)%512 if case%4 else 0;frame=STACK-0x20 if not mode else F;regions=[(frame if base==F else base,n) for base,n in REGIONS]
  for base,n in regions:r.u.mem_write(base,rng.randbytes(n))
  put(A+0xc,P);kind=(case//2)%5;put(P+0xa4,kind);put(P+0xb4+0xa4,1);put(P+0xb4+0xac,2);put(P+0xb4+0xb0,0)
  put(P+0xac,(case//10)%4 if kind!=3 else (T if case%7 else 0));put(P+0xb0,case%3 if kind!=3 else 123 if case%11 else 124);put(T+8,123)
  for li in range(4):
   put(L+li*0x88,(case//2)%2);put(L+li*0x88+4,(case//7)%4)
   for i in range(32):put(L+li*0x88+8+i*4,i%3)
  put(A+0x70,((case%5)-1)<<16)
  edges=[0,1,0xffffffff,0x80000000,0x7fffffff,0x40000000,0xc0000000]
  for base in [A,M,M+0x114,M+0x228,T]:
   for offset in [0x24,0x28,0x2c,0x7c]:put(base+offset,rng.choice(edges) if case%3 else rng.getrandbits(32))
  if not mode and kind==3:
   for at in (A+0x24,A+0x28,A+0x2c,A+0x7c,T+0x28,T+0x2c):put(at,0)
   put(T+0x24,0x300000)
  angle,distance,radius,yaw=F+8,F+12,F,F+4
  if mode and case%7==1:radius=M+0x7c
  if mode and case%7==3:angle=distance=radius=yaw=A+0x8c
  data=b''.join(bytes(r.u.mem_read(base,n)) for base,n in regions);rows.append(f'{mode} {mutation} {angle} {distance} {radius} {yaw} {data.hex()}')
  trace=[];pending=[];armed_read=False
  def norm(at):return F+at-frame if frame<=at<frame+16 else at
  def parent():
   pc=r.u.reg_read(UC_X86_REG_EIP);return 0x450ae0<=pc<=0x450c3e or 0x45c6a0<=pc<=0x45c807
  def access(u,kind,at,width,value,user):
   nonlocal armed_read
   if not parent() or not any(b<=at and at+width<=b+n for b,n in regions):return
   if kind==unicorn.UC_MEM_READ:
    n=read(at,width);trace.append(f'R{norm(at)},{width},{n};')
    if not mode and not armed_read and mutation&128 and at==P+0xa4 and not n:armed_read=True;pending.append(lambda:put(P+0xa4,1))
   else:
    trace.append(f'W{norm(at)},{width},{value&0xffffffff};')
    if mutation&256 and at==A+0x8c:pending.append(lambda:put(A+0x70,3<<16))
    if mutation&16 and at==A+0x8c:pending.append(lambda:(_ for _ in ()).throw(FixtureFault()))
  def argv(n):return list(r.read(r.u.reg_read(UC_X86_REG_ESP)+4,n))
  def record(site,aa):trace.append('C'+str(site)+''.join(','+str(norm(v)) for v in aa)+';');calls[hex(site)]=calls.get(hex(site),0)+1
  def event():
   record(0x42ee30,argv(3))
   if mutation&4:put(P+0xac,2);put(P+0xb0,1);put(A+0xc,P+0xb4)
   if mutation&64:raise FixtureFault()
   r.u.reg_write(UC_X86_REG_EAX,0)
  def code(u,at,size,user):
   while pending:pending.pop(0)()
   if not mode and at in (0x450bfc,0x450bb3) and mutation&32:put(P+0xa4,0)
   if at==0x45c6a0 and not mode:record(at,argv(5))
   if at in (0x477670,0x477770):
    record(at,argv(3 if at==0x477670 else 2))
    if mutation&1:put(A+0xc,P+0xb4)
    if mutation&2:put(M+0x7c,0x12345);put(M+0x30,0x12345678);put(A+0x70,2<<16)
    if mutation&8:raise FixtureFault()
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)];r.hook(0x42ee30,event)
  failed=False;result=0
  try:
   result=r.call(0x45c6a0,A,angle,distance,radius,yaw) if mode else r.call(0x450ae0,A)
   while pending:pending.pop(0)()
  except FixtureFault:failed=True;faults+=1
  for h in hooks:r.u.hook_del(h)
  r.clear_hooks();digest=1469598103934665603
  for b,n in regions:
   for byte in r.u.mem_read(b,n):digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {digest} '+''.join(trace))
 got=subprocess.run([str(args.probe.resolve())],input='\n'.join(rows)+'\n',text=True,capture_output=True,check=True).stdout.splitlines();assert len(got)==len(expected)
 differences=[{'case':i,'input':rows[i],'original':x,'port':y} for i,(x,y) in enumerate(zip(expected,got)) if x!=y]
 report={'source_sha256':HASH,'cases':args.cases,'differences':len(differences),'fault_prefixes':faults,'calls':calls,'real_bodies':['450AE0','45C6A0','477670','477770'],'foreign_fixtures':['42EE30 event flag callback with current-state changes'],'comparison':'Full current actor/person/target/marker/list/side/frame bytes and ordered parent reads/writes/calls; rawtargetidentity, falseoutputs and output aliases','details':differences[:3]}
 if args.report:args.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='details'}));
 if differences:print(json.dumps(differences[0]));return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
