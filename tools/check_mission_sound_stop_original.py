#!/usr/bin/env python3
"""Replay43F020 and real49A620 against all1024 shared registration rows."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
from check_projectile_hits_original import Replay,HASH
A,R=0x7dde80,0x6e0700
class FixtureFault(Exception):pass

def main():
 q=argparse.ArgumentParser();q.add_argument('game',type=Path);q.add_argument('probe',type=Path);q.add_argument('--cases',type=int,default=256);q.add_argument('--report',type=Path);a=q.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);expected=[];faults=clears=0
 def put(at,n,w=4):r.u.mem_write(at,(n&((1<<(w*8))-1)).to_bytes(w,'little'))
 def read(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 for seed in range(a.cases):
  actor=A if seed%19 else 0;scale=seed&255 if seed%13 else 0x100+(seed&255);mutation=seed%128
  r.u.mem_write(R,bytes((seed+i*17)&255 for i in range(16384)));r.u.mem_write(A,bytes((seed+i*17)&255 for i in range(0x114)))
  for i in range(1024):put(R+i*16+4,A+0x114 if i%11 else 0);put(R+i*16+12,(seed+i)&255,1)
  selected=[seed%1024,(seed*7+511)%1024,1023]
  for j,i in enumerate(selected):put(R+i*16+4,actor);put(R+i*16+12,(seed+j%2)&255,1)
  put(A+0x1c,R+selected[seed%3]*16);trace=[];pending=[];current_row=0
  def parent():return 0x43f020<=r.u.reg_read(UC_X86_REG_EIP)<=0x43f06c
  def access(u,kind,at,width,value,user):
   if not parent() or not (R<=at and at+width<=R+16384 or A<=at and at+width<=A+0x114):return
   if kind==unicorn.UC_MEM_READ:
    trace.append(f'R{at},{width},{read(at,width)};')
    if mutation&64 and R<=at<R+16384 and (at-R)%16==4:pending.append(lambda at=at:put(at+8,scale&255,1))
    if mutation&16 and at==A+0x1c:raise FixtureFault()
   else:
    trace.append(f'W{at},{width},{value&0xffffffff};')
    if mutation&32:pending.append(lambda:(_ for _ in ()).throw(FixtureFault()))
  def code(u,at,size,user):
   nonlocal current_row,clears
   while pending:pending.pop(0)()
   if at==0x49a620:
    args=list(r.read(r.u.reg_read(UC_X86_REG_ESP)+4,3));trace.append('C'+str(at)+''.join(','+str(n) for n in args)+';');current_row=args[0];clears+=1
    if mutation&8:raise FixtureFault()
   if at==0x43f04b:
    if mutation&1:put(A+0x1c,current_row)
    if mutation&2:put(A+0x1c,R+((current_row-R+16)%16384))
    if mutation&4 and current_row<R+16368:put(current_row+20,actor);put(current_row+28,scale&255,1)
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)];failed=False
  try:
   r.call(0x43f020,actor,scale)
   while pending:pending.pop(0)()
  except (FixtureFault,unicorn.UcError):failed=True;faults+=1
  for h in hooks:r.u.hook_del(h)
  digest=1469598103934665603
  for b,n in [(R,16384),(A,0x114)]:
   for byte in r.u.mem_read(b,n):digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {digest} '+''.join(trace))
 got=subprocess.run([str(a.probe.resolve())],input=''.join(f'{i}\n' for i in range(a.cases)),text=True,capture_output=True,check=True).stdout.splitlines();assert len(got)==len(expected)
 differences=[{'seed':i,'original':x,'port':y} for i,(x,y) in enumerate(zip(expected,got)) if x!=y]
 report={'source_sha256':HASH,'cases':a.cases,'differences':len(differences),'fault_prefixes':faults,'clear_calls':clears,'real_bodies':['43F020','49A620'],'comparison':'All1024 registration rows and actorimage, ordered rawreads/stores/clearcalls, mutablelink/scale/nextrows and nullactorfaultprefix','foreign_fixtures':['controlled clear/actor-access failures and post-clear memory mutations'],'details':differences[:1]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='details'}));
 if differences:print(json.dumps(differences[0]));return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
