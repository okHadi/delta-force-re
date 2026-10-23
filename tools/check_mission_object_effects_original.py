#!/usr/bin/env python3
"""Compare four raw object effect bodies with the supplied original."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256,STACK
A=0x7dde80;P=0x1200000;D=0x1210000;F=0x1300000
BODIES=[0x446fb0,0x447610,0x4606b0,0x460d20]
OFFSETS=[24,52,40,284]
RANGES=[[(0,18),(0x1c,12)],[(0,43),(0x38,16)],[(0,24),(0x2c,12)],[(0,0x114),(0x120,16)]]
def run(args):
 r=Rig(args.game);rng=random.Random(0x4606b0);active=False;trace=[];calls=0;random_state=0;failed=False;v=None;S=0
 def put(at,n,w=4):r.u.mem_write(at,(n&((1<<(8*w))-1)).to_bytes(w,'little'))
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def normalize(at):return F+at-S if S<=at<S+0x140 else at
 def memory(u,kind,at,width,value,_):
  if not active:return
  owned=any(start<=at and at+width<=start+size for start,size in [(A,0x114),(P,24),(D,0x13c),(0x67f528,4),(0xc23810,4)]+[(S+off,size) for off,size in RANGES[v[0]]])
  if not owned:return
  n=get(at,width) if kind==unicorn.UC_MEM_READ else value&((1<<(width*8))-1)
  trace.append(f'{"R" if kind==unicorn.UC_MEM_READ else "W"}{normalize(at)},{width},{n};')
 r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def stub(site,count):
  def callback():
   nonlocal calls,random_state,failed
   sp=r.u.reg_read(UC_X86_REG_ESP);raw=[get(sp+4+i*4) for i in range(count)];values=[normalize(n) for n in raw];trace.append('C'+str(site)+''.join(','+str(n) for n in values)+';');calls+=1
   if calls==v[6]:failed=True;r.u.emu_stop();return
   result=0
   if site==0x49a620:
    r.u.mem_write(raw[0],bytes([raw[1]&255])*raw[2]);result=raw[0]
    if v[5]&4:
     if v[0]==1:put(A+0x24,0x44332211)
     if v[0]==3:put(S+0x124,0x55667788)
   if site==0x442630 and v[5]&1:
    for off,n in [(0,11),(4,22),(8,33)]:put(P+off,n)
    if v[0]==0:put(S+0x1c,0x10203040);put(S+0x24,2)
   if site==0x45dc70 and v[5]&2:
    for off,n in [(0,44),(4,55),(8,66),(12,0x77889900)]:put(P+off,n)
   if site==0x477850:random_state=raw[0]
   if site==0x477860:random_state=(random_state*1664525+1013904223)&0xffffffff;result=random_state>>16
   if site in (0x460d20,0x45d970):result=A if v[4] else 0
   if site==0x4828c0:result=2
   r.finish(result)
  return callback
 counts={0x442630:3,0x4606b0:3,0x460d20:4,0x49a620:3,0x477850:1,0x477860:0,0x45dc70:4,0x4657d0:5,0x43e7d0:4,0x45fcf0:6,0x463200:3,0x4828c0:1,0x45d970:2}
 for site,count in counts.items():r.stubs[site]=stub(site,count)
 rows=[]
 for body in range(4):
  for host in [0,1]:
   for owner in [0,A,0x7cca80,0xffffffff]:
    for count in [0,1,2,3,10,0xffffffff,0x80000000]:
     for mutation in range(8):rows.append([body,host,owner,count,1,mutation,0,rng.randbytes(0x114).hex()])
 for _ in range(args.cases):rows.append([rng.randrange(4),rng.getrandbits(32),rng.getrandbits(32),rng.choice([0,1,2,4,8,0xffffffff]),rng.randrange(2),rng.randrange(8),rng.randrange(1,25),rng.randbytes(0x114).hex()])
 expected=[]
 for v in rows:
  body=v[0];S=STACK-OFFSETS[body];image=bytearray.fromhex(v[7]);pose=image[0x24:0x3c];struct.pack_into('<I',image,4,D);r.u.mem_write(A,bytes(image));r.u.mem_write(P,bytes(pose));r.u.mem_write(D,bytes((i^0x6d)&255 for i in range(0x13c)));r.u.mem_write(S,b'\xa5'*0x140);put(0x67f528,v[1]);put(0xc23810,D-2*0x13c)
  if body==0:callargs=[v[2],P,v[3]]
  elif body==1:callargs=[v[2],get(P),get(P+4),get(P+8)]
  elif body==2:callargs=[v[2],P,v[3]]
  else:callargs=[v[2],get(P),get(P+4),get(P+8)]
  trace=[];calls=0;random_state=0;failed=False;active=True;result=0
  # Run the selected entry itself; the same address is a fixture only when called by another body.
  entry_stub=r.stubs.pop(BODIES[body],None)
  try:
   r.call(BODIES[body],*callargs);result=r.u.reg_read(UC_X86_REG_EAX)
  except RuntimeError:
   if not failed:raise
  finally:
   active=False
   if entry_stub:r.stubs[BODIES[body]]=entry_stub
  digest=1469598103934665603
  for at,size in [(A,0x114),(P,24),(D,0x13c)]+[(S+off,size) for off,size in RANGES[body]]:
   for n in bytes(r.u.mem_read(at,size)):digest=((digest^n)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result if body==3 and not failed else 0} {digest} '+''.join(trace))
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',capture_output=True,text=True);assert not p.returncode,p.stderr;actual=p.stdout.splitlines();assert len(actual)==len(expected)
 diff=[{'case':i,'input':rows[i],'actual':a,'expected':b} for i,(a,b) in enumerate(zip(actual,expected)) if a!=b];report={'source_sha256':SOURCE_SHA256,'bodies':[hex(n) for n in BODIES],'cases':len(rows),'differences':len(diff),'mismatches':diff[:3],'comparison':'ordered current reads/stores/calls; local and input bytes; full image; unsigned wire indexes; cached pointers; late args; callback mutations; partial failure','foreign_calls':'explicit fixture contracts; RNG sequence fixture for these rawbody comparisons; native shared RNG join separate','stack_normalization':'source local frame addresses normalized to1300000; saved registers and return slot excluded'};args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(diff)}));assert not diff
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=500);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
