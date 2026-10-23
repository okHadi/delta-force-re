#!/usr/bin/env python3
"""Compare complete486660 cleanup chain over one shared image."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectile_hits_original import Replay,HASH,STACK
B,Z=0xc54480,0xc8ff50
REGIONS=[(B,116),(0xc65dc8,8),(0xc65dd0,368),(0xc80d10,372)]
class Fault(Exception):pass

def main():
 parser=argparse.ArgumentParser();parser.add_argument('game',type=Path);parser.add_argument('probe',type=Path);parser.add_argument('--cases',type=int,default=512);parser.add_argument('--report',type=Path);a=parser.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rng=random.Random(0x486660);rows=[];expected=[];faults=0;sites_count={};aliases=0
 def read(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def put(at,value,w=4):r.u.mem_write(at,(value&((1<<(w*8))-1)).to_bytes(w,'little'))
 def parent():
  pc=r.u.reg_read(UC_X86_REG_EIP);return any(lo<=pc<=hi for lo,hi in [(0x486660,0x48666e),(0x485b34,0x485b94),(0x485dd2,0x485e07),(0x485d71,0x485d9a),(0x485d50,0x485d64),(0x486510,0x486524)])
 for case in range(a.cases):
  mutation=0 if case%3 else rng.randrange(512)|(rng.randrange(8)<<10)
  r.u.mem_write(B,bytes(Z-B))
  for b,n in REGIONS:r.u.mem_write(b,rng.randbytes(n))
  put(B,[0,1,2,0xffffffff,0x80000000][case%5]);put(0xc65dcc,[0,1,2,0xffffffff,0x80000000][(case//5)%5]);put(0xc80d10,0x12345678)
  for i in range(3):put(B+8+i*36+0x16,128 if (case+i)%3==0 else 0,1);put(B+8+i*36+0x1c,0 if (case+i)%4==0 else 0x1300000+i*16)
  for i in range(2):put(0xc65dd0+i*0xb8+0x18,0 if (case+i)%4==0 else 0x1300100+i*16)
  if case%29==0:
   mutation=0;put(B,0);put(0xc65dcc,601);put(0xc80d28,0x1300600);aliases+=1
  rows.append(f'{mutation} '+b''.join(bytes(r.u.mem_read(b,n))for b,n in REGIONS).hex());trace=[];pending=[];calls=stores=0
  def access(u,kind,at,width,value,user):
   nonlocal stores
   if not parent():return
   if not B<=at or at+width>Z:return
   if kind==unicorn.UC_MEM_READ:
    value=read(at,width);trace.append(f'R{at},{width},{value};')
    if mutation&1 and at==B+0x24:pending.append(lambda:put(B+0x24,0x1300090))
    if mutation&256 and at==0xc65dcc:pending.append(lambda:put(0xc65dcc,0))
   else:
    value&=(1<<(width*8))-1;trace.append(f'W{at},{width},{value};');stores+=1
    if mutation&2 and at==B+0x24:pending.append(lambda:put(B,2))
    if mutation&16 and stores==((mutation>>10)%8+1):pending.append(lambda:(_ for _ in ()).throw(Fault()))
  def code(u,at,size,user):
   nonlocal calls
   while pending:pending.pop(0)()
   if at not in (0x4980d0,0x49a620):return
   aa=list(r.read(u.reg_read(UC_X86_REG_ESP)+4,1 if at==0x4980d0 else 3));trace.append('C'+str(at)+''.join(','+str(v)for v in aa)+';');calls+=1;sites_count[hex(at)]=sites_count.get(hex(at),0)+1
   if mutation&32 and calls==((mutation>>10)%8+1):raise Fault()
   if at==0x4980d0:
    if mutation&4:put(0xc65dcc,2)
    if mutation&64:put(B+0x24,0x1300080)
   else:
    if aa[0]<B or aa[0]+aa[2]>Z:raise Fault()
    r.u.mem_write(aa[0],bytes([aa[1]&255])*aa[2])
    if aa[0]==0xc65dd0:
     if mutation&8:put(0xc65dcc,1)
     if mutation&128:put(0xc65dd0+0xb8+0x18,0x1300200);put(0xc65dcc,2)
   u.reg_write(UC_X86_REG_EAX,0)
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)]
  for site in (0x4980d0,0x49a620):r.hook(site,lambda:None)
  failed=False
  try:
   r.call(0x486660)
   while pending:pending.pop(0)()
  except(Fault,unicorn.UcError):failed=True;faults+=1
  for hook in hooks:r.u.hook_del(hook)
  r.clear_hooks();digest=1469598103934665603
  for byte in bytes(r.u.mem_read(B,Z-B)):digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {digest} '+''.join(trace))
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',capture_output=True,text=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y];report={'source_sha256':HASH,'real_bodies':['486660','485B30','485DD0','485D70','485D50','486510'],'cases':a.cases,'differences':len(diffs),'fault_prefixes':faults,'count601_alias_cases':aliases,'calls':sites_count,'comparison':'Complete one physicalimage C54480..C8FF50 and orderedownedchain accesses/foreigncalls; currentcounts/pointer/null/shared branches and failures. Realalias slot600 atC80D10 overlapsglobals/graphicregistry. Release/memset effects explicitfixtures. CPUregisters/stack excluded.','details':diffs[:1]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items()if k!='details'}))
 if diffs:
  d=diffs[0];x=d['original'].split(' ',2);y=d['port'].split(' ',2);print('first',d['case'],x[:2],y[:2]);xx=x[-1].split(';');yy=y[-1].split(';')
  for i,(v,z)in enumerate(zip(xx,yy)):
   if v!=z:print('trace',i,xx[max(0,i-3):i+4],yy[max(0,i-3):i+4]);break
  return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
