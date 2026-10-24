#!/usr/bin/env python3
"""Compare resource loader/fixup/preparation/header label with original code."""
import argparse,json,random,subprocess
from pathlib import Path
import unicorn,capstone
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_ESI,UC_X86_REG_EDX,UC_X86_REG_EIP
from check_projectiles_original import Rig,SOURCE_SHA256
B=0x3000000;N=0x1200000;F=0x2007ff0;R=0x6ba440;MASK=(1<<64)-1

def fixture(seed=0):
 b=bytearray(0x1800)
 def put(at,n):b[at:at+4]=(n&0xffffffff).to_bytes(4,'little')
 for at,n in {0:0x31524843,4:0x80,8:1,12:0x100,16:1,20:0x200,24:1,28:0x600,36:0x1000,40:0x1100,56:seed,60:0x80000000,64:0xffffffff,0x80:0x1200,0x84:0x1300,0x88:0x1400,0x8c:1,0x90:1,0x98:1,0x200:0x1500,0x1500:1,0x1504:1,0x1508:1,0x1530:0x1600,0x1534:0x1640,0x1200:seed,0x1204:0xffffffff,0x1208:0x80000000,0x1300:0x1200,0x1304:0x1200,0x1308:0x1200}.items():put(at,n)
 return bytes(b)
def run(a):
 r=Rig(a.game);r.u.mem_map(B-0x1000,0x401000);rng=random.Random(0x42cbb0);rows=[]
 for body in range(4):
  for mutation in [0,1,2,4,8,16,31]:rows.append([body,0,99,mutation,0,fixture(rng.getrandbits(32))])
 for field in [8,0x90,0x1500]:
  for counter in [0,0x80000000,0xffffffff]:
   raw=bytearray(fixture());raw[field:field+4]=counter.to_bytes(4,'little')
   for body in [0,1,2]:rows.append([body,0,99,0,0,bytes(raw)])
 for count,match in [(1,0),(3,2),(64,63),(64,99),(0xffffffff,99)]:rows.append([0,count,match,0,0,fixture()])
 for fail in [1,2,3,4,5,10,20,40,80,160,320,640,1000,1500]:
  for body in range(4):rows.append([body,0,99,0,fail,fixture()])
 for _ in range(a.cases):rows.append([rng.randrange(4),0,99,rng.randrange(32),rng.choice([0,0,rng.randrange(1,1700)]),fixture(rng.getrandbits(32))])
 if a.chr:rows.extend([[0,0,99,0,0,a.chr.read_bytes()],[1,0,99,0,0,a.chr.read_bytes()],[2,0,99,0,0,a.chr.read_bytes()]])
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);skip=set()
 for lo,hi in [(0x42cbb0,0x42ccd6),(0x42c9a0,0x42cbad),(0x439d20,0x439e2b),(0x498440,0x49844f)]:
  for i in md.disasm(bytes(r.u.mem_read(lo,hi-lo)),lo):
   if i.mnemonic.startswith(('push','pop','call','ret')):skip.add(i.address)
 skip.update([0x498440,0x498444]);skip.add(0x42c9a7) #stable numeric C argument, outside the memory interface
 active=False;pending=[];m={};written=set();trace=1469598103934665603;events=0;failed=False;reject=None;prepared=False;v=None
 def h(value,n):
  for byte in (n&0xffffffff).to_bytes(4,'little'):value=((value^byte)*1099511628211)&MASK
  return value
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def put(at,n,w=4):r.u.mem_write(at,(n&((1<<(w*8))-1)).to_bytes(w,'little'))
 def txt(at,s):r.u.mem_write(at,s.encode()+b'\0')
 def flush(*_):
  while pending:pending.pop(0)()
 def step():
  nonlocal events,failed
  events+=1
  if events==v[4]:failed=True;r.u.emu_stop();return False
  return True
 def event(kind,at,width,value):
  nonlocal trace
  for n in [ord(kind),at,width,value]:trace=h(trace,n)
 def memory(u,kind,at,width,value,_):
  nonlocal reject,prepared,failed
  pc=u.reg_read(UC_X86_REG_EIP)
  if not active or pc in skip:return
  if not step():
   if kind==unicorn.UC_MEM_WRITE:reject=(at,bytes(u.mem_read(at,width)))
   return
  if any(at+i not in m for i in range(width)):
   failed=True;u.emu_stop();
   if kind==unicorn.UC_MEM_WRITE:reject=(at,bytes(u.mem_read(at,width)))
   return
  L=F-8 if v[0]==2 else F-0x7c
  if kind==unicorn.UC_MEM_READ:
   n=get(at,width);event('R',at,width,n)
   if v[3]&1 and at==F+0x14:pending.append(lambda:put(at,N+64))
   if v[3]&2 and prepared and at==B+0x38:pending.append(lambda:put(L,B+0x100))
   if v[3]&8 and at==B+8 and not prepared:pending.append(lambda:put(at,0))
   if v[3]&16 and prepared and at==B+0x1300:pending.append(lambda:put(L+4,0))
  else:
   event('W',at,width,value&((1<<(width*8))-1));written.update(range(at,at+width))
   if at==L:prepared=True
 def service(site):
  nonlocal trace
  if not step():return
  sp=r.u.reg_read(UC_X86_REG_ESP);argc=1 if site==0x499410 else 2;args=[get(sp+4+i*4)for i in range(argc)]
  for n in [ord('C'),site,argc,*args]:trace=h(trace,n)
  if site==0x499410:
   if v[3]&4:put(F-4,R+100)
   result=B
  elif site==0x456560:
   i=0
   while get(args[0]+i,1)and get(args[0]+i,1)!=46:i+=1
   txt(args[0]+i,'.chr');result=0
  else:
   i=0;result=0
   while True:
    x=get(args[0]+i,1);y=get(args[1]+i,1);x=x-32 if 97<=x<=122 else x;y=y-32 if 97<=y<=122 else y
    if x!=y:result=1;break
    if not x:break
    i+=1
  r.finish(result)
 for site in [0x499410,0x456560,0x499e5e]:r.stubs[site]=lambda site=site:service(site)
 r.u.hook_add(unicorn.UC_HOOK_CODE,flush);r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory);expected=[]
 for v in rows:
  body,count,match,mutation,fail,raw=v;m={**{B-16+i:0 for i in range(len(raw)+16)},**{R+i:0 for i in range(0x1904)},**{N+i:0 for i in range(128)},**{F-0x80+i:0 for i in range(0x98)}}
  for at in m:put(at,0,1)
  r.u.mem_write(B,raw);txt(N,'fixture');txt(N+64,'changed');put(F+0x14,N);put(0x6bbd40,count);put(R+0x60,B)
  for i in range(64):txt(R+i*100,'fixture'if i==match else'other')
  #Preparation receives already relocated bytes, with no observation of setup.
  if body==2:
   r.u.reg_write(UC_X86_REG_ESI,N);r.u.reg_write(UC_X86_REG_EDX,R+0x60);old=v;v=[body,count,match,0,0,raw];r.call(0x439d20);v=old
  trace=1469598103934665603;events=0;failed=False;reject=None;written=set();pending=[];prepared=False;active=True
  try:
   if body==0:r.call(0x42cbb0,N)
   elif body==1:r.u.reg_write(UC_X86_REG_ESI,N);r.u.reg_write(UC_X86_REG_EDX,R+0x60);r.call(0x439d20)
   elif body==2:r.call(0x42c9a0,0)
   else:r.call(0x498440,B,N)
   flush()
  except (RuntimeError,unicorn.UcError):failed=True
  active=False
  if reject:r.u.mem_write(*reject)
  result=getattr(r,'u').reg_read(UC_X86_REG_EAX)if not failed and body in [0,3]else 0
  image=1469598103934665603
  for at in sorted(m):
   if at<F-0x80 or at>=F+0x18 or at in written:image=h(h(image,at),get(at,1))
  expected.append(f'{int(failed)} {result} {events} {trace} {image}')
 lines=[' '.join(map(str,v[:5]))+' '+v[5].hex()for v in rows]
 p=subprocess.run([str(a.probe)],input='\n'.join(lines)+'\n',text=True,capture_output=True);assert not p.returncode,p.stderr;actual=p.stdout.splitlines();assert len(actual)==len(expected)
 diff=[{'case':i,'input':rows[i][:5],'actual':x,'expected':y}for i,(x,y)in enumerate(zip(actual,expected))if x!=y]
 a.report.write_text(json.dumps({'sha256':SOURCE_SHA256,'bodies':['42CBB0','439D20','42C9A0','498440'],'cases':len(rows),'differences':len(diff),'mismatches':diff[:5],'scope':'Current ordered memory/call hash and resulting owned bytes; CPU registers/flags/save slots and stable numeric C args excluded; declared compare/extension/file providers; mutation and failure prefixes'},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(diff)}));assert not diff
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=100);p.add_argument('--chr',type=Path);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
