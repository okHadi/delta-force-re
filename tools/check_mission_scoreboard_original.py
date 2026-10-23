#!/usr/bin/env python3
"""Compare message15's original instruction block and current shared frame."""
import argparse,json,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
from check_projectiles_original import Rig,SOURCE_SHA256

class FixtureFault(Exception):pass
def run(a):
 rig=Rig(a.game);u=rig.u;frame=0x1308000;packet=0x1200000;name=0x1210000
 active=False;trace=[];seed=0;lookups=0
 def get(at,w=4):return int.from_bytes(u.mem_read(at,w),'little')
 def put(at,w,n):u.mem_write(at,(n&((1<<(8*w))-1)).to_bytes(w,'little'))
 def memory(m,kind,at,w,n,_):
  if not active or not 0x46ebc7<=m.reg_read(UC_X86_REG_EIP)<=0x46ed04:return
  # Omit CPU push/pop saves and call arguments. Keep every mapped local access.
  if frame-0x100<=at<frame:return
  if kind==unicorn.UC_MEM_READ:trace.append(f'R{at},{w},{get(at,w)};')
  else:trace.append(f'W{at},{w},{n&((1<<(8*w))-1)};')
 def lookup():
  nonlocal lookups
  lookups+=1
  sp=u.reg_read(UC_X86_REG_ESP);args=list(struct.unpack('<2I',u.mem_read(sp+4,8)));trace.append('C'+','.join(map(str,[0x404cf0,*args]))+';')
  if seed&1:put(0xbcfbe8,4,0)
  if seed&2:put(frame+0x60,4,get(frame+0x60)+2)
  if seed&4:put(frame+0x64,4,get(frame+0x64)+4)
  if seed&8 and lookups==1:put(frame+0x68,4,7)
  if seed&16:put(frame+0x5c,4,packet+16)
  if seed&32:put(packet+0x4b,4,0x89abcdef)
  if seed&64:raise FixtureFault()
  rig.finish(name+0x100)
 rig.stubs[0x404cf0]=lookup;u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 plans=[(s,c) for s in range(a.cases) for c in [0,1,2,17,32,33,64,255]];expected=[]
 for seed,count in plans:
  u.mem_write(0xbcf664,b'\0'*0x3000);u.mem_write(frame,b'\0'*0x70)
  u.mem_write(packet,bytes((i*37+seed*11)&255 for i in range(0x400)));put(packet+0x8e,1,count);put(frame+0x44,4,packet)
  for i in range(256):put(0x7cca8c+i*0x114,4,name if (seed+i)%3 else 0)
  u.mem_write(name+4,b'P\0');u.mem_write(name+0x100,b'C\0')
  trace=[];lookups=0;failed=False;active=True;u.reg_write(UC_X86_REG_ESP,frame)
  try:
   u.emu_start(0x46ebc7,0x46daa3,count=2000000)
   assert u.reg_read(UC_X86_REG_EIP)==0x46daa3,(seed,count,hex(u.reg_read(UC_X86_REG_EIP)),get(0xbcfbe8),get(frame+0x68))
  except FixtureFault:failed=True
  finally:active=False
  trace.append(f'F{int(failed)};');h=1469598103934665603
  for n in bytes(u.mem_read(0xbcf664,0x3000))+bytes(u.mem_read(frame,0x70)):h=((h^n)*1099511628211)&0xffffffffffffffff
  trace.append(f'H{h};');expected.append(''.join(trace))
 p=subprocess.run([str(a.probe)],input='\n'.join(f'{s} {c}' for s,c in plans)+'\n',text=True,capture_output=True);assert p.returncode==0,p.stderr
 actual=p.stdout.splitlines();assert len(actual)==len(expected)
 diffs=[{'seed':s,'count':c,'actual':x,'expected':y} for (s,c),x,y in zip(plans,actual,expected) if x!=y]
 report={'source_sha256':SOURCE_SHA256,'entry':'46EBC7','stop':'46DAA3','cases':len(plans),'differences':len(diffs),'mismatches':diffs[:1],'lookup':'Explicit mutable localization fixture','scope':'Message15 block only; CPU save stack excluded'}
 a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=128);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
