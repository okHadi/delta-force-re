#!/usr/bin/env python3
"""Compare the two real event marker binders and ordered memory effects."""
import argparse,hashlib,json,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP
from check_projectile_hits_original import Replay,HASH
E,T,A,ALT,D,M=0x1300100,0x1301000,0x1302000,0x1303000,0x1304000,0x8ace80
REGIONS=[(0x4f2bc8,32),(0x1300000,0x5000),(M,4*0x114),(0x8cf900,4)]
class Fault(Exception):pass
def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1024);p.add_argument('--report',type=Path);a=p.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rows=[];expected=[];failures=0
 def owned(at,w):return any(b<=at and at+w<=b+n for b,n in REGIONS)
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def put(at,v,w=4):
  if not owned(at,w):raise Fault()
  r.u.mem_write(at,(v&((1<<(8*w))-1)).to_bytes(w,'little'))
 for case in range(a.cases):
  op=case%2;seed=case//2;mutation=0 if case%3==0 else(seed*37)&127;fault=(case//5)%180+1 if case%5==0 else 0
  for b,n in REGIONS:r.u.mem_write(b,bytes([seed&255])*n)
  put(0x4f2bcc,E);put(0x4f2bc8,0 if seed%13==0 else 0xffffffff if seed%17==0 else 2);put(0x8cf900,0 if seed%11==0 else 0xffffffff if seed%19==0 else 4)
  for i in range(2):put(E+i*24+4,T+i*128);put(E+i*24+8,A+i*128);put(E+i*24+21,2,1);put(E+i*24+22,2,1)
  for base in(T,A,ALT):
   for i in range(8):put(base+i*32+4,(0 if i%3==2 else 1)if base==T else(0 if i%3==2 else 13 if i%2 else 12));put(base+i*32+8,10 if base==T else 0);put(base+i*32+12,7);put(base+i*32+16,7)
  if seed%23==0:put(E+4,0);put(E+8,0)
  for i in range(4):put(M+i*0x114+4,0 if seed%7==0 and i==0 else D+i*64);put(D+i*64+0x20,0 if seed%5==0 and i==1 else 0x177d);put(M+i*0x114+0x7c,8 if seed%3==0 and i==2 else 7);put(M+i*0x114+0x24,0x80000000+i if i==0 else seed*17+i*31);put(M+i*0x114+0x28,0x7fffffff if i==0 else -seed*19-i*37)
  rows.append(f'{op} {seed} {mutation} {fault}');trace=1469598103934665603;entries=operations=0;read_mutated=write_mutated=False;pending=[]
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
  def read_effect(at):
   nonlocal read_mutated
   if not read_mutated and at==M+0x7c:
    read_mutated=True
    if mutation&1:put(E+(8 if op else 4),ALT)
    if mutation&8:put(0x8cf900,1)
   if at==M+0x24 and mutation&4:put(M+0x28,0x87654321)
   if at==E+(22 if op else 21)and mutation&32:put(0x4f2bc8,0)
  def write_effect():
   nonlocal write_mutated
   if not write_mutated:
    write_mutated=True
    if mutation&2:put(0x4f2bcc,E+24);put(0x4f2bc8,1);put(E+(22 if op else 21),2,1)
    if mutation&16:put(E+(8 if op else 4),ALT+32)
    if mutation&64:put(M+0x7c,8)
  def access(u,kind,at,w,v,user):
   pc=u.reg_read(UC_X86_REG_EIP)
   if not(0x42eea0<=pc<=0x42f1aa):return
   if not owned(at,w):
    if 0x2000000<=at<0x2010000:return
    raise Fault()
   if kind==unicorn.UC_MEM_READ:emit('R',at,w,get(at,w));read_effect(at);stop()
   else:emit('W',at,w,v&((1<<(w*8))-1));pending.extend([write_effect,stop])
  def code(u,at,size,user):
   while pending:pending.pop(0)()
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)];failed=False
  try:
   r.call(0x42f020 if op else 0x42eea0)
   while pending:pending.pop(0)()
  except(Fault,unicorn.UcError):failed=True;failures+=1
  for h in hooks:r.u.hook_del(h)
  digest=1469598103934665603
  for b,n in REGIONS:
   for v in bytes(r.u.mem_read(b,n)):digest=((digest^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {digest} {trace} {entries}')
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',text=True,capture_output=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y];report={'source_sha256':HASH,'cases':a.cases,'differences':len(diffs),'failure_prefixes':failures,'comparison':'Real42EEA0 and42F020;complete declared bytes and ordered scalar R/W;read/write mutations and failures;stack excluded. Physical marker/definition data are explicit controlled fixtures.','details':diffs[:12]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report));return bool(diffs)
if __name__=='__main__':raise SystemExit(main())
