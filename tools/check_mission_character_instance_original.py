#!/usr/bin/env python3
"""Compare raw character instances with the hash-bound original instructions."""
import argparse,json,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_EIP,UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256
REGS=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]
ENTRIES=[0x439e48,0x43a1c0,0x43a690,0x43a2c7,0x43a4dc,0x43a730]
class Fault(Exception):pass
def run(a):
 rig=Rig(a.game);u=rig.u;active=False;trace=[];pending=[];seed=0;op=0
 def get(at,w=4):return int.from_bytes(u.mem_read(at,w),'little')
 def put(at,w,n):u.mem_write(at,(n&((1<<(8*w))-1)).to_bytes(w,'little'))
 def owned(ip):return 0x439e48<=ip<=0x439f87 or 0x43a080<=ip<=0x43a0d8 or 0x43a1c0<=ip<=0x43a688 or 0x43a690<=ip<=0x43a795
 def mem(m,kind,at,w,n,_):
  if not active or not owned(m.reg_read(UC_X86_REG_EIP)) or 0x2000000<=at<0x2010000:return
  if kind==unicorn.UC_MEM_READ:
   if seed&16 and at==0x1300210:raise Fault()
   if seed&1 and at==0x1300058:put(0x1300080,4,0x1301600)
   if seed&2 and at==0x1300235:put(0x1301008,4,123)
   trace.append(f'R{at},{w},{get(at,w)};')
  else:
   trace.append(f'W{at},{w},{n&((1<<(8*w))-1)};')
   if seed&4 and at==0x130100c:pending.append((0x1300236,1,7))
   if seed&8 and at==0x439a14:pending.append((0x1300238,1,11))
   if seed&32 and at==0x1301004:pending.append(None)
 def flush(m,at,size,_):
  while pending:
   item=pending.pop(0)
   if item is None:raise Fault()
   put(*item)
  if active and bytes(u.mem_read(at,2))==b'\xf7\xfb':
   divisor=u.reg_read(UC_X86_REG_EBX);num=u.reg_read(UC_X86_REG_EAX)
   if divisor==0 or (divisor==0xffffffff and num==0x80000000):raise Fault()
 def alloc():
  sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4) for i in range(4)];trace.append('A'+','.join(map(str,args))+';')
  if seed&64:raise Fault()
  if seed&128:put(0x1300210,4,2)
  u.reg_write(UC_X86_REG_ECX,0xbad11111);u.reg_write(UC_X86_REG_EDX,0xbad22222);rig.finish(0x1302000)
 def err():trace.append(f'E{0x43b6d5},{u.reg_read(UC_X86_REG_EDX)};');raise Fault()
 rig.stubs[0x497f60]=alloc;rig.stubs[0x43b6d5]=err;u.hook_add(unicorn.UC_HOOK_CODE,flush);u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,mem)
 globals_cold=bytes(u.mem_read(0x439980,0x400));static={}
 for at,n in [(0x42f1d0,0xa00),(0x43b4f1,0x200)]:
  for i in range(n):static[at+i]=(1,get(at+i,1))
 seeds=list(dict.fromkeys(range(a.cases)))+[256*k+v for k in range(12) for v in [0,1,2,4,8,16,32,64,128,255]]+[4096,8192,8448,8704,8193,8192+64,8192+128]
 expected=[];payload=[];cases=[]
 for op in range(6):
  for seed in seeds:
   d=dict(static)
   def p(at,n,w=4):d[at]=(w,n&((1<<(w*8))-1))
   for i,n in enumerate(globals_cold):p(0x439980+i,n,1)
   p(0x1300000,0x1300200);p(0x130000c,0x1300600);p(0x1300010,0x1300220 if op in [3,4] else (0x1300400 if seed&256 else 0));p(0x1300014,[1,2,0,0xffffffff,0x80000000][(seed>>9)%5]);p(0x1300058,0x34 if seed&512 else 0x24);p(0x1300065,seed,1)
   for off,val in [(0x80,0x1301000),(0x84,0x1301200),(0x8c,0x1301400),(0x90,0x1301500),(0x94,0x1302000)]:p(0x1300000+off,val)
   p(0x1300200,0xdeadbeef if seed&4096 else (0x31524843 if not seed&1024 else 0x30524843));p(0x1300204,0x1300800);p(0x130020c,0x1300700);p(0x1300210,[0,0xffffffff,0x80000000][(seed>>8)%3] if seed&8192 else 1+(seed%2));p(0x1300700,1);p(0x1300704,0);p(0x1300818,seed&1)
   p(0x1300608,[1,2,0,0xffffffff,0x80000000][(seed>>9)%5])
   for f in [0x1300220,0x1300400]:
    p(f+4,0x80 if seed&1024 else 0);p(f+8,1 if seed&256 else (seed%7)+2,2);p(f+0xc,seed*191);p(f+0x10,seed*71);p(f+0x14,seed*231);p(f+0x18,seed%2,1);p(f+0x20,0x1301800 if f==0x1300220 else 0x1301900);p(f+0x28,0x1301a00 if seed&512 else 0)
    for i in range(12):p(f+(0x34 if seed&512 else 0x24)+i,(seed*3+i*31)&255,1)
    #Restore header fields overwritten by packed payload for CHR1.
    p(f+0x28,0x1301a00 if seed&512 else 0)
   for i in range(24):p(0x1301a00+i,(seed+i*7)&255,1)
   for i in range(48):p(0x1301800+i,seed*13+i*19,1);p(0x1301900+i,seed*17+i*23,1)
   for i in range(0,0x100,4):p(0x1301000+i,(seed*0x199+i*337)&0xffffffff);p(0x1301600+i,(seed*0x311+i*71)&0xffffffff)
   p(0x4399d0,seed*0x17803);p(0x439a08,seed*0x712355);p(0x439a0c,seed*0x845361);p(0x439a10,seed*0x912497)
   regs=[11,[0,1,2,0xffffffff,0x80000000][(seed>>7)%5],33,44,0x1300000,0x1300200 if op==0 else 0x1300220,77]
   u.mem_write(0x1300000,b'\0'*0x4000)
   for at,(w,n) in d.items():put(at,w,n)
   for reg,n in zip(REGS,regs):u.reg_write(reg,n)
   trace=[];pending=[];failed=False;active=True
   try:rig.call(ENTRIES[op],count=1000000)
   except (Fault,unicorn.UcError):failed=True
   except RuntimeError as e:raise RuntimeError(f'{op=} {seed=} ip={u.reg_read(UC_X86_REG_EIP):x} '+str(e))
   finally:active=False
   trace.append(f'F{int(failed)};');trace.extend(f'G{u.reg_read(reg)};' for reg in REGS)
   h=1469598103934665603
   for data in [u.mem_read(0x1300000,0x4000),u.mem_read(0x439980,0x400)]:
    for n in data:h=((h^n)*1099511628211)&0xffffffffffffffff
   trace.append(f'H{h};');expected.append(''.join(trace));payload.append(' '.join(map(str,[op,seed,*regs,len(d),*[v for at,(w,n) in d.items() for v in (at,w,n)]])));cases.append((op,seed))
 proc=subprocess.run([str(a.probe)],input='\n'.join(payload)+'\n',text=True,capture_output=True);assert proc.returncode==0,proc.stderr;actual=proc.stdout.splitlines();assert len(actual)==len(expected),(len(actual),len(expected));diffs=[{'op':o,'seed':s,'actual':x,'expected':y} for (o,s),x,y in zip(cases,actual,expected) if x!=y]
 report={'source_sha256':SOURCE_SHA256,'cases':len(cases),'differences':len(diffs),'mismatches':diffs[:2],'scope':'Seven register values and ordered memory/requiredcall traces; ESP/EFLAGS/savedstack excluded; source497F60 clobbers ECX/EDX in fixture','audio_output':'none'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
