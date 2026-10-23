#!/usr/bin/env python3
"""Compare45BF80 with the supplied original instructions."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256,STACK
A=0x7dde80;F=0x1300000;S=STACK-36

def run(args):
 r=Rig(args.game);rng=random.Random(0x45bf80);active=False;trace=[];pending=[];v=None;calls=0;failed=False
 def put(at,n,w=4):r.u.mem_write(at,(n&((1<<(8*w))-1)).to_bytes(w,'little'))
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def flush(*_):
  while pending:pending.pop(0)()
 def memory(u,kind,at,width,value,_):
  if not active or not(at==0x67f528 or A<=at and at+width<=A+0x114 or S<=at and at+width<=S+24):return
  norm=F+at-S if S<=at<S+24 else at
  if kind==unicorn.UC_MEM_READ:
   n=get(at,width);trace.append(f'R{norm},{width},{n};')
   if at==A+0x2c and v[1]&4:put(A+0x28,0x12345678)
  else:
   n=value&((1<<(8*width))-1);trace.append(f'W{norm},{width},{n};')
   if at==A+0xfc and v[1]&2:pending.append(lambda:put(A+0x6c,0x80,1))
 r.u.hook_add(unicorn.UC_HOOK_CODE,flush);r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def call(site,count):
  def body():
   nonlocal calls,failed
   sp=r.u.reg_read(UC_X86_REG_ESP);values=[get(sp+4+i*4) for i in range(count)];values=[F if n==S else n for n in values];trace.append('C'+str(site)+''.join(','+str(n) for n in values)+';');calls+=1
   if calls==v[2]:failed=True;r.u.emu_stop();return
   if site==0x446fb0 and v[1]&1:
    for off,n in [(0x24,11),(0x28,22),(0x2c,33)]:put(A+off,n)
   r.finish()
  return body
 for site,count in [(0x446fb0,3),(0x447610,4),(0x4471d0,1)]:r.stubs[site]=call(site,count)
 rows=[]
 for host in [0,1,0xffffffff]:
  for flags in [0,4,0x80,0xff]:
   for health in [0,1,0x7fff,0x8000,0xffff]:
    for mutation in range(8):
     image=bytearray(rng.randbytes(0x114));image[0x6c]=flags;struct.pack_into('<H',image,0x74,health);rows.append([host,mutation,0,rng.getrandbits(32),image.hex()])
 for _ in range(args.cases):rows.append([rng.choice([0,1,1]),rng.randrange(8),rng.randrange(4),rng.getrandbits(32),rng.randbytes(0x114).hex()])
 expected=[]
 for v in rows:
  r.u.mem_write(A,bytes.fromhex(v[4]));r.u.mem_write(S,b'\xa5'*24);put(0x67f528,v[0]);trace=[];pending=[];calls=0;failed=False;active=True
  try:r.call(0x45bf80,A,v[3]);flush()
  except RuntimeError:
   if not failed:raise
  finally:active=False
  digest=1469598103934665603
  for n in bytes(r.u.mem_read(A,0x114))+bytes(r.u.mem_read(S,24)):digest=((digest^n)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {digest} '+''.join(trace))
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True);assert not p.returncode,p.stderr;actual=p.stdout.splitlines();assert len(actual)==len(expected)
 diffs=[{'case':i,'input':rows[i],'actual':a,'expected':b} for i,(a,b) in enumerate(zip(actual,expected)) if a!=b]
 report={'source_sha256':SOURCE_SHA256,'body':'45BF80','cases':len(rows),'differences':len(diffs),'mismatches':diffs[:3],'coverage':'ordered actor/frame/global reads and stores; full image/frame; cached flags; late Z/Y/X; callback mutation; call error prefix; message ignored','foreign_calls':'required fixture boundaries446FB0/447610/4471D0; native lifecycle join separate'};args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(diffs)}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=500);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
