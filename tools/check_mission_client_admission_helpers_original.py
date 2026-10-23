#!/usr/bin/env python3
"""Compare admission helper byte accesses, substitutions and transport dispatch."""
import argparse,json,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
from check_projectiles_original import Rig,SOURCE_SHA256
BODIES=[0x442450,0x43f770,0x497870]
class FixtureFault(Exception):pass
def fixture(op,seed):
 d={}
 def put(at,value,w=4):d[at]=(w,value&((1<<(w*8))-1))
 def text(at,value):
  for i,n in enumerate(value+b'\0'):put(at+i,n,1)
 if op==0:
  slot=[0,1,2,16,17,0xffffffff][(seed>>6)%6];arg1=(0x6e9a90+slot*0x9b84)&0xffffffff;arg2=0
  if seed&64:arg1=(arg1+1)&0xffffffff
  for i in range(17):
   put(0x6e9c1c+i*0x9b84,0 if i%3==1 else 1,1)
   for j in range(17):put(0x6e9c60+i*0x9b84+j,(i+j)%18,1)
  put(0x6e9c60,slot&255,1)
 elif op==1:
  arg1,arg2=0x1300100,0x1300200
  formats=[b'$A / $A',b'',b'a',b'$',b'$B',b'$a',b'$A'*64,b'A'*128,b'A'*129,b'$A$A']
  names=[b'Bob',b'',b'x'*16,b'x'*17,b'q']
  text(arg1,formats[(seed>>6)%len(formats)]);text(arg2,names[(seed>>10)%len(names)])
  alias=(seed>>14)%5
  if alias==1:arg1=0x78ee54;text(arg1,b'$A')
  elif alias==2:arg2=0x78ee54;text(arg2,b'Bob')
  elif alias==3:arg1=0x78ee5c;text(arg1,b'$A')
  elif alias==4:arg2=0x78ee5c;text(arg2,b'q')
  if seed&0x80000:arg2=0
 else:
  arg1,arg2=77+(seed>>6),0;put(0x501398,0x1300300);put(0x130036c,0x410000)
 return arg1,arg2,d
def run(a):
 rig=Rig(a.game);u=rig.u;active=False;trace=[];seed=0;name_reads=0;reference_reads=0
 def get(at,w=4):return int.from_bytes(u.mem_read(at,w),'little')
 def put(at,w,n):u.mem_write(at,(n&((1<<(w*8))-1)).to_bytes(w,'little'))
 def owned():
  ip=u.reg_read(UC_X86_REG_EIP);return 0x442450<=ip<=0x4424ac or 0x43f770<=ip<=0x43f82f or 0x497870<=ip<=0x497880
 def memory(m,kind,at,w,n,_):
  nonlocal name_reads,reference_reads
  if not active or not owned() or 0x2000000<=at<0x2010000:return
  if kind==unicorn.UC_MEM_READ:
   if at==0x1300100:
    if name_reads==0 and seed&1:put(at+1,1,ord('B'))
    name_reads+=1
   if at==0x6e9c60:
    if reference_reads==1 and seed&4:put(at,1,0)
    reference_reads+=1
   value=get(at,w);trace.append(f'R{at},{w},{value};')
   if at==0x501398 and seed&8:put(value+0x6c,4,0x410100)
  else:
   trace.append(f'W{at},{w},{n&((1<<(w*8))-1)};')
   if at==0x78ee55 and seed&2:put(0x1300200,1,ord('Z'))
   if at==0x78ee56 and seed&16:
    put(at,w,n);raise FixtureFault()
 def foreign(site):
  sp=u.reg_read(UC_X86_REG_ESP);peer=get(sp+4);trace.append(f'C{site},{peer};')
  if seed&32:raise FixtureFault()
  rig.finish(site+peer)
 for site in [0x410000,0x410100]:rig.stubs[site]=lambda site=site:foreign(site)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 seeds=list(dict.fromkeys(list(range(a.cases))+[64*i+7 for i in range(10)]+[1024*i+2 for i in range(5)]+[16384*i for i in range(5)]+[0x80000]))
 cases=[(op,s) for op in range(3) for s in seeds];payload=[];expected=[]
 for op,seed in cases:
  for at,size in [(0x6e9a90,17*0x9b84),(0x78ee54,0x800),(0x1300000,0x1000)]:u.mem_write(at,b'\0'*size)
  arg1,arg2,d=fixture(op,seed)
  for at,(w,n) in d.items():put(at,w,n)
  trace=[];name_reads=reference_reads=0;failed=False;result=0;active=True
  try:
   result=rig.call(BODIES[op],*([arg1,arg2] if op==1 else [arg1]))
   # Rig.call returns None; the source return is the machine EAX.
   from unicorn.x86_const import UC_X86_REG_EAX
   result=u.reg_read(UC_X86_REG_EAX)
  except FixtureFault:failed=True;result=0
  finally:active=False
  trace.append(f'F{int(failed)};')
  if op:trace.append(f'O{result};')
  for at,(w,n) in d.items():trace.append(f'V{at},{w},{get(at,w)};')
  h=1469598103934665603
  for n in u.mem_read(0x78ee54,0x800):h=((h^n)*1099511628211)&0xffffffffffffffff
  trace.append(f'H{h};');expected.append(''.join(trace));payload.append(' '.join(map(str,[op,seed,arg1,arg2,len(d),*[v for at,(w,n) in d.items() for v in [at,w,n]]])))
 proc=subprocess.run([str(a.probe)],input='\n'.join(payload)+'\n',text=True,capture_output=True);assert proc.returncode==0,proc.stderr;actual=proc.stdout.splitlines();assert len(actual)==len(expected)
 diffs=[{'op':op,'seed':seed,'actual':x,'expected':y} for (op,seed),x,y in zip(cases,actual,expected) if x!=y]
 report={'source_sha256':SOURCE_SHA256,'cases':len(cases),'differences':len(diffs),'mismatches':diffs[:2],'transport':'Required indirect target fixture; no DLL export port','audio_output':'none'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=128);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
