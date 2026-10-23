#!/usr/bin/env python3
"""Compare the four Post Menu bodies with the supplied executable."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectile_hits_original import Replay,HASH
CELLS=[0x67dbbc,0x67f520,0x67f528,0x67f534,0x67f53c,0x68d530,0x4e9bf0,0x4e9bdc,0x4e9c18,0x50322c,0x1300000]
SITES={0x407850:0,0x40d550:0,0x40bfd0:1,0x40bca0:1,0x40e4d0:1,0x422ef0:1,0x4230a0:0,0x40dd80:2,0x42af00:0,0x40b960:0,0x461340:1,0x4c4f40:2,0x41e2e0:0,0x49a092:3,0x401d00:2,0x407fd0:0,0x40fa90:1,0x1300200:0,0x4230e0:0,0x496c50:0}
RANGES=[(0x424540,0x4245a7),(0x4245b0,0x424724),(0x424730,0x42477c),(0x424780,0x424781)]
class FixtureFault(Exception):pass
def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=2048);p.add_argument('--report',type=Path);a=p.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rng=random.Random(0x4245b0);rows=[];expected=[];faults=0
 for case in range(a.cases):
  op=case%4;mutation=rng.randrange(1024);row=[op,mutation,(case//4)%10 if case%7==0 else 0,1 if case%11==0 else 0,(case//4)%13 if case%9==0 else 0,case//4%8,rng.randrange(2),rng.randrange(2),rng.randrange(4),rng.randrange(2),0 if case%5==0 else 0x1300100,rng.randrange(2),rng.randrange(2),0x1300200,0x1300300,0x1300400];rows.append(' '.join(map(str,row)))
  for at,value in zip(CELLS,row[5:]):r.write(at,value)
  trace=[];pending=[];reads=writes=calls=0
  def parent():return any(lo<=r.u.reg_read(UC_X86_REG_EIP)<hi for lo,hi in RANGES)
  def access(u,kind,at,width,value,user):
   nonlocal reads,writes
   if not parent() or at not in CELLS:return
   assert width==4
   if kind==unicorn.UC_MEM_READ:
    value=r.read(at)[0];trace.append(f'R{at},4,{value};');reads+=1
    if reads==row[2]:raise FixtureFault()
   else:
    trace.append(f'W{at},4,{value};');writes+=1
    if writes==row[3]:raise FixtureFault()
    if mutation&512:pending.append(lambda at=at:r.write(at,r.read(at)[0]^0x40))
  def code(u,at,size,user):
   nonlocal calls
   while pending:pending.pop(0)()
   if at not in SITES:return
   args=list(r.read(r.u.reg_read(UC_X86_REG_ESP)+4,SITES[at]));trace.append('C'+str(at)+''.join(','+str(v)for v in args)+';');calls+=1
   for mask,address,value in [(1,0x67dbbc,3),(2,0x67f520,1),(4,0x67f528,1),(16,0x67f53c,1),(32,0x68d530,4),(64,0x4e9bf0,1),(128,0x4e9bdc,1),(256,0x50322c,0x40)]:
    if mutation&mask:r.write(address,r.read(address)[0]^value)
   if mutation&8:r.write(0x67f534,(r.read(0x67f534)[0]+1)%4)
   if calls==row[4]:raise FixtureFault()
   r.u.reg_write(UC_X86_REG_EAX,0x1300100 if at==0x40bfd0 else 0x1300000 if at==0x49a092 else 0)
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)]
  for site in SITES:r.hook(site,lambda:None)
  failed=False;result=0
  try:
   result=r.call(RANGES[op][0]);result=result if op==1 else 0
   while pending:pending.pop(0)()
  except (FixtureFault,unicorn.UcError):failed=True;faults+=1
  for hook in hooks:r.u.hook_del(hook)
  r.clear_hooks();digest=1469598103934665603
  for at in CELLS:
   for byte in bytes(r.u.mem_read(at,4)):digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {0 if failed else result} {digest} '+''.join(trace))
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',capture_output=True,text=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y];report={'source_sha256':HASH,'cases':a.cases,'differences':len(diffs),'fault_prefixes':faults,'bodies':['424540','4245B0','424730','424780'],'comparison':'Ordered parent reads/stores/calls, current cells, captured arguments, callback mutations and failures. Foreign services use explicit fixtures.','details':diffs[:1]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items()if k!='details'}))
 if diffs:print(json.dumps(diffs[0]));return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
