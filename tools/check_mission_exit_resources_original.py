#!/usr/bin/env python3
"""Compare the three exit helpers with the supplied original instructions."""
import argparse,hashlib,json,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectile_hits_original import Replay,HASH,STACK
BASE,END=0xbd4800,0xbf6c04
class Fault(Exception):pass
def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1024);p.add_argument('--report',type=Path);a=p.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rows=[];expected=[];faults=0
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def put(at,v,w=4):r.u.mem_write(at,(v&((1<<(8*w))-1)).to_bytes(w,'little'))
 for case in range(a.cases):
  op=case%3;seed=case;mutation=case%2;fault=0 if case%4 else ((case//4)%65+1);fault=(0x10000|(case%2+1))if case%17==0 else fault
  r.u.mem_write(BASE,bytes((i*13+seed)&255 for i in range(END-BASE)));put(0x67dbbc,seed)
  for at in range(0xbd5284,0xbd5480,4):put(at,0x1300000+(at-0xbd5284)*4 if (at//4+seed)%7 else 0)
  put(0xbd4828,0x1400000 if seed%5 else 0);put(0xbd482c,0x1400010 if seed%7 else 0)
  rows.append(f'{op} {seed} {mutation} {fault}');trace=[];pending=[];calls=stores=0
  def access(u,kind,at,w,v,user):
   nonlocal stores
   pc=u.reg_read(UC_X86_REG_EIP)
   if not 0x47a640<=pc<=0x47a790:return
   if not(BASE<=at and at+w<=END or at==0x67dbbc):return
   if kind==unicorn.UC_MEM_READ:trace.append(f'R{at},{w},{get(at,w)};')
   else:
    trace.append(f'W{at},{w},{v&((1<<(8*w))-1)};');stores+=1
    if fault&0x10000 and stores==(fault&255):pending.append(lambda:(_ for _ in ()).throw(Fault()))
  def code(u,at,size,user):
   nonlocal calls
   while pending:pending.pop(0)()
   n={0x49a840:1,0x4980d0:1,0x49a620:3}.get(at)
   if n is None:return
   args=list(r.read(u.reg_read(UC_X86_REG_ESP)+4,n));trace.append('C'+str(at)+''.join(','+str(v)for v in args)+';');calls+=1
   if not fault&0x10000 and fault and calls==fault:raise Fault()
   if mutation and calls==1:put(0xbd52ac,0x1400080);put(0xbd4828,77);put(0xbd482c,88)
   if at==0x49a620:r.u.mem_write(args[0],bytes([args[1]&255])*args[2])
   u.reg_write(UC_X86_REG_EAX,0xffffffff)
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)]
  for site in (0x49a840,0x4980d0,0x49a620):r.hook(site,lambda:None)
  failed=False
  try:
   r.call([0x47a640,0x47a660,0x47a690][op])
   while pending:pending.pop(0)()
  except(Fault,unicorn.UcError):failed=True;faults+=1
  for h in hooks:r.u.hook_del(h)
  r.clear_hooks();digest=1469598103934665603
  for byte in bytes(r.u.mem_read(BASE,END-BASE))+bytes(r.u.mem_read(0x67dbbc,4)):digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {digest} '+''.join(trace))
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',capture_output=True,text=True,check=True).stdout.splitlines();diffs=[{'case':i,'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y];assert len(got)==len(expected)
 report={'source_sha256':HASH,'cases':a.cases,'differences':len(diffs),'fault_prefixes':faults,'comparison':'Full BD4800..BF6C04 and status cell; ordered original parent reads/writes and foreign calls. Close/free/clear are declared fixtures. CPU registers and stack excluded.','details':diffs[:1]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report if diffs else {k:v for k,v in report.items()if k!='details'}));return bool(diffs)
if __name__=='__main__':raise SystemExit(main())
