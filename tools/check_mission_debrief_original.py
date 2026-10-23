#!/usr/bin/env python3
"""Compare the six Debrief state/button bodies with original instructions."""
import argparse,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256,STACK
P=0x1100000;F=0x1300000;T=0x1320000;S=STACK-0x70
CELLS=[0x68bef4,0x503238,0x4cf2e4,0x68bef0,0x4e6d00,0x4e6d04,0x68ba34,0x4e9bf0,0x4e9bdc,0x4e9c1c,0x4e9c18,0xcc08cc,0x8ee1d4]
BODIES=[0x41f390,0x41f510,0x41f570,0x41f5e0,0x41f700,0x420e50]
CALLS={0x407850:0,0x40d550:0,0x40bfd0:1,0x40bca0:1,0x40e4d0:1,0x4980d0:1,0x493a80:3,0x499f1f:1,0x413440:1,0x497f60:4,0x4564b0:1,0x49a620:3,0x41ef40:0,0x41f320:0,0x409180:1,0x41ef80:0,0x40d040:1,0x413460:10,0x422ef0:1,0x4230a0:0,0x40dd80:2,0x40d9e0:0,0x4132f0:1,0x4563a0:0,0x4563b0:0,0x461350:0,0x40d820:0,0x461340:1,0x40fa90:1,0x4133c0:1,0x4230e0:0,0x496c50:0,0x40e2a0:2,0x40b9a0:0}
def run(a):
 r=Rig(a.game);rng=random.Random(0x41f390);active=False;trace=[];pending=[];calls=reads=writes=0;failed=False;owned=set();v=None;rejected=None
 def put(at,n,width=4):r.u.mem_write(at,(n&((1<<(8*width))-1)).to_bytes(width,'little'))
 def get(at,width=4):return int.from_bytes(r.u.mem_read(at,width),'little')
 def normalize(n):return F+n-S if v[0]==0 and S<=n<S+0x64 else n
 def flush(*_):
  while pending:pending.pop(0)()
 def memory(u,kind,at,width,value,_):
  nonlocal reads,writes,failed,rejected
  if not active or not all(at+i in owned for i in range(width)):return
  norm=normalize(at)
  if kind==unicorn.UC_MEM_READ:
   reads+=1
   if reads==v[5]:trace.append(f'F{norm},{width};');failed=True;u.emu_stop();return
   trace.append(f'R{norm},{width},{get(at,width)};')
   if v[3]&256 and at==0x67f724:pending.append(lambda:(put(0x67f728,0x12345678),put(0x4e7544,0x87654321)))
  else:
   writes+=1;n=value&((1<<(8*width))-1)
   if writes==v[6]:rejected=(at,bytes(u.mem_read(at,width)));trace.append(f'E{norm},{width},{n};');failed=True;u.emu_stop();return
   trace.append(f'W{norm},{width},{n};')
   if v[3]&512 and at==0x68bef0:pending.append(lambda:put(at,T+0x20))
 r.u.hook_add(unicorn.UC_HOOK_CODE,flush);r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def callback(site,count):
  def body():
   nonlocal calls,failed
   sp=r.u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4) for i in range(count)];trace.append('C'+str(site)+''.join(','+str(normalize(n)) for n in args)+';');calls+=1
   if calls==v[4]:failed=True;r.u.emu_stop();return
   result=0
   if site==0x40bfd0:result=0x77220000
   elif site==0x40bca0 and v[3]&1:put(0x68bef4,0x77220004)
   elif site==0x4980d0 and v[3]&2:put(0x503238,0x77330004)
   elif site==0x493a80:put(args[0],args[2]&255,1)
   elif site==0x499f1f:
    if v[3]&4:put(0x4cf2e4,P+0x100)
    result=0x77330008
   elif site==0x497f60:result=0 if v[2]&128 else T
   elif site==0x49a620:r.u.mem_write(args[0],bytes([args[1]&255])*args[2])
   elif site==0x41f320 and v[3]&8:put(0x4cf2e4,P+0x100);put(0x68ba34,5)
   elif site==0x40d040:
    if v[3]&16:put(0x68bef0,T+0x20);put(0x4cf2e4,P);put(P+0x28,5)
    result=0x77110000
   elif site==0x413460 and v[3]&32:put(0x4cf2e4,P+0x100);put(P+0x128,4);put(0x68ba34,7)
   elif site in [0x4132f0,0x40fa90] and v[3]&64:put(0x4e9bf0,get(0x4e9bf0)^1)
   elif site in [0x4563a0,0x4563b0] and v[3]&128:put(0xcc08cc,1);put(0x8ee1d4,0x4d0cd4);put(0x4e9bf0,0)
   elif site==0x461350:result=get(0x8ee1d4)
   elif site==0x461340:put(0x8ee1d4,args[0])
   r.finish(result)
  return body
 for site,count in CALLS.items():r.stubs[site]=callback(site,count)
 rows=[]
 for mode in range(6):
  for flags in [0,1,2,4,8,16,24,32,64,128,255]:
   for mutation in [0,1,2,4,8,16,32,64,128,256,512,1023]:rows.append([mode,rng.getrandbits(32),flags,mutation,0,0,0,rng.choice([0,1,2,6,7,8,0xffffffff])])
 for mode in [4,5]:
  for action in [0,1,2,3,4,5,6,7,8,9,0x80000000,0xffffffff]:rows.append([mode,rng.getrandbits(32),0,0,0,0,0,action])
 for _ in range(a.cases):rows.append([rng.randrange(6),rng.getrandbits(32),rng.randrange(256),rng.randrange(1024),rng.choice([0,1,2,5,10,15,20]),rng.choice([0,1,2,10,100,330,500]),rng.choice([0,1,4,100,330]),rng.choice([0,1,2,6,7,8])])
 expected=[]
 for v in rows:
  owned=set();spans=[]
  def reserve(at,n,visible=None):
   block=bytes(((i*37)^(v[1]>>(i%4*8)))&255 for i in range(n));r.u.mem_write(at,block);spans.append((at,n));owned.update(range(at,at+n))
  reserve(0x67f714,0x514);reserve(0x4e7544,0x514);reserve(P,0x200);reserve(S if v[0]==0 else F,0x64);reserve(T,0x2710)
  for at in CELLS:reserve(at,4)
  for at,n in [(P+0x28,1),(P+0x128,2),(0x4cf2e4,P),(0x68bef4,0 if v[2]&1 else 0x77220000),(0x503238,0 if v[2]&2 else 0x77330000),(0x68bef0,0 if v[2]&4 else T),(0x68ba34,3),(0x4e9bf0,(v[2]>>3)&1),(0x4e9bdc,(v[2]>>4)&1),(0x4e9c1c,0x4563a0),(0x4e9c18,0x4563b0),(0xcc08cc,(v[2]>>5)&1),(0x8ee1d4,0x4d0cd4 if v[2]&64 else 0)]:put(at,n)
  trace=[];pending=[];calls=reads=writes=0;failed=False;rejected=None;active=True
  try:r.call(BODIES[v[0]],*([0x12345678,v[7]] if v[0]>=4 else []));flush();result=r.u.reg_read(UC_X86_REG_EAX)
  except RuntimeError:
   if not failed:raise
   result=0
  finally:active=False
  if rejected:r.u.mem_write(*rejected)
  digest=1469598103934665603
  for at,n in sorted(spans,key=lambda span:normalize(span[0])):
   for value in bytes(r.u.mem_read(at,n)):digest=((digest^value)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result if v[0]==1 and not failed else 0} {digest} '+''.join(trace))
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,v)) for v in rows)+'\n',text=True,capture_output=True);assert not p.returncode,p.stderr;actual=p.stdout.splitlines();assert len(actual)==len(expected)
 diffs=[{'case':i,'input':rows[i],'actual':x,'expected':y} for i,(x,y) in enumerate(zip(actual,expected)) if x!=y]
 report={'source_sha256':SOURCE_SHA256,'bodies':[hex(x) for x in BODIES],'cases':len(rows),'differences':len(diffs),'mismatches':diffs[:3],'coverage':'Full result/profile/frame/text/cells; ordered current accesses/calls; entry/profile/menu switches; current render/update gates; nullallocation/errorprefix; buttonactions','foreign_services':'Explicit required fixtures; actual menu assets/StateLoop join is native'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(diffs)}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=300);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
