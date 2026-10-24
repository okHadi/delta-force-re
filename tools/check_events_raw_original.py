#!/usr/bin/env python3
"""Compare captured raw event engine records, widths and call order with original code."""
import argparse,hashlib,json,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectile_hits_original import Replay,HASH
E,T,A,EE,TT,AA=0x1300100,0x1301100,0x1302100,0x1300600,0x1301600,0x1302600
REGIONS=[(0x4f2bc8,32),(0x6d7e4c,0x8410),(0x1300000,0x4000),(0x793640,0x1040),(0x67f520,4),(0x67f549,1),(0x6e4734,4),(0x67dbec,4)]
class Fault(Exception):pass
def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1024);p.add_argument('--report',type=Path);a=p.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rows=[];expected=[];faults=0
 def owned(at,w):return any(b<=at and at+w<=b+n for b,n in REGIONS)
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def put(at,v,w=4):
  if not owned(at,w):raise Fault()
  r.u.mem_write(at,(v&((1<<(w*8))-1)).to_bytes(w,'little'))
 sites={0x460330:1,0x460560:2,0x45f670:2,0x45f4c0:1,0x4601a0:1,0x45f5b0:1,0x404dd0:1,0x43f500:1,0x444ca0:1,0x460640:2,0x460280:1}
 for case in range(a.cases):
  op=case%8;seed=case//8;mutation=0 if case%3==0 else(seed*37)&255;fault=(case//5)%80+1 if case%5==0 else 0
  for b,n in REGIONS:r.u.mem_write(b,bytes([seed&255])*n)
  for at,v in [(0x4f2bc8,12),(0x4f2bcc,E),(0x4f2bd0,4),(0x4f2bd4,T),(0x4f2bd8,8),(0x4f2bdc,A),(0x4f2be0,seed%3),(0x4f2be4,seed%8),(0x6e4734,15 if seed%3 else 0xffffffff),(0x67f520,0 if seed%3 else 1)]:put(at,v)
  put(0x67f549,0 if seed%2 else 2,1);timers=[0,1,127,128,129,0x8000,0xffff]
  for base in(E,EE):
   for i in range(12):
    for off,v in[(0,(seed+i)%8),(4,T),(8,A),(12,((0x200 if seed%2 else 0)<<16)|timers[(seed+i*2)%7]),(16,((0x180 if seed%3 else 0)<<16)|timers[(seed+i)%7]),(20,(1 if seed%4==0 else 0)|(((seed+i)%4)<<8)|(((seed+i+1)%4)<<16))]:put(base+i*24+off,v)
  for base in(T,TT):
   for i in range(4):
    for off,v in[(0,(seed+i)%8),(4,(seed+i)%6),(8,(seed+i)%13),(12,(13 if seed%2 else 0xffffffff)if(seed+i)%6==2 else i%2),(16,(seed+i)%64),(20,(seed+i)%32)]:put(base+i*32+off,v)
  for base in(A,AA):
   for i in range(8):
    for off,v in[(0,0),(4,(seed+i)%17),(12,seed+i),(16,seed*3+i)]:put(base+i*32+off,v)
  for i in range(4):put(0x7936a4+i*64,i%3);put(0x7936a8+i*64,8);put(0x7936ac+i*64,seed%7)
  rows.append(f'{op} {seed} {mutation} {fault}');trace=1469598103934665603;entries=operations=calls=0;fired=False;pending=[]
  def emit(kind,at,w,v):
   nonlocal trace,entries
   entries+=1
   for x in(ord(kind),at,w,v):
    for i in range(4):trace=((trace^((x>>(i*8))&255))*1099511628211)&0xffffffffffffffff
  def stop():
   nonlocal operations
   if fault:
    operations+=1
    if operations==fault:raise Fault()
  def after_fired():
   if mutation&32:put(0x4f2bc8,3);put(0x4f2bcc,EE);put(E+8,AA);put(E+22,3,1)
  def access(u,kind,at,w,v,user):
   nonlocal fired
   pc=u.reg_read(UC_X86_REG_EIP)
   if not(0x42e520<=pc<=0x42ebd4 or 0x42ee50<=pc<=0x42ee69 or 0x443c8c<=pc<=0x443ca5):return
   if not owned(at,w):
    if 0x2000000<=at<0x2010000 or 0x42e5cc<=at<0x42e620 or 0x42e7cc<=at<0x42e810:return
    raise Fault()
   if kind==unicorn.UC_MEM_READ:emit('R',at,w,get(at,w));stop()
   else:
    emit('W',at,w,v&((1<<(w*8))-1))
    if not fired and at==E+20 and w==1:fired=True;pending.append(after_fired)
    pending.append(stop)
  def code(u,at,size,user):
   nonlocal calls
   while pending:pending.pop(0)()
   if at not in sites:return
   args=list(r.read(u.reg_read(UC_X86_REG_ESP)+4,sites[at]));emit('C',at,len(args),0)
   for v in args:emit('A',0,0,v)
   stop();calls+=1
   if calls==1:
    if mutation&1:put(E+4,TT);put(E+8,AA)
    if mutation&2:put(0x4f2bcc,EE);put(0x4f2bc8,1);put(0x4f2be4,5)
    if mutation&4:put(E+12,128,2);put(E+18,128,2)
    if mutation&8:put(E+21,0,1);put(E+22,1,1)
    if mutation&16:put(A+32+4,15)
    if mutation&64:put(0x4f2bc8,16 if seed%2 else 0)
    if mutation&128:put(0x67f520,0);put(0x67f549,2,1)
   u.reg_write(UC_X86_REG_EAX,0x101 if seed%3 else 0)
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)]
  for site in sites:r.hook(site,lambda:None)
  if op==7:r.hook(0x443cab,lambda:None)
  failed=False;result=0
  try:
   address=[0x42e9e0,0x42e930,0x42e620,0x42e810,0x42eaf0,0x42eb60,0x42eba0,0x443c8c][op]
   value=r.call(address,*([E if seed%37 else 0]if op==0 else[E]if op==1 else[T]if op==2 else[A]if op==3 else[]))
   if op in(1,2):result=value
   while pending:pending.pop(0)()
  except(Fault,unicorn.UcError):failed=True;faults+=1
  for h in hooks:r.u.hook_del(h)
  r.clear_hooks();digest=1469598103934665603
  for b,n in REGIONS:
   for byte in bytes(r.u.mem_read(b,n)):digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {digest} {trace} {entries}')
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',capture_output=True,text=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y];report={'source_sha256':HASH,'cases':a.cases,'differences':len(diffs),'failure_prefixes':faults,'comparison':'Real raw process/combine/evaluate/dispatch/tick/bit passes/frame block and real flag tests. Complete declared bytes + exact ordered R/W/C tuple hash/count. Host action/probe effects are fixtures; fired reads use real mapped BYTE addresses including past active count. Immutable jump tables and stack are excluded.','details':diffs[:8]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report));return bool(diffs)
if __name__=='__main__':raise SystemExit(main())
