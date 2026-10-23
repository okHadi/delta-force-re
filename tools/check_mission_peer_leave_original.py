#!/usr/bin/env python3
"""Compare peer reset and leave-drain with actual executable instructions."""
import argparse,json,random,subprocess,zlib
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256,STACK
F=0x1300000;FS=STACK-0xe0;C=0x6e9a90;S=0x6e8090
SPANS=[(0x67f520,0x18),(0x6e5588,8),(S,0x1a00),(C,0xa53c4),(F,0xce)]
def run(a):
 r=Rig(a.game);rng=random.Random(0x4401e0);cache={};active=False;trace=[];reads=writes=calls=0;failed=False;rejected=None;v=None;remaining=0
 def norm(at):return F+at-FS if FS<=at<FS+0xce else at
 def physical(at):return FS+at-F if v[0]==0 and F<=at<F+0xce else at
 def put(at,n,width=4):r.u.mem_write(physical(at),(n&((1<<(width*8))-1)).to_bytes(width,'little'))
 def get(at,width=4):return int.from_bytes(r.u.mem_read(physical(at),width),'little')
 def owned(at,width):return any(base<=at and at+width<=base+size for base,size in SPANS)
 def memory(u,kind,at,width,value,_):
  nonlocal reads,writes,failed,rejected
  at_norm=norm(at)
  if not active or not owned(at_norm,width):return
  if kind==unicorn.UC_MEM_READ:
   reads+=1
   if reads==v[5]:trace.append(f'F{at_norm},{width};');failed=True;u.emu_stop();return
   trace.append(f'R{at_norm},{width},{get(at_norm,width)};')
  else:
   writes+=1;n=value&((1<<(width*8))-1)
   if writes==v[6]:rejected=(at,bytes(u.mem_read(at,width)));trace.append(f'E{at_norm},{width},{n};');failed=True;u.emu_stop();return
   trace.append(f'W{at_norm},{width},{n};')
 def callback(site,count):
  def body():
   nonlocal calls,remaining,failed
   sp=r.u.reg_read(UC_X86_REG_ESP);args=[int.from_bytes(r.u.mem_read(sp+4+i*4,4),'little') for i in range(count)];args=[norm(n) for n in args];trace.append('C'+str(site)+''.join(','+str(n) for n in args)+';');calls+=1
   if calls==v[4]:failed=True;r.u.emu_stop();return
   result=0
   if site==0x49a620:
    r.u.mem_write(physical(args[0]),bytes([args[1]&255])*args[2])
    if args[0]==F and v[3]&16:put(0x67f534,1);put(0x6e558c,0)
   elif site==0x4403c0:
    put(F+1,get(F+1,1)+1,1);put(F+3,args[1])
    if v[3]&1:put(args[1]+0x1c4,1,2)
    if v[3]&2:put(args[1]+0x1c4,0,2)
    if v[3]&4 and args[1]+0x9b84<C+0xa53c4:put(args[1]+0x9b84+0x18c,1,1)
    if v[3]&32:put(F+2,get(F+2,1)^1,1)
   elif site==0x441750:
    if v[3]&8:put(args[0]+0x1c4,1,2)
   elif site==0x460e40:
    if v[3]&1:put(0x6e5588,get(0x6e5588)^1)
    result=remaining
    if remaining:remaining-=1
   elif site==0x442ae0:
    if v[3]&2:put(0x67f528,0);put(0x67f520,0)
    if v[3]&4:put(0x6e5588,0)
   r.finish(result)
  return body
 for site,count in [(0x49a620,3),(0x4403c0,2),(0x441750,4),(0x460e40,0),(0x442ae0,0),(0x4421d0,0),(0x490880,0)]:r.stubs[site]=callback(site,count)
 r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 seeds=[0,1,2,0x80000000,0xffffffff];rows=[]
 for seed in seeds:
  for flags in range(4):
   for mutation in [0,1,2,4,8,16,32,63]:rows.append([0,seed,flags,mutation,0,0,0,0])
 for flags in range(32):
  for mutation in [0,1,2,7]:
   for count in [0,1,4]:rows.append([1,1,flags,mutation,0,0,0,count])
 for _ in range(a.cases):rows.append([rng.randrange(2),rng.choice(seeds),rng.randrange(32),rng.randrange(64),rng.choice([0,1,2,3,5,12,40,70]),rng.choice([0,1,2,3,8,20,50,90]),rng.choice([0,1,2,10,35,100]),rng.randrange(5)])
 expected=[]
 for v in rows:
  for at,size in SPANS:
   key=(size,v[1])
   if key not in cache:cache[key]=bytes(((i*37)^(v[1]>>((i%4)*8)))&255 for i in range(size))
   r.u.mem_write(physical(at),cache[key])
  for at,n in [(0x67f534,v[2]&1),(0x6e558c,(v[2]>>1)&1),(0x67f528,(v[2]>>2)&1),(0x67f520,(v[2]>>3)&1),(0x6e5588,(v[2]>>4)&1)]:put(at,n)
  for i in range(17):put(C+i*0x9b84+0x18c,int(((v[1]+i)&0xffffffff)%3!=0),1);put(C+i*0x9b84+0x1c4,((v[1]+i)&0xffffffff)%6,2)
  for i in range(64):put(S+i*0x68+0x40,0x80000000+i if ((i+v[1])&0xffffffff)%3 else 0)
  trace=[];reads=writes=calls=0;failed=False;rejected=None;remaining=v[7];active=True
  try:r.call(0x403a50 if v[0] else 0x4401e0);result=r.u.reg_read(UC_X86_REG_EAX)
  except RuntimeError:
   if not failed:raise
   result=0
  finally:active=False
  if rejected:r.u.mem_write(*rejected)
  checksum=1
  for at,size in SPANS:checksum=zlib.adler32(bytes(r.u.mem_read(physical(at),size)),checksum)
  expected.append(f'{int(failed)} {result if v[0] and not failed else 0} {checksum} '+''.join(trace))
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,v)) for v in rows)+'\n',text=True,capture_output=True);assert not p.returncode,p.stderr;actual=p.stdout.splitlines();assert len(actual)==len(expected)
 diffs=[{'case':i,'input':rows[i],'actual':x,'expected':y} for i,(x,y) in enumerate(zip(actual,expected)) if x!=y];a.report.write_text(json.dumps({'source_sha256':SOURCE_SHA256,'bodies':['4401E0','403A50/447F60'],'cases':len(rows),'differences':len(diffs),'mismatches':diffs[:2],'coverage':'All17fullclientrecords+64squads+currentframe/cells; orderedreads/stores/requiredcalls; state changes across builder/enqueue; shortcircuit gates; current parse flag; child/read/writefailure prefixes','foreign_services':'Declared memset/builder/enqueue/receive/parser/outbox/pump boundaries; native joins use real canonical clients/result/outbox','frame_normalization':'OriginalSTACK-E0 maps to caller virtual1300000; exactCEbytes'},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(diffs)}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=200);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
