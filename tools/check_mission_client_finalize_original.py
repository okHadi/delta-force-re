#!/usr/bin/env python3
"""Compare current-client 440640 source order and mutable nested boundaries."""
import argparse,json,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
from check_projectiles_original import Rig,SOURCE_SHA256
class FixtureFault(Exception):pass
def fixture(seed):
 d={}
 def put(at,n,w=4):d[at]=(w,n&((1<<(w*8))-1))
 put(0x67f528,0 if seed&1024 else 1)
 for i in range(17):
  c=0x6e9a90+i*0x9b84;put(c,77+i);put(c+8,0);put(c+0x18c,1 if i==0 or seed&2048 and i==16 else 0,1);put(c+0xa0,i);put(c+0xa4,i+10);put(c+0x9c,0x1234+i,2)
  for j,n in enumerate(b'initial\0'):put(c+0x0c+j,n,1)
 for i in range(3):put(0x7cca80+i*0x114+0x0c,0x794680)
 return d
def run(a):
 rig=Rig(a.game);u=rig.u;active=False;trace=[];seed=0;client=0
 def get(at,w=4):return int.from_bytes(u.mem_read(at,w),'little')
 def put(at,w,n):u.mem_write(at,(n&((1<<(8*w))-1)).to_bytes(w,'little'))
 def memory(m,kind,at,w,n,_):
  if not active or not 0x440640<=m.reg_read(UC_X86_REG_EIP)<=0x4406d8 or 0x2000000<=at<0x2010000:return
  if kind==unicorn.UC_MEM_READ:
   if at<0x1000:raise FixtureFault()
   trace.append(f'R{at},{w},{get(at,w)};')
  else:
   trace.append(f'W{at},{w},{n&((1<<(8*w))-1)};')
   if at<0x1000:return
   if at==client+8 and seed&4:pending.append((at,4,0x7cca80+0x114))
   if at==0x7cca80+8 and seed&8:pending.append((client+0x9c,2,0x5678))
 pending=[]
 def flush(m,at,size,_):
  while pending:
   address,w,n=pending.pop(0);put(address,w,n)
 def foreign(site):
  nonlocal client
  sp=u.reg_read(UC_X86_REG_ESP);args=list(struct.unpack('<'+'I'*(2 if site==0x45d520 else 1),u.mem_read(sp+4,8 if site==0x45d520 else 4)));trace.append('C'+','.join(map(str,[site,*args]))+';')
  if site==0x45d520:
   client=0x6e9a90+(args[0]%17)*0x9b84
   if seed&1:put(client,4,88)
   if seed&2:put(0x6e9a90+0x9b84+0x18c,1,1)
   rig.finish(0 if seed&512 else 0x7cca80)
  else:
   if seed&16:put(client+8,4,0x7cca80+2*0x114)
   if seed&32:put(get(client+8)+0x0c,4,0x794680+0xb4)
   if seed&64:u.mem_write(client+0x0c,b'current\0')
   if seed&128:put(0x6e9a90+0x9b84+0x18c,1,0)
   if seed&256:raise FixtureFault()
   rig.finish(0)
 for site in [0x45d520,0x43f610]:rig.stubs[site]=lambda site=site:foreign(site)
 def invalid(m,kind,at,w,n,_):
  if active and at<0x1000:
   if kind==unicorn.UC_MEM_WRITE_UNMAPPED:
    entry=f'W{at},{w},{n&((1<<(8*w))-1)};'
    if not trace or trace[-1]!=entry:trace.append(entry)
   return False
  raise RuntimeError(f'Unowned original access seed{seed} at{at:x} kind{kind} ip{m.reg_read(UC_X86_REG_EIP):x} trace{trace[-8:]}')
 u.hook_add(unicorn.UC_HOOK_MEM_INVALID,invalid)
 u.hook_add(unicorn.UC_HOOK_CODE,flush);u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 seeds=list(dict.fromkeys(list(range(a.cases))+[1<<i for i in range(12)]+[2048+v for v in [0,1,4,16,32,64,128,256,512]]));payload=[];expected=[]
 for seed in seeds:
  for at,size in [(0x6e9a90,17*0x9b84),(0x7cca80,3*0x114),(0x794680,2*0xb4)]:u.mem_write(at,b'\0'*size)
  d=fixture(seed)
  for at,(w,n) in d.items():put(at,w,n)
  trace=[];pending=[];client=0;failed=False;active=True
  try:rig.call(0x440640)
  except FixtureFault:failed=True
  except unicorn.UcError:
   if seed&512 and u.reg_read(UC_X86_REG_EIP)==0x440688 and trace[-1].startswith('W8,4,'):failed=True
   else:raise
  finally:active=False
  trace.append(f'F{int(failed)};')
  for at,(w,n) in d.items():trace.append(f'V{at},{w},{get(at,w)};')
  for n in u.mem_read(0x7cca80,3*0x114):trace.append(f'A{n},')
  for n in u.mem_read(0x794680,2*0xb4):trace.append(f'P{n},')
  expected.append(''.join(trace));payload.append(' '.join(map(str,[seed,len(d),*[v for at,(w,n) in d.items() for v in [at,w,n]]])))
 proc=subprocess.run([str(a.probe)],input='\n'.join(payload)+'\n',text=True,capture_output=True);assert proc.returncode==0,proc.stderr;actual=proc.stdout.splitlines();assert len(actual)==len(expected)
 diffs=[{'seed':s,'actual':x,'expected':y} for s,x,y in zip(seeds,actual,expected) if x!=y];report={'source_sha256':SOURCE_SHA256,'cases':len(seeds),'differences':len(diffs),'mismatches':diffs[:2],'nested_services':'Explicit mutable45D520 and43F610 fixtures','audio_output':'none'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
