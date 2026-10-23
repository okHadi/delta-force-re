#!/usr/bin/env python3
"""Compare 45D520 source instruction order and complete physical actor pool."""
import argparse,json,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256
class FixtureFault(Exception):pass
def fixture(seed):
 d={}
 def put(at,n,w=4):d[at]=(w,n&((1<<(w*8))-1))
 put(0x8cf97c,[0,2,0xffffffff][(seed>>10)%3]);put(0x8cf982,0x10 if seed&1024 else 0,1);put(0xc23810,0x1300000);put(0x67f549,seed&1,1);put(0x8cfa18,[0,1,255,256,0xffffffff,0x80000000][(seed>>12)%6])
 free=[0,1,255,256][(seed>>6)%4]
 for i in range(256):put(0x7cca80+i*0x114,0 if i==free else 9)
 for base in [0x1300000,0x1301000,0x1302000]:
  for i in range(3):
   for offset,n in [(0x6c,0x11110000+i),(0x74,0x22220000+i),(0x84,0x33330000+i),(0x90,0x44440000+i),(0x94,[0,0x8000,0xffff][i])]:put(base+i*0x13c+offset,n)
 return [0,7,0xffffffff][(seed>>8)%3],[0,1,0xffffffff][(seed>>11)%3],d
def run(a):
 rig=Rig(a.game);u=rig.u;active=False;seed=0;actor=definition=0;trace=[];pending=[]
 def get(at,w=4):return int.from_bytes(u.mem_read(at,w),'little')
 def put(at,w,n):u.mem_write(at,(n&((1<<(w*8))-1)).to_bytes(w,'little'))
 def memory(m,kind,at,w,n,_):
  if not active or not 0x45d520<=m.reg_read(UC_X86_REG_EIP)<=0x45d6c7 or 0x2000000<=at<0x2010000:return
  if kind==unicorn.UC_MEM_READ:trace.append(f'R{at},{w},{get(at,w)};')
  else:
   trace.append(f'W{at},{w},{n&((1<<(w*8))-1)};')
   if at==actor+0x14 and seed&2:pending.append((definition+0x6c,4,0xaabbccdd))
   if at==actor+0x18 and seed&4:pending.append((definition+0x84,4,0x45454545))
 def flush(m,at,size,_):
  while pending:
   target,w,n=pending.pop(0);put(target,w,n)
 def foreign(site):
  nonlocal actor,definition
  sp=u.reg_read(UC_X86_REG_ESP);count=3 if site==0x49a620 else 1;args=list(struct.unpack('<'+'I'*count,u.mem_read(sp+4,count*4)));trace.append('C'+','.join(map(str,[site,*args]))+';')
  if site==0x4828c0:
   if seed&1:put(0xc23810,4,0x1301000)
   result=(seed>>16)%3
  elif site==0x49a620:
   actor=args[0];definition=get(0xc23810)+((seed>>16)%3)*0x13c;u.mem_write(actor,b'\0'*0x114);result=actor
   if seed&8:put(0x8cfa18,4,99)
  elif site==0x42cce0:
   if seed&16:raise FixtureFault()
   if seed&32:put(0xc23810,4,0x1302000)
   if seed&64:put(definition+0x94,2,0xffff)
   result=0x55550000+seed
  else:
   if seed&128:raise FixtureFault()
   if seed&256:put(actor+0x18,4,0x66660000)
   if seed&512:put(0x67f549,1,1)
   result=0x794680
  rig.finish(result)
 for site in [0x4828c0,0x49a620,0x42cce0,0x44a0c0]:rig.stubs[site]=lambda site=site:foreign(site)
 u.hook_add(unicorn.UC_HOOK_CODE,flush);u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 seeds=list(dict.fromkeys(list(range(a.cases))+[65536*i+v for i in range(3) for v in [0,1,64,128,192,1024,2048,4096,8192,12288,16384,20480]]));payload=[];expected=[]
 for seed in seeds:
  for at,size in [(0x7cca80,256*0x114),(0x1300000,0x3000)]:u.mem_write(at,b'\0'*size)
  face,side,d=fixture(seed)
  for at,(w,n) in d.items():put(at,w,n)
  trace=[];pending=[];actor=definition=0;failed=False;result=0;active=True
  try:rig.call(0x45d520,face,side);result=u.reg_read(UC_X86_REG_EAX)
  except FixtureFault:failed=True
  finally:active=False
  trace.append(f'F{int(failed)};O{result};')
  for at,(w,n) in d.items():trace.append(f'V{at},{w},{get(at,w)};')
  h=1469598103934665603
  for n in u.mem_read(0x7cca80,256*0x114):h=((h^n)*1099511628211)&0xffffffffffffffff
  trace.append(f'H{h};');expected.append(''.join(trace));payload.append(' '.join(map(str,[seed,face,side,len(d),*[v for at,(w,n) in d.items() for v in [at,w,n]]])))
 proc=subprocess.run([str(a.probe)],input='\n'.join(payload)+'\n',text=True,capture_output=True);assert proc.returncode==0,proc.stderr;actual=proc.stdout.splitlines();assert len(actual)==len(expected)
 diffs=[{'seed':s,'actual':x,'expected':y} for s,x,y in zip(seeds,actual,expected) if x!=y];report={'source_sha256':SOURCE_SHA256,'cases':len(seeds),'differences':len(diffs),'mismatches':diffs[:2],'nested_services':'Mutable definition/clear/CHR/person fixtures','audio_output':'none'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=512);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
