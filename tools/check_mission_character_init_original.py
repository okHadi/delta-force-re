#!/usr/bin/env python3
"""Compare current character scalar setup, pose writes and carry-result action setup."""
import argparse,json,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_EFLAGS
from check_projectiles_original import Rig,SOURCE_SHA256
REGS=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP];BASE,SIZE=0x42fc60,40192
class FixtureFault(Exception):pass
def fixture(seed):
 d={};instance=BASE+[0,187,255][(seed>>5)%3]*0x9c;frame=[0,1,2,0xffffffff,0x80000000][(seed>>8)%5];bones=[0,1,2,0xffffffff,0x80000000][(seed>>11)%5]
 def put(at,n,w=4):d[at]=(w,n&((1<<(8*w))-1))
 put(instance,0x1300000);put(instance+0x58,0x34);put(instance+0x80,0x1301000);put(instance+0x66,255,1);put(0x1300014,0x1300100);put(0x1300100,0 if seed&128 else 0x1300200);put(0x1300204,bones)
 for f in range(4):
  at=0x1300214+f*0x80;put(at,0x80)
  for i in range(16):put(at+0x34+i,(i*17+f*31)&255,1)
 return instance,frame,bones,d
def run(a):
 rig=Rig(a.game);u=rig.u;cold=bytes(u.mem_read(BASE,SIZE));cold_values=struct.unpack('<3I',u.mem_read(0x436ea4,12));active=False;op=seed=instance=0;trace=[];pending=[]
 def get(at,w=4):return int.from_bytes(u.mem_read(at,w),'little')
 def put(at,w,n):u.mem_write(at,(n&((1<<(8*w))-1)).to_bytes(w,'little'))
 def owned(ip):return 0x43a080<=ip<=0x43a0d8 or 0x43a0e0<=ip<=0x43a144 or 0x43a150<=ip<=0x43a178
 def memory(m,kind,at,w,n,_):
  if not active or not owned(m.reg_read(UC_X86_REG_EIP)) or 0x2000000<=at<0x2010000:return
  if kind==unicorn.UC_MEM_READ:
   if seed&32 and at in [0x1300214,0x1300248]:raise FixtureFault()
   if at==instance+0x58 and seed&1:put(instance+0x80,4,0x1301800)
   if at==0x130024a and seed&8:put(0x1300248,1,99)
   trace.append(f'R{at},{w},{get(at,w)};')
  else:
   trace.append(f'W{at},{w},{n&((1<<(8*w))-1)};')
   if at==0x1301000 and seed&2:pending.append(('put',0x130024c,1,77))
   if at==instance+0x0c and seed&4:pending.append(('put',0x1300214,4,0x90))
   if seed&16 and at in [0x1301004,0x1301804,instance+0x28]:pending.append(('fault',0,0,0))
 def flush(m,at,size,_):
  while pending:
   kind,address,width,value=pending.pop(0)
   if kind=='fault':raise FixtureFault()
   put(address,width,value)
 def decoder():
  r=[u.reg_read(reg) for reg in REGS];trace.append('C'+','.join(map(str,[0x43a690,*r]))+';');put(r[4]+0x10,4,r[5]);values=[101,102,103,104,105,106,107]
  for reg,n in zip(REGS,values):u.reg_write(reg,n)
  if seed&64:raise FixtureFault()
  rig.finish(values[0])
 rig.stubs[0x43a690]=decoder;u.hook_add(unicorn.UC_HOOK_CODE,flush);u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 seeds=list(dict.fromkeys(list(range(a.cases))+[256*f+v for f in range(5) for v in [0,1,2,4,8,16,32,64,128]]+[2048*b for b in range(5)]));cases=[(operation,s) for operation in range(3) for s in seeds];payload=[];expected=[]
 for op,seed in cases:
  u.mem_write(BASE,cold);u.mem_write(0x1300000,b'\0'*0x2000);instance,frame,bones,d=fixture(seed)
  for at,(w,n) in d.items():put(at,w,n)
  for reg,n in zip(REGS,[0,frame,33,44,instance,55,66]):u.reg_write(reg,n)
  trace=[];pending=[];failed=selected=False;active=True
  try:rig.call([0x43a080,0x43a0e0,0x43a150][op]);selected=op==2 and not (u.reg_read(UC_X86_REG_EFLAGS)&1)
  except FixtureFault:failed=True
  finally:active=False
  trace.append(f'F{int(failed)};O{int(selected)};');trace.extend(f'G{u.reg_read(reg)};' for reg in REGS);h=1469598103934665603
  for data in [u.mem_read(BASE,SIZE),u.mem_read(0x1300000,0x2000)]:
   for n in data:h=((h^n)*1099511628211)&0xffffffffffffffff
  trace.append(f'H{h};');expected.append(''.join(trace));payload.append(' '.join(map(str,[op,seed,instance,frame,bones,*cold_values,len(d),*[v for at,(w,n) in d.items() for v in [at,w,n]]])))
 proc=subprocess.run([str(a.probe)],input='\n'.join(payload)+'\n',text=True,capture_output=True);assert proc.returncode==0,proc.stderr;actual=proc.stdout.splitlines();assert len(actual)==len(expected)
 diffs=[{'operation':o,'seed':s,'actual':x,'expected':y} for (o,s),x,y in zip(cases,actual,expected) if x!=y];report={'source_sha256':SOURCE_SHA256,'cases':len(cases),'differences':len(diffs),'mismatches':diffs[:2],'scope':'Seven registers, carry-result bool, source memory and required mutable43A690 boundary; otherflags/ESP/savedstack excluded','audio_output':'none'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
