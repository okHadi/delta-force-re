#!/usr/bin/env python3
"""Replay486530 with mutable shared arguments, flag and source registry."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectile_hits_original import Replay,HASH,STACK
N,F,E,P,C,R=0x1300000,0x1301000,0x1302000,0x1303000,0xc80d14,0xc80d20
REGIONS=[(N,64),(F,32),(E,12),(P,8),(C,4),(R,4*0x7c)]
class FixtureFault(Exception):pass

def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=2048);p.add_argument('--report',type=Path);a=p.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rng=random.Random(0x486530);rows=[];expected=[];faults=0;counts={};frame=STACK-0x30;arguments=STACK+4
 def read(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def put(at,n,w=4):r.u.mem_write(at,(n&((1<<(w*8))-1)).to_bytes(w,'little'))
 def text(at,value):r.u.mem_write(at,value.encode()+b'\0')
 for case in range(a.cases):
  mode=case%2;mutation=rng.randrange(4096);regions=[(frame if b==F else arguments if b==E else b,n)for b,n in REGIONS]
  r.u.mem_write(R,bytes(500*0x7c))
  for b,n in regions:r.u.mem_write(b,rng.randbytes(n))
  for i in range(4):r.u.mem_write(R+i*0x7c,bytes(0x7c));text(R+i*0x7c+4,['bird','tree','gazelle','bexp'][i])
  name=['new','GaZeLlE','tree','new.file.name','even','o','', 'abcdefghijklmno','q'*34][case%9];text(N,name);text(N+40,'second');put(arguments,N if case%23 else frame);put(arguments+4,case%6);put(arguments+8,P if case%17 else arguments+4);put(C,0xffffffff if case%41==0 else 0x80000000 if case%43==0 else 500 if case%29==0 else case%3)
  if case%23==0:text(frame,'alias')
  data=b''.join(bytes(r.u.mem_read(b,n))for b,n in regions)
  # Normalize the one initial pointer into the caller frame.
  native=bytearray(data);argoffset=64+32
  if read(arguments)==frame:native[argoffset:argoffset+4]=F.to_bytes(4,'little')
  if read(arguments+8)==arguments+4:native[argoffset+8:argoffset+12]=(E+4).to_bytes(4,'little')
  rows.append(f'{mode} {mutation} {native.hex()}');trace=[];pending=[];calls=stores=compares=0
  def norm(at):
   if frame<=at<frame+32:return F+at-frame
   if arguments<=at<arguments+12:return E+at-arguments
   return at
  def pointer(value):
   return F if value==frame else E+4 if value==arguments+4 else value
  def parent():return 0x486530<=r.u.reg_read(UC_X86_REG_EIP)<=0x486647
  def access(u,kind,at,width,value,user):
   nonlocal stores
   if not parent():return
   # Source copy overflow faults at the same declared owner boundary.
   if frame+32<=at<frame+36 and kind==unicorn.UC_MEM_WRITE and r.u.reg_read(UC_X86_REG_EIP)in[0x4865a0,0x4865ac]:
    trace.append(f'W{F+at-frame},{width},{value&255};');raise FixtureFault()
   if not any(b<=at and at+width<=b+n for b,n in regions):return
   if kind==unicorn.UC_MEM_READ:
    value=read(at,width);value=pointer(value)if at in [arguments,arguments+8]else value;trace.append(f'R{norm(at)},{width},{value};')
    if mutation&1 and at==arguments:pending.append(lambda:put(arguments,N+16))
   else:
    value&=(1<<(width*8))-1;trace.append(f'W{norm(at)},{width},{value};');stores+=1
    if mutation&2 and at==P:pending.append(lambda:put(C,2))
    if mutation&4 and at==P+4:pending.append(lambda:put(C,3))
    if mutation&8 and at==frame+1:pending.append(lambda:put(N+2,ord('X'),1))
    if mutation&1024 and stores==((mutation>>7)%6+1):pending.append(lambda:(_ for _ in ()).throw(FixtureFault()))
  sites={0x499e5e:2,0x456560:2,0x48fd00:1,0x486370:3,0x48fd70:0};after={}
  def code(u,at,size,user):
   nonlocal calls,compares
   while pending:pending.pop(0)()
   if at in after:put(C,1);after.pop(at)
   if at not in sites:return
   caller=read(r.u.reg_read(UC_X86_REG_ESP))
   if not 0x486530<=caller<=0x486647:return
   args=list(r.read(r.u.reg_read(UC_X86_REG_ESP)+4,sites[at]))if sites[at]else [];trace.append('C'+str(at)+''.join(','+str(norm(x))for x in args)+';');calls+=1;counts[hex(at)]=counts.get(hex(at),0)+1
   if mutation&2048 and calls==((mutation>>7)%6+1):raise FixtureFault()
   if at==0x499e5e:
    compares+=1
    if mutation&16 and compares==1:after[caller]=True
   if at==0x48fd00 and mutation&32:put(arguments+4,5);put(C,1)
   if at==0x486370:
    if mutation&64:put(arguments+8,P+4)
    if not R<=args[0] or args[0]+0x24>R+4*0x7c:raise FixtureFault()
    put(args[0]+0x20,0xabcdef01);r.u.reg_write(UC_X86_REG_EAX,0 if mode&1 else 1)
   if at==0x48fd70:
    if mutation&128:put(arguments+8,P+4)
    if mutation&256:put(C,2)
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)]
  for site in [0x48fd00,0x486370,0x48fd70]:r.hook(site,lambda:None)
  failed=False;result=0
  try:
   r.call(0x486530,*r.read(arguments,3))
   while pending:pending.pop(0)()
   result=r.u.reg_read(UC_X86_REG_EAX)
  except (FixtureFault,unicorn.UcError):failed=True;faults+=1
  for h in hooks:r.u.hook_del(h)
  r.clear_hooks();digest=1469598103934665603
  for b,n in regions:
   data=bytearray(r.u.mem_read(b,n))
   if b==arguments:
    for offset in [0,8]:data[offset:offset+4]=pointer(int.from_bytes(data[offset:offset+4],'little')).to_bytes(4,'little')
   for byte in data:digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {digest} '+''.join(trace))
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',capture_output=True,text=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y];report={'source_sha256':HASH,'cases':a.cases,'differences':len(diffs),'fault_prefixes':faults,'calls':counts,'real_bodies':['486530','499E5E->49BE20','456560'],'foreign_fixtures':['48FD00/48FD70 loadcontext','486370 coldloadsuccess/failureandpartialwrites'],'comparison':'Allsharedargument/name/scratch/flag/count/registrybytes and orderedparentreads/writes/calls; currentmutations, input/framealias, sourcewordflagalias, boundedownerfaultoncopyoverflow. ESP/savedregisters excluded.','details':diffs[:1]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items()if k!='details'}))
 if diffs:print(json.dumps(diffs[0]));return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
