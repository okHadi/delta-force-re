#!/usr/bin/env python3
"""Compare attached sound registration and loudness with original instructions."""
import argparse,json,struct,subprocess
from pathlib import Path
from unicorn import UC_HOOK_MEM_READ,UC_HOOK_MEM_WRITE,UC_MEM_READ,UcError
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256
BASE=0x6e0700;FIRST=0x3200000;SECOND=FIRST+0x100;OFFSET=14695981039346656037
def mix(h,n):
 for i in range(4):h=((h^((n>>(i*8))&255))*1099511628211)&((1<<64)-1)
 return h
def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
 run=subprocess.run([str(a.probe.resolve())],input=''.join(f'{i}\n' for i in range(a.cases)),text=True,capture_output=True,check=True);actual=[json.loads(s) for s in run.stdout.splitlines()];assert len(actual)==a.cases
 rig=Rig(a.game);rig.stubs.pop(0x43ef80,None);rig.stubs.pop(0x43f4d0,None);rig.u.mem_map(0x3100000,0x10000);rig.u.mem_map(FIRST,0x10000);state={};active=False
 def put(at,n,width=4):rig.u.mem_write(at,int(n&((1<<(width*8))-1)).to_bytes(width,'little'))
 def get(at,width=4):return int.from_bytes(rig.u.mem_read(at,width),'little')
 def memory(u,kind,at,width,value,_):
  if not active:return
  if not (BASE<=at<BASE+16384 or at==0x4f7ea4 or 0x4f2ea4<=at<0x4f7ea4 or FIRST<=at<FIRST+0x200 or at==0x310001c):return
  key='reads' if kind==UC_MEM_READ else 'writes';n=get(at,width) if kind==UC_MEM_READ else value&((1<<(width*8))-1)
  for v in (at,width,n):state[key]=mix(state[key],v)
  state['read_count' if kind==UC_MEM_READ else 'write_count']+=1
 def loader():
  sp=rig.u.reg_read(UC_X86_REG_ESP);assert get(sp+4)==seed%512;state['calls']+=1;put(definition,SECOND);rig.finish(int(seed%5!=0))
 rig.stubs[0x43ee80]=loader;rig.u.hook_add(UC_HOOK_MEM_READ|UC_HOOK_MEM_WRITE,memory);differences=[];faults=0
 for seed,got in enumerate(actual):
  definition=0x4f2ea8+(seed%512)*40;actor=0x3100000 if seed%17 else 0;occupied=(0,1,1023,1024)[seed%4]
  for i in range(1024):rig.u.mem_write(BASE+i*16,struct.pack('<4I',0xabc00000+i,7 if i<occupied else 0,seed+i,0xfedcba98))
  put(definition,FIRST if seed%7 else 0);put(definition+0x20,2 if seed%6 else 0,1);put(FIRST+0x18,9 if seed%4 else 0);put(FIRST+0x48,seed*97);put(SECOND+0x48,seed*177);put(0x4f7ea4,0xffffffff-seed);put(0x310001c,0)
  state=dict(reads=OFFSET,writes=OFFSET,read_count=0,write_count=0,calls=0);active=True;fault=False
  try:rig.call(0x43ef80,actor,seed%512,seed*11,seed*13)
  except UcError:fault=True;faults+=1
  rig.call(0x43f4d0,seed%512);loudness=rig.u.reg_read(UC_X86_REG_EAX);active=False;image=OFFSET
  for n in struct.unpack('<4096I',rig.u.mem_read(BASE,16384)):image=mix(image,n)
  image=mix(image,get(0x4f7ea4));image=mix(image,get(0x310001c) if actor else 0)
  want=dict(state,image=image,fault=int(fault),loudness=loudness)
  if got!=want:differences.append(dict(seed=seed,want=want,got=got))
 report=dict(source_sha256=SOURCE_SHA256,bodies=['43EF80','43F4D0'],cases=a.cases,fault_prefixes=faults,differences=len(differences),details=differences[:3],foreign_bodies=['43EE80 ambient sample loader fixture'],comparison='full registration owner,counter,actor link and ordered memory accesses through64bit FNV hashes')
 a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='details'}));return bool(differences)
if __name__=='__main__':raise SystemExit(main())
