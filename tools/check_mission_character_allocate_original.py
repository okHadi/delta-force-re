#!/usr/bin/env python3
"""Compare raw character allocation, wrapper locals and explicit register ABI."""
import argparse,json,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP
from check_projectiles_original import Rig,SOURCE_SHA256
REGS=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]
BASE,SIZE,FRAME=0x42fc60,40192,0x2007ff0
class FixtureFault(Exception):pass
def run(a):
 rig=Rig(a.game);u=rig.u;cold=bytes(u.mem_read(BASE,SIZE));assert sum(v!=0 for v in cold)==9;cold_values=struct.unpack('<3I',u.mem_read(0x436ea4,12));active=False;op=seed=stage=frame_reads=0;trace=[];pending=[]
 def get(at,w=4):return int.from_bytes(u.mem_read(at,w),'little')
 def put(at,w,n):u.mem_write(at,(n&((1<<(w*8))-1)).to_bytes(w,'little'))
 def memory(m,kind,at,w,n,_):
  nonlocal frame_reads
  ip=m.reg_read(UC_X86_REG_EIP)
  if not active or not (0x439c70<=ip<=0x439cc6 or op and 0x42cce0<=ip<=0x42cd21):return
  if 0x2000000<=at<0x2010000 and not (op and FRAME-8<=at and at+w<=FRAME):return
  if kind==unicorn.UC_MEM_READ:
   if at==FRAME-8:
    if frame_reads==0 and seed&64:u.mem_write(0x439860,b'\1'*256)
    frame_reads+=1
   trace.append(f'R{at},{w},{get(at,w)};')
  else:
   trace.append(f'W{at},{w},{n&((1<<(w*8))-1)};')
   if at==FRAME-8 and seed&16:pending.append((at,0x1300900))
   if at==FRAME-4 and seed&32:pending.append((FRAME-8,0xdeadbeef))
 def flush(m,at,size,_):
  while pending:
   address,value=pending.pop(0);put(address,4,value)
 def foreign(site):
  nonlocal stage
  registers=[u.reg_read(reg) for reg in REGS];trace.append('C'+','.join(map(str,[site,*registers]))+';');put(registers[4]+stage*4,4,registers[5])
  if seed&1:put(0x43995f,1,1)
  stage+=1;values=[0xa000+stage,0xb000+stage,0xc000+stage,0xd000+stage,0xe000+stage,0xf000+stage,0x10000+stage]
  for reg,value in zip(REGS,values):u.reg_write(reg,value)
  if seed&(1<<stage):raise FixtureFault()
  rig.finish(values[0])
 for site in [0x439e48,0x43a0e0,0x43a150]:rig.stubs[site]=lambda site=site:foreign(site)
 u.hook_add(unicorn.UC_HOOK_CODE,flush);u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 seeds=list(dict.fromkeys(list(range(a.cases))+[128*i+v for i in range(4) for v in [0,1,2,4,8,16,32,64]]+[0x10000,0x20000,0x20010]));cases=[(operation,s) for operation in range(2) for s in seeds];expected=[];payload=[]
 for op,seed in cases:
  u.mem_write(BASE,cold);u.mem_write(FRAME-8,b'\0'*8);resource=0 if seed&0x10000 else 0x1300000;free=187 if seed&0x20000 else [0,1,255,256][(seed>>7)%4];d={0x1300060:(4,0x1300800)}
  for i in range(256):d[0x439860+i]=(1,0 if i==free else 1)
  for at,(w,n) in d.items():put(at,w,n)
  for reg,value in zip(REGS,[11,22,33,44,55,0x1300800,77]):u.reg_write(reg,value)
  trace=[];stage=frame_reads=0;pending=[];failed=False;active=True
  try:rig.call(0x42cce0 if op else 0x439c70,*([resource] if op else []))
  except FixtureFault:failed=True
  finally:active=False
  trace.append(f'F{int(failed)};');trace.extend(f'G{u.reg_read(reg)};' for reg in REGS)
  if op:trace.append(f'L{get(FRAME-8)},{get(FRAME-4)};')
  h=1469598103934665603
  for n in u.mem_read(BASE,SIZE):h=((h^n)*1099511628211)&0xffffffffffffffff
  trace.append(f'H{h};');expected.append(''.join(trace));payload.append(' '.join(map(str,[op,seed,resource,*cold_values,len(d),*[v for at,(w,n) in d.items() for v in [at,w,n]]])))
 proc=subprocess.run([str(a.probe)],input='\n'.join(payload)+'\n',text=True,capture_output=True);assert proc.returncode==0,proc.stderr;actual=proc.stdout.splitlines();assert len(actual)==len(expected)
 diffs=[{'operation':o,'seed':s,'actual':x,'expected':y} for (o,s),x,y in zip(cases,actual,expected) if x!=y];report={'source_sha256':SOURCE_SHA256,'cases':len(cases),'differences':len(diffs),'mismatches':diffs[:2],'scope':'Seven generalregisters, currentrawmemory/locals and nestedregistercalls; ESP/EFLAGS/savedstack excluded','nested_services':'Explicit mutable439E48/43A0E0/43A150 fixtures','audio_output':'none'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
