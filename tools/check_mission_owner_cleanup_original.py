#!/usr/bin/env python3
"""Compare611-byte sound/bank/resource cleanup bodies with current raw memory."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectile_hits_original import Replay,HASH,STACK
C,B,L,M,A,F,E,T,V,H,J,R,D,Z=0x1300000,0x1301000,0x1302000,0x1303000,0x1304000,0x1305000,0x1306000,0x1307000,0x6e06c4,0x6e06e8,0x6e0700,0x503304,0xd08940,0x4a2712
REGIONS=[(C,128),(B,160),(L,144),(M,168),(A,156),(F,16),(E,4),(T,160),(V,12),(H,24),(J,64),(R,64),(D,4),(Z,4)]
class Fault(Exception):pass

def main():
 parser=argparse.ArgumentParser();parser.add_argument('game',type=Path);parser.add_argument('probe',type=Path);parser.add_argument('--cases',type=int,default=1024);parser.add_argument('--report',type=Path);a=parser.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rng=random.Random(0x43e6d0);rows=[];expected=[];faults=0;counts={};frame=STACK-0x20;arguments=STACK+4
 def read(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def put(at,value,w=4):r.u.mem_write(at,(value&((1<<(w*8))-1)).to_bytes(w,'little'))
 ranges=[(0x43e6d0,0x43e71b),(0x494dc0,0x494dcd),(0x4a3590,0x4a35a9),(0x43e640,0x43e6c7),(0x49db20,0x49dc14),(0x402d40,0x402d4d),(0x44b940,0x44b991),(0x44b920,0x44b931)]
 for case in range(a.cases):
  op=case%4;mutation=0 if case%3 else rng.randrange(1024)|(rng.randrange(12)<<10);regions=[(frame if b==F and op==1 else arguments if b==E and op==1 else b,n)for b,n in REGIONS]
  for b,n in regions:r.u.mem_write(b,rng.randbytes(n))
  r.u.mem_write(J+64,bytes(0x4000-64));arg=arguments if op==1 else E
  put(arg,0 if case%23==0 else C);put(C+0x38,0 if case%17==0 else B);put(C+0x28,(case//4)%3);put(C+64+0x38,B+80);put(C+64+0x28,1)
  for i in range(2):put(B+i*80+0x24,2 if i==0 else 1);put(B+i*80+0x28,L+i*96);put(B+i*80+0x2c,L+48)
  for i in range(3):
   put(L+i*48,(case+i)%3,2);put(L+i*48+0x10,M+i*56)
   for k in range(2):put(M+i*56+k*28,A+((i+k)%3)*52)
   put(A+i*52+0x23,(case+i)%2,1);put(A+i*52+0x24,0x1400000+i*32)
  put(V,T);put(V+4,[0,1,2,0xffffffff][(case//4)%4]);put(D,(case//4)%2)
  for i in range(6):put(H+i*4,0 if (case+i)%3==0 else 20+i)
  for i in range(4):put(T+i*40+0x18,0x1401000+i*16);put(T+i*40+0x20,4 if (case+i)%2 else 0,1)
  for i in range(4):put(R+i*4,0 if (case+i)%3==0 else 0x1402000+i*16)
  rows.append(f'{op} {mutation} '+b''.join(bytes(r.u.mem_read(b,n))for b,n in regions).hex());trace=[];pending=[];calls=stores=0
  def norm(at):return F+at-frame if op==1 and frame<=at<frame+16 else E+at-arguments if op==1 and arguments<=at<arguments+4 else at
  def owned(at,width):return any(b<=at and at+width<=b+n for b,n in regions)or J<=at and at+width<=J+0x4000
  def access(u,kind,at,width,value,user):
   nonlocal stores
   pc=u.reg_read(UC_X86_REG_EIP)
   if not any(lo<=pc<=hi for lo,hi in ranges):return
   if not owned(at,width):
    if STACK-0x100<=at<STACK+0x20:return
    if kind==unicorn.UC_MEM_WRITE:trace.append(f'W{norm(at)},{width},{value&((1<<(width*8))-1)};')
    raise Fault()
   if kind==unicorn.UC_MEM_READ:
    value=read(at,width);trace.append(f'R{norm(at)},{width},{value};')
    if mutation&1 and at==H:pending.append(lambda:put(H+4,77))
    if mutation&2 and at==C+0x28:pending.append(lambda:put(arg,C+64))
    if mutation&8 and at==A+0x23:pending.append(lambda:put(A+0x24,0x1400090))
   else:
    value&=(1<<(width*8))-1;trace.append(f'W{norm(at)},{width},{value};');stores+=1
    if mutation&4 and at==T+0x18:pending.append(lambda:put(V,T+40))
    if mutation&16 and stores==((mutation>>10)%10+1):pending.append(lambda:(_ for _ in ()).throw(Fault()))
  sites={0x4560a0:0,0x4a2aed:0,0x495070:1,0x4980d0:1,0x49a620:3,0x455b40:0,0x455ea0:0,0x4950a0:2,0x49cf70:1}
  def code(u,at,size,user):
   nonlocal calls
   while pending:pending.pop(0)()
   if at not in sites:return
   aa=list(r.read(u.reg_read(UC_X86_REG_ESP)+4,sites[at]))if sites[at]else[];trace.append('C'+str(at)+''.join(','+str(norm(v))for v in aa)+';');calls+=1;counts[hex(at)]=counts.get(hex(at),0)+1
   if mutation&32 and calls==((mutation>>10)%12+1):raise Fault()
   if at==0x4980d0:
    if mutation&64:put(B+0x24,1);put(L,0,2)
    if mutation&128:put(L+0x10,M+56);put(C+0x38,B+80)
    if mutation&512:put(R+4,read(R+4)+1)
   if at==0x49a620:
    for i in range(aa[2]):
     if not owned(aa[0]+i,1):raise Fault()
     put(aa[0]+i,aa[1]&255,1)
   if at==0x4a2aed and mutation&256:put(H,99)
   u.reg_write(UC_X86_REG_EAX,0)
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)]
  for site in sites:r.hook(site,lambda:None)
  failed=False
  try:
   if op==0:r.call(0x43e6d0)
   elif op==1:r.call(0x49db20,read(arg))
   elif op==2:r.call(0x402d40)
   else:r.call(0x44b940,R+4)
   while pending:pending.pop(0)()
  except(Fault,unicorn.UcError):failed=True;faults+=1
  for hook in hooks:r.u.hook_del(hook)
  r.clear_hooks();digest=1469598103934665603
  for b,n in regions:
   for byte in bytes(r.u.mem_read(b,n)):digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {digest} '+''.join(trace))
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',capture_output=True,text=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y];report={'source_sha256':HASH,'real_bodies':['43E6D0','494DC0/4A3590','43E640','49DB20','402D40','44B940','44B920'],'cases':a.cases,'differences':len(diffs),'fault_prefixes':faults,'calls':counts,'comparison':'Complete declared currentcontexts/banks/layers/members/retainedassets/frame/argument/ambient/handles/registrationprefix/resource/gates withorderedownedreads/writes/foreigncalls andpartialfaults. Foreignrelease retainsbytes fixture; Script/Playback helpers explicitfixtures. Fullregistrationclearcall is checked withdeclared64-byte prefix state. CPUregisters/savedstack excluded.','details':diffs[:1]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items()if k!='details'}))
 if diffs:
  d=diffs[0];x=d['original'].split(' ',2);y=d['port'].split(' ',2);print('first',d['case'],x[:2],y[:2]);xx=x[-1].split(';');yy=y[-1].split(';')
  for i,(v,z)in enumerate(zip(xx,yy)):
   if v!=z:print('trace',i,xx[max(0,i-3):i+4],yy[max(0,i-3):i+4]);break
  return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
