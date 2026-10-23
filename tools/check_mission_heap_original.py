#!/usr/bin/env python3
"""Compare the arena heap with all nine actual instruction bodies."""
import argparse,json,random,subprocess,zlib
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EBX
from check_projectiles_original import Rig,SOURCE_SHA256,STACK
A=0x1100000;C=0x1200000;T=0x1210000;F=0x1300000;CB=0x405000
SPANS=[(0x5013a0,4),(0xcc0870,4),(A,0x1000),(C,0x40),(T,0x4000),(F,0x30)]
def run(a):
 r=Rig(a.game);rng=random.Random(0x497f60);active=False;pending=[];trace=[];reads=writes=calls=pointer_reads=0;failed=False;rejected=None;v=None;fs=F
 def norm(at):return F+at-fs if fs<=at<fs+0x30 else at
 def physical(at):return fs+at-F if F<=at<F+0x30 else at
 def visible(at,width):
  if F<=at and at+width<=F+0x30:
   off=at-F
   return (off+width<=12 or off>=32) if v[0]==1 else off==8 and width==4 if v[0]==2 else True
  return any(base<=at and at+width<=base+n for base,n in SPANS)
 def put(at,n):r.u.mem_write(physical(at),(n&0xffffffff).to_bytes(4,'little'))
 def get(at):return int.from_bytes(r.u.mem_read(physical(at),4),'little')
 def flush(*_):
  while pending:pending.pop(0)()
 def memory(u,kind,at,width,value,_):
  nonlocal reads,writes,pointer_reads,failed,rejected
  if not active:return True
  at_norm=norm(at)
  if 0x2000000<=at<0x2010000 and not visible(at_norm,width):return True
  owned=visible(at_norm,width)
  is_read=kind in [unicorn.UC_MEM_READ,unicorn.UC_MEM_READ_UNMAPPED]
  if is_read:
   reads+=1
   if reads==v[7] or not owned:trace.append(f'F{at_norm},{width};');failed=True;u.emu_stop();return False
   n=get(at_norm);trace.append(f'R{at_norm},{width},{n};')
   if v[6]&1 and at_norm==0x5013a0:pending.append(lambda:put(0x5013a0,C+0x20 if n==C else C))
   if v[6]&2 and at_norm==F+0x20:pending.append(lambda:put(at_norm,v[1]^0x10))
   if v[6]&4 and at_norm==A+0xc:pending.append(lambda:put(A+0xc,A+0xa0))
   if v[6]&16 and at_norm==T+4:
    pointer_reads+=1
    if pointer_reads==1:pending.append(lambda:put(T+4,CB+4))
  else:
   writes+=1;n=value&((1<<(width*8))-1)
   if writes==v[8] or not owned:
    if kind==unicorn.UC_MEM_WRITE:rejected=(at,bytes(u.mem_read(at,width)))
    trace.append(f'E{at_norm},{width},{n};');failed=True;u.emu_stop();return False
   trace.append(f'W{at_norm},{width},{n};')
   if v[6]&8 and A<=at_norm<A+0x1000:pending.append(lambda:put(F+0x2c,v[3]^0xabcdef01))
   if v[6]&64 and v[0]==2 and A<=at_norm<A+0x1000:pending.append(lambda:put(F+8,A+0xb0))
  return True
 def callback(site,count):
  def body():
   nonlocal calls,failed
   sp=r.u.reg_read(UC_X86_REG_ESP);args=[int.from_bytes(r.u.mem_read(sp+4+i*4,4),'little') for i in range(count)];args=[norm(n) for n in args];trace.append('C'+str(site)+''.join(','+str(n) for n in args)+';');calls+=1
   if calls==v[9]:failed=True;r.u.emu_stop();return
   result=0
   if site==0x49a620:
    r.u.mem_write(physical(args[0]),bytes([args[1]&255])*args[2])
    if v[6]&256:put(0x5013a0,C+0x20)
   elif site==0x49dc80:
    if v[6]&128:put(0xcc0870,0x800)
    result=0 if v[5]&4 else A
   elif v[6]&32:put(T+2047*8,args[0]);put(T+2047*8+4,CB+8)
   r.finish(result)
  return body
 for site,count in [(0x49a620,3),(0x49dc80,1),(CB,1),(CB+4,1),(CB+8,1)]:r.stubs[site]=callback(site,count)
 r.u.hook_add(unicorn.UC_HOOK_CODE,flush);r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory);r.u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,memory)
 rows=[]
 for mode in range(6):
  for mutation in [0,1,2,4,8,16,32,64,128,256,511]:
   for flags in [0,2,4,8,16]:rows.append([mode,0x30,16,0xaabbccdd,1,flags,mutation,0,0,0])
 for size in [0,1,15,16,17,0x80,0x1000,0xfffffff0,0xffffffff]:
  for align in [0,1,2,3,4,16,32,64,0x80000000]:rows.append([1,size,align,0x12345678,1,0,0,0,0,0])
 for offset in [0,1,0x10,0x30,0x40,0x70,0xa0,0xe0,0x140,0x180]:
  for size in [0,1,0x10,0x40,0xffffffff]:rows.append([5,size,4,0,offset,0,0,0,0,0])
 for _ in range(a.cases):rows.append([rng.randrange(6),rng.choice([0,1,16,32,0x80,0x300,0x1000,0xffffffff]),rng.choice([0,1,3,4,16,32]),rng.getrandbits(32),rng.randrange(4),rng.randrange(32),rng.randrange(512),rng.choice([0,1,2,3,5,12,25,100,2048]),rng.choice([0,1,2,4,8,15]),rng.choice([0,1,2,3])])
 for row in rows:row.extend([0x12345678,0])
 for incoming in [0,A,A+16,0x12345678]:
  for cached in [0,A+0x40]:
   for align in [4,16]:rows.append([1,16,align,0xaabbccdd,1,32,0,0,0,0,incoming,cached])
 expected=[]
 for v in rows:
  fs=STACK-0x1c if v[0]==1 else STACK-4 if v[0]==2 else F
  for at,n in SPANS:r.u.mem_write(physical(at),bytes(n))
  r.u.mem_write(A,bytes((i*37+11)&255 for i in range(0x1000)));r.u.mem_write(physical(F),b'\xa5'*0x30)
  put(0x5013a0,C);put(0xcc0870,0 if v[5]&16 else 0x1000)
  for ctx in [C,C+0x20]:
   for offset,n in [(0,A),(4,0x180 if v[5]&1 else 0x1000),(8,0 if v[5]&32 else A),(12,0xffffffff if v[5]&8 else 0x1000023),(16,T if v[5]&2 else 0)]:put(ctx+offset,n)
  nodes=[A,A+0x40,A+0xa0,A+0x140];lengths=[0,0x20,0x40,0x10]
  for i,node in enumerate(nodes):
   for offset,n in [(0,lengths[i]),(4,100+i),(8,200+i),(12,nodes[i+1] if i<3 else 0)]:put(node+offset,n)
  payload=nodes[v[4]%4]+16
  for i in [0,1,2047]:put(T+i*8,A+0xb0 if i==2047 else payload);put(T+i*8+4,CB+(8 if i==2047 else i*4))
  for offset,n in [(0,v[1]),(4,v[2]),(8,payload if v[0]==2 else v[11]),(32,v[1]),(36,v[2]),(40,0xfedcba98),(44,v[3])]:put(F+offset,n)
  trace=[];pending=[];reads=writes=calls=pointer_reads=0;failed=False;rejected=None;active=True
  args=[[],[v[1],v[2],0xfedcba98,v[3]],[payload],[nodes[v[4]%4]],[payload],[A,A+v[4],v[1],F]][v[0]];r.u.reg_write(UC_X86_REG_EBX,v[10])
  try:r.call([0x497e20,0x497f60,0x4980d0,0x498550,0x498850,0x498a00][v[0]],*args);flush();result=r.u.reg_read(UC_X86_REG_EAX)
  except (RuntimeError,unicorn.UcError):
   if not failed:raise
   result=0
  finally:active=False
  if rejected:r.u.mem_write(*rejected)
  checksum=1
  for at,n in SPANS:
   data=bytes(r.u.mem_read(physical(at),n))
   if at==F and v[0]==1:data=data[:12]+data[32:]
   if at==F and v[0]==2:data=data[8:12]
   checksum=zlib.adler32(data,checksum)
  expected.append(f'{int(failed)} {result if v[0]!=4 and not failed else 0} {checksum} '+''.join(trace))
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,v)) for v in rows)+'\n',text=True,capture_output=True);assert not p.returncode,p.stderr;actual=p.stdout.splitlines();assert len(actual)==len(expected)
 reuse_cases=0
 for payload_size in [0,1,15,16,31,64,128,512]:
  for alignment in [4,16]:
   v=[0,payload_size,alignment,0x12345678,1,0,0,0,0,0,0x12345678,0];fs=F;put(0x5013a0,C);put(0xcc0870,0x1000);r.call(0x497e20)
   def allocate():
    r.call(0x497f60,payload_size,alignment,0,0x12345678);return r.u.reg_read(UC_X86_REG_EAX)
   first=allocate();middle=allocate();last=allocate();assert first and middle and last
   pattern=bytes((i*29+7)&255 for i in range(payload_size));r.u.mem_write(middle,pattern);r.call(0x4980d0,middle);assert r.u.reg_read(UC_X86_REG_EAX)==0;assert bytes(r.u.mem_read(middle,payload_size))==pattern
   reused=allocate();assert reused==middle;assert bytes(r.u.mem_read(reused,payload_size))==pattern;reuse_cases+=1
 diffs=[{'case':i,'input':rows[i],'actual':x,'expected':y} for i,(x,y) in enumerate(zip(actual,expected)) if x!=y];a.report.write_text(json.dumps({'source_sha256':SOURCE_SHA256,'bodies':['497E20','497F60','4980D0','498550','498850','498A00','498960','498990','4989C0'],'cases':len(rows),'original_retained_reuse_sequences':reuse_cases,'differences':len(diffs),'mismatches':diffs[:3],'coverage':'Fullarena/context/callbacktable/callerframe/currentcells; exactcurrentread/store/callorder; wrappingalignment/size/indices, retainedbytes, context/args/header/callbackmutation, failures','normalization':'AllocatorstackframeESP-1C locals0/4/8+args20..2C; releasestackframeESP-4 argument8; register-save/return slots excluded','foreign_services':'Required CRTbacking/memset/dynamiccallback fixtures; allnineownedbodies execute realinstructions'},indent=2)+'\n');print(json.dumps({'cases':len(rows),'original_retained_reuse_sequences':reuse_cases,'differences':len(diffs)}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=300);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
