#!/usr/bin/env python3
"""Compare real460280 and45FF20 current memory effects."""
import argparse,hashlib,json,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP
from check_projectile_hits_original import Replay,HASH
ACTION,ACTOR,PERSON=0x1300100,0x7cca80,0x794680
REGIONS=[(ACTION,32),(ACTOR,3*0x114),(PERSON,3*0xb4),(0x8cfa18,4),(0x4fb2cc,64)]
class Fault(Exception):pass
def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1024);p.add_argument('--report',type=Path);a=p.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rows=[];expected=[];failures=0
 def owned(at,w):return any(b<=at and at+w<=b+n for b,n in REGIONS)
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def put(at,v,w=4):
  if not owned(at,w):raise Fault()
  r.u.mem_write(at,(v&((1<<(8*w))-1)).to_bytes(w,'little'))
 for seed in range(a.cases):
  mutation=0 if seed%3==0 else(seed*37)&127;fault=(seed//5)%120+1 if seed%5==0 else 0
  for b,n in REGIONS:r.u.mem_write(b,bytes([seed&255])*n)
  put(ACTION+4,11 if seed%11==0 else 13 if seed%2 else 12);put(ACTION+8,(seed//2)%18);put(ACTION+12,0x80000000 if seed%7==0 else 0);put(ACTION+16,(seed//36)%16);put(ACTION+20,0x7fffffff if seed%7==0 else 100);put(ACTION+24,0x80000000 if seed%5==0 else 0);put(ACTION+28,0x7fffffff if seed%5==0 else 100);put(0x8cfa18,0 if seed%13==0 else 0xffffffff if seed%17==0 else 3)
  for i in range(3):
   put(ACTOR+i*0x114+12,0 if(seed+i)%9==0 else PERSON+i*0xb4);put(ACTOR+i*0x114+0x6e,((1 if seed%2 else 2)<<16)|((seed+i)&0xffff))
   if i==1 and seed%3==0:put(ACTOR+i*0x114+0x70,0xffff,2)
   put(ACTOR+i*0x114+0x72,2 if seed%2 else 1,2);put(ACTOR+i*0x114+0x24,101 if i==2 and seed%4==0 else 25+i*20);put(ACTOR+i*0x114+0x28,20+i*20)
  for i in range(16):put(0x4fb2cc+i*4,0x12345678^(i*0x112233))
  rows.append(f'{seed} {mutation} {fault}');trace=1469598103934665603;entries=operations=0;changed=wrote=False;pending=[]
  def emit(tag,at,w,v):
   nonlocal trace,entries
   entries+=1
   for x in(ord(tag),at,w,v):
    for i in range(4):trace=((trace^((x>>(i*8))&255))*1099511628211)&0xffffffffffffffff
  def stop():
   nonlocal operations
   if fault:
    operations+=1
    if operations==fault:raise Fault()
  def after_read(at):
   nonlocal changed
   if not changed and at==ACTOR+12:
    changed=True
    if mutation&1:put(ACTION+4,12 if seed%2 else 13)
    if mutation&2:put(ACTION+8,2)
    if mutation&4:put(0x8cfa18,1)
    if mutation&8:put(ACTION+20,0)
    if mutation&16:put(ACTOR+0x114+12,PERSON)
  def after_write():
   nonlocal wrote
   if not wrote:
    wrote=True
    if mutation&32:put(ACTION+16,0)
    if mutation&64:put(ACTOR+0x114+0x70,2 if seed%2 else 1,2)
  def access(u,kind,at,w,v,user):
   pc=u.reg_read(UC_X86_REG_EIP)
   if not(0x45ff20<=pc<=0x460324):return
   if not owned(at,w):
    if 0x2000000<=at<0x2010000 or 0x460198<=at<0x4601a0 or 0x4ccf0c<=at<0x4ccf10:return
    # Immutable float constants/jump tables retain their real PE bytes.
    if 0x400000<=at<0x500000:return
    raise Fault()
   if kind==unicorn.UC_MEM_READ:emit('R',at,w,get(at,w));after_read(at);stop()
   else:emit('W',at,w,v&((1<<(8*w))-1));pending.extend([after_write,stop])
  def code(u,at,size,user):
   while pending:pending.pop(0)()
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)];failed=False
  try:
   r.call(0x460280,ACTION)
   while pending:pending.pop(0)()
  except(Fault,unicorn.UcError):failed=True;failures+=1
  for h in hooks:r.u.hook_del(h)
  digest=1469598103934665603
  for b,n in REGIONS:
   for v in bytes(r.u.mem_read(b,n)):digest=((digest^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {digest} {trace} {entries}')
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',text=True,capture_output=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y];report={'source_sha256':HASH,'cases':a.cases,'differences':len(diffs),'failure_prefixes':failures,'comparison':'Real460280+45FF20+49BAE6, complete declared current action/actor/person bytes and ordered scalar R/W;masked source x87 control037F;immutable constants, jump tables and stack excluded.','details':diffs[:12]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report));return bool(diffs)
if __name__=='__main__':raise SystemExit(main())
