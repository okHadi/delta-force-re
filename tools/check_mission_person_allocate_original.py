#!/usr/bin/env python3
"""Compare 44A0C0 instructions, current pool, clear and owner-store failures."""
import argparse,json,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256
class FixtureFault(Exception):pass
def fixture(seed):
 d={};free=[0,1,17,1279,1280][(seed>>4)%5]
 for i in range(1280):d[0x794680+i*0xb4]=(4,0 if i==free else 0x7cca80)
 if free<1280:
  for off in [4,0x24,0x54,0x6c,0xb0]:d[0x794680+free*0xb4+off]=(4,0x12345678+off)
 return [0,0x7cca80,0xffffffff,0x80000000][(seed>>8)%4],d
def run(a):
 rig=Rig(a.game);u=rig.u;active=False;trace=[];seed=0;pending=False
 def get(at,w=4):return int.from_bytes(u.mem_read(at,w),'little')
 def put(at,w,n):u.mem_write(at,(n&((1<<(8*w))-1)).to_bytes(w,'little'))
 def memory(m,kind,at,w,n,_):
  nonlocal pending
  if not active or not 0x44a0c0<=m.reg_read(UC_X86_REG_EIP)<=0x44a102 or 0x2000000<=at<0x2010000:return
  if kind==unicorn.UC_MEM_READ:trace.append(f'R{at},{w},{get(at,w)};')
  else:
   trace.append(f'W{at},{w},{n&((1<<(8*w))-1)};')
   if seed&2:pending=True
 def flush(m,at,size,_):
  nonlocal pending
  if pending:pending=False;raise FixtureFault()
 def clear():
  sp=u.reg_read(UC_X86_REG_ESP);args=list(struct.unpack('<3I',u.mem_read(sp+4,12)));trace.append('C'+','.join(map(str,[0x49a620,*args]))+';');selected=args[0];u.mem_write(selected,b'\0'*args[2])
  if seed&4:put(selected,4,0xabcdef01)
  if seed&8:put(0x794680,4,0)
  if seed&1:raise FixtureFault()
  rig.finish(selected)
 rig.stubs[0x49a620]=clear;u.hook_add(unicorn.UC_HOOK_CODE,flush);u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 seeds=list(dict.fromkeys(list(range(a.cases))+[base+variant for base in [0,16,32,48,64,256,512,768] for variant in range(16)]));payload=[];expected=[]
 for seed in seeds:
  u.mem_write(0x794680,b'\0'*(1280*0xb4));owner,d=fixture(seed)
  for at,(w,n) in d.items():put(at,w,n)
  trace=[];pending=False;failed=False;result=0;active=True
  try:rig.call(0x44a0c0,owner);result=u.reg_read(UC_X86_REG_EAX)
  except FixtureFault:failed=True
  finally:active=False
  trace.append(f'F{int(failed)};O{result};')
  for at,(w,n) in d.items():trace.append(f'V{at},{w},{get(at,w)};')
  h=1469598103934665603
  for n in u.mem_read(0x794680,1280*0xb4):h=((h^n)*1099511628211)&0xffffffffffffffff
  trace.append(f'H{h};');expected.append(''.join(trace));payload.append(' '.join(map(str,[seed,owner,len(d),*[v for at,(w,n) in d.items() for v in [at,w,n]]])))
 proc=subprocess.run([str(a.probe)],input='\n'.join(payload)+'\n',text=True,capture_output=True);assert proc.returncode==0,proc.stderr;actual=proc.stdout.splitlines();assert len(actual)==len(expected)
 diffs=[{'seed':s,'actual':x,'expected':y} for s,x,y in zip(seeds,actual,expected) if x!=y];report={'source_sha256':SOURCE_SHA256,'cases':len(seeds),'differences':len(diffs),'mismatches':diffs[:2],'nested_services':'Explicit mutable clear fixture','audio_output':'none'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
