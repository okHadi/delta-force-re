#!/usr/bin/env python3
"""Replay seven original leave-raster bodies with declared backend callbacks."""
import argparse,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256
D=0x2007fd8;O=0x1300000;P=0x1100000

def run(a):
 D=0x2007fd8;r=Rig(a.game);rng=random.Random(0x4719f0);rows=[]
 for body in range(7):
  for mode in [0,1,2,0xffffffff]:
   for hit in [0,1]:
    for mutation in [0,1,2,4,8,15,16,32,64,128,255]:
     rows.append([body,4,3,16,32,mode,hit,mutation,0])
 for stride,bits in [(0,0),(0,32),(0x10000000,0xffffffff),(0xfffffffc,32),(4,8)]:
  for height in [0,1,3,0xffffffff]:rows.append([2,4,height,stride,bits,1,0,0,0])
 for fail in range(1,85):
  for body in range(5):rows.append([body,4,3,16,32,1,0,15,fail])
 for _ in range(a.cases):rows.append([rng.randrange(7),rng.choice([0,1,4]),rng.choice([0,1,3,0xffffffff]),rng.choice([0,4,16]),rng.choice([0,8,32]),rng.randrange(4),rng.randrange(2),rng.randrange(256),rng.choice([0,0,rng.randrange(1,65)])])
 active=False;pending=[];trace=[];words={};count=0;failed=False;rejected=None;v=None
 def put(at,n):r.u.mem_write(at,(n&0xffffffff).to_bytes(4,'little'))
 def get(at):return int.from_bytes(r.u.mem_read(at,4),'little')
 def flush(*_):
  while pending:pending.pop(0)()
 def step():
  nonlocal count,failed
  count+=1
  if count==v[8]:failed=True;r.u.emu_stop();return False
  return True
 def memory(u,kind,at,width,value,_):
  nonlocal rejected
  if not active or at not in words:return
  if not step():
   if kind==unicorn.UC_MEM_WRITE:rejected=(at,bytes(u.mem_read(at,width)))
   return
  if kind==unicorn.UC_MEM_READ:
   n=get(at);trace.append(f'R{at},{width},{n};')
   if v[7]&1 and at==0x50130c:pending.append(lambda:put(at,P+0x100))
   if v[7]&4 and at==0xd092c0:pending.append(lambda:put(0xd09308,0x1200300))
  else:
   trace.append(f'W{at},{width},{value&0xffffffff};')
   if v[7]&2 and at==O+4:pending.append(lambda:put(D+8,(v[3]+4)&0xffffffff))
 def backend(site):
  if not step():return
  sp=r.u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(1 if site==0x456e30 else 0 if site==0x1200200 else 2)]
  trace.append('C'+str(site)+''.join(','+str(n)for n in args)+';')
  if site==0x456e30:put(0x4f8718,args[0])
  if v[7]&8 and site==0x1200200:put(0xd0930c,0x1200100)
  r.finish(0)
 for site in [0x456e30,0x1200100,0x1200200,0x1200300]:r.stubs[site]=lambda site=site:backend(site)
 r.u.hook_add(unicorn.UC_HOOK_CODE,flush);r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory);expected=[]
 for v in rows:
  body,width,height,stride,bits,mode,hit,mutation,fail=v;D=0x2007fd8 if body==0 else 0x1210000
  words={0x503138:mode,0x50130c:P,0xd092c0:width,0xd092c4:height,0x4f8718:2,0xcc0884:0 if mutation&16 else 1,0xd09300:0 if mutation&32 else 1,0xd09308:0 if mutation&64 else 0x1200100,0xd0924c:0x1200200,0xd0930c:0 if mutation&128 else 0x1200300,0xd0932c:0,0xd09330:0}
  for i,n in enumerate([width,height,stride,P,0x503740,bits]):words[D+i*4]=n
  for i in range(8):words[O+4+i*4]=0;words[O+0xc08+i*4]=0
  words.update({O:P if hit else 0,O+0xc04:0x503740 if hit else 0,O+0x1808:width if hit else 0,O+0x180c:height if hit else 0,O+0x1810:stride if hit else 0,O+0x1824:8})
  for at,n in words.items():put(at,n)
  trace=[];pending=[];count=0;failed=False;rejected=None;active=True
  sites=[0x404540,0x404610,0x4719f0,0x496c70,0x496c50,0x496620,0x496630];args=[[O],[],[O,D],[width,height],[],[],[]]
  try:r.call(sites[body],*args[body]);flush()
  except (RuntimeError,unicorn.UcError):failed=True
  active=False
  if rejected:r.u.mem_write(*rejected)
  result=r.u.reg_read(UC_X86_REG_EAX)if not failed and body in [0,5,6]else 0
  expected.append(f'{int(failed)} {result} '+''.join(f'{at}:{get(at)},'for at in sorted(words))+' '+''.join(trace))
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,v))for v in rows)+'\n',text=True,capture_output=True);assert not p.returncode,p.stderr;actual=p.stdout.splitlines();assert len(actual)==len(expected)
 diff=[{'case':i,'input':rows[i],'actual':x,'expected':y}for i,(x,y)in enumerate(zip(actual,expected))if x!=y]
 a.report.write_text(json.dumps({'sha256':SOURCE_SHA256,'bodies':['404540','404610','4719F0','496620','496630','496C50','496C70'],'cases':len(rows),'differences':len(diff),'mismatches':diff[:3],'scope':'Exact memory and declared profiler/backend call traces; stable scalar args; full current descriptor and raster fields, pixel pointer capture, callback mutations, cache keys/bits exclusion, signed divide faults, failure prefixes'},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(diff)}));assert not diff
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=300);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
