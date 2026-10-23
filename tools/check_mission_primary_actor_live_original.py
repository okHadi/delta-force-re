#!/usr/bin/env python3
"""Run45D970 against the primary adapter and original fixed pool."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256
A=0x7dde80;D=0x18000000;S=0x1d000000;COUNT=0x8cfa1c

def run(args):
 r=Rig(args.game);r.u.mem_map(D,0x1000);r.u.mem_map(S,0x1000);rng=random.Random(0x45d970);active=False;v=None;trace=[];failed=False
 def put(at,n,w=4):r.u.mem_write(at,(n&((1<<(w*8))-1)).to_bytes(w,'little'))
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def memory(u,kind,at,width,value,_):
  if not active or not(any(start<=at and at+width<=start+size for start,size in [(A,0x45000),(D,0x400),(S,0x114),(COUNT,4)])):return
  n=get(at,width) if kind==unicorn.UC_MEM_READ else value&((1<<(width*8))-1);trace.append(f'{"R" if kind==unicorn.UC_MEM_READ else "W"}{at},{width},{n};')
 r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def person():
  nonlocal failed
  sp=r.u.reg_read(UC_X86_REG_ESP);actor=get(sp+4);trace.append(f'C{0x44a0c0},{actor};')
  if v[7]:failed=True;r.u.emu_stop();return
  if v[6]:put(actor+0x24,0x12345678)
  r.finish(0x794680 if v[5] else 0)
 r.stubs[0x44a0c0]=person
 rows=[]
 for count,occupied in [(0,0),(0,3),(1,0),(1,1),(3,1),(3,3),(3,7)]:
  for index in [0,1,2]:
   for flags in [0,4]:
    for person_value,mutation,fail in [(0,0,0),(1,0,0),(1,1,0),(0,0,1)]:rows.append([count,occupied,index,6,flags,person_value,mutation,fail,rng.randbytes(0x114).hex()])
 for _ in range(args.cases):
  count=rng.randrange(9);rows.append([count,rng.randrange(13),rng.randrange(3),rng.choice([0,6,6]),rng.choice([0,4]),rng.randrange(2),rng.randrange(2),rng.randrange(2),rng.randbytes(0x114).hex()])
 expected=[]
 for v in rows:
  count,occupied,index,type_,flags=v[:5];pool=bytearray(0x45000)
  for slot in range(max(count,occupied)):
   if slot!=occupied:struct.pack_into('<II',pool,slot*0x114,1,D+0x13c)
  r.u.mem_write(A,bytes(pool));r.u.mem_write(D,bytes(0x400));source=bytearray.fromhex(v[8]);struct.pack_into('<II',source,0,index,D+index*0x13c);struct.pack_into('<I',source,0xc,0);struct.pack_into('<I',source,0x18,0);r.u.mem_write(S,bytes(source));put(COUNT,count);put(D+index*0x13c+0x28,type_);put(D+index*0x13c+0x24,flags,1);trace=[];failed=False;active=True
  try:r.call(0x45d970,S,0)
  except RuntimeError:
   if not failed:raise
  finally:active=False
  digest=1469598103934665603
  for byte in bytes(r.u.mem_read(A,0x45000)):digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {0 if failed else r.u.reg_read(UC_X86_REG_EAX)} {get(COUNT)} {digest} '+''.join(trace))
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True);assert not p.returncode,p.stderr;actual=p.stdout.splitlines();assert len(actual)==len(expected)
 diff=[{'case':i,'input':rows[i],'actual':a,'expected':b} for i,(a,b) in enumerate(zip(actual,expected)) if a!=b];report={'source_sha256':SOURCE_SHA256,'body':'45D970 mode0 with new canonicalprimary adapter','cases':len(rows),'differences':len(diff),'mismatches':diff[:3],'coverage':'actual69DWORDcopy andcountpublication/personcall order; prefixholes; uncountedtail; full1024rawimages; failureaftercount; currentpersonbinding; sourceNULL anddefinitiongates','foreign_body':'44A0C0 fixture updates canonicalcurrentperson before rawpointerstore'};args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(diff)}));assert not diff
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=64);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
