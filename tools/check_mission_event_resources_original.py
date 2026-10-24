#!/usr/bin/env python3
"""Compare event allocation/load/reset bodies with current-memory foreign fixtures."""
import argparse,hashlib,json,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectile_hits_original import Replay,HASH
REGIONS=[(0x4f2bc4,0x28),(0x6d7e4c,0x8410),(0x1300000,0x6000)]
class Fault(Exception):pass
def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1024);p.add_argument('--report',type=Path);a=p.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rows=[];expected=[];faults=0
 def owned(at,w):return any(b<=at and at+w<=b+n for b,n in REGIONS)
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def put(at,v,w=4):
  if not owned(at,w):raise Fault()
  r.u.mem_write(at,(v&((1<<(w*8))-1)).to_bytes(w,'little'))
 for case in range(a.cases):
  op=case%2;seed=case//2;mutation=0 if case%4==0 else(seed*73)&511;fault=(case//5)%90+1 if case%5==0 else 0
  for b,n in REGIONS:r.u.mem_write(b,bytes([seed&255])*n)
  for i in range(3):put(0x4f2bc8+i*8,3);put(0x4f2bcc+i*8,0x1300100+i*0x2000 if(seed+i)%4 else 0)
  put(0x4f2be0,seed&255,1);rows.append(f'{op} {seed} {mutation} {fault}');trace=1469598103934665603;entries=operations=frees=clears=allocs=reads=count_reads=event_reads=0;fixed=False;pending=[]
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
  def access(u,kind,at,w,v,user):
   nonlocal count_reads,event_reads,fixed
   pc=u.reg_read(UC_X86_REG_EIP)
   if not(0x42ebe0<=pc<=0x42ec82 or 0x42ec90<=pc<=0x42edc0):return
   if not owned(at,w):
    if 0x2000000<=at<0x2010000:return
    raise Fault()
   if kind==unicorn.UC_MEM_READ:
    emit('R',at,w,get(at,w))
    if at==0x4f2bc8:
     count_reads+=1
     if count_reads==2 and mutation&64:pending.append(lambda:put(at,1))
    if at==0x4f2bcc:
     event_reads+=1
     if event_reads==3 and mutation&256:pending.append(lambda:put(at,0x1301000))
   else:
    emit('W',at,w,v&((1<<(w*8))-1))
    if at==0x4f2bdc and mutation&16:pending.append(lambda:put(0x4f2bcc,0x1300000))
    if not fixed and 0x1300000<=at<0x1302000:
     fixed=True
     if mutation&32:pending.append(lambda:put(0x4f2bcc,0x1301000))
   pending.append(stop)
  sites={0x4980d0:1,0x49a620:3,0x499280:2,0x497f60:4,0x42eea0:0,0x42f020:0}
  def code(u,at,size,user):
   nonlocal frees,clears,allocs,reads
   while pending:pending.pop(0)()
   if at not in sites:return
   n=sites[at];aa=list(r.read(u.reg_read(UC_X86_REG_ESP)+4,n))if n else[];emit('C',at,n,0)
   for v in aa:emit('A',0,0,v)
   stop();result=0
   if at==0x4980d0:
    frees+=1
    if frees==1 and mutation&1:put(0x4f2bd4,0)
    result=0xffffffff
   elif at==0x49a620:
    for i in range(aa[2]):put(aa[0]+i,aa[1],1)
    clears+=1
    if clears==1 and mutation&2:put(0x6e0250,0xffffffff);put(0x6e0254,0xffffffff)
    result=aa[0]
   elif at==0x497f60:
    allocs+=1
    if allocs==1 and mutation&4:put(0x4f2bd0,7);put(0x4f2bd8,8)
    if allocs==3 and mutation&8:put(0x4f2bcc,0x1301000)
    result=0 if seed%[11,13,17][allocs-1]==0 else 0x1300000+(allocs-1)*0x2000
   elif at==0x499280:
    reads+=1
    if reads<=3:put(aa[0],seed%4 if reads==1 else 3)
    else:
     for i in range(aa[1]):put(aa[0]+i,(seed+i)&255,1)
     if reads==4:
      for row in range(seed%4):put(aa[0]+row*24+4,row);put(aa[0]+row*24+8,2-row)
    result=aa[1]
   elif at==0x42eea0 and mutation&128:put(0x4f2be0,0xa4,1)
   u.reg_write(UC_X86_REG_EAX,result)
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)]
  for site in sites:r.hook(site,lambda:None)
  failed=False
  try:
   r.call(0x42ec90 if op else 0x42ebe0)
   while pending:pending.pop(0)()
  except(Fault,unicorn.UcError):failed=True;faults+=1
  for h in hooks:r.u.hook_del(h)
  r.clear_hooks();digest=1469598103934665603
  for b,n in REGIONS:
   for byte in bytes(r.u.mem_read(b,n)):digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {digest} {trace} {entries}')
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',capture_output=True,text=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y];report={'source_sha256':HASH,'bodies':['42EBE0','42EC90'],'cases':a.cases,'differences':len(diffs),'failure_prefixes':faults,'comparison':'Complete declared globals/flag arrays/payload fixtures and ordered R/W/call tuple FNV64+count. Real reset and load parents execute. Current-file/allocator/free/clear/marker children are declared fixtures; CPU stack/registers/flags excluded.','details':diffs[:4]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report));return bool(diffs)
if __name__=='__main__':raise SystemExit(main())
