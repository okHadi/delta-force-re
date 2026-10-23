#!/usr/bin/env python3
"""Compare sound definition sorting with original43E340 instructions."""
import argparse,json,struct,subprocess
from pathlib import Path
from unicorn import UC_HOOK_MEM_READ,UC_HOOK_MEM_WRITE,UC_MEM_READ
from unicorn.x86_const import UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256
BASE=0x4f2ea4;MASK=(1<<64)-1;OFFSET=14695981039346656037
def mix(h,n):
 for i in range(4):h=((h^((n>>(i*8))&255))*1099511628211)&MASK
 return h
def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=128);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
 run=subprocess.run([str(a.probe.resolve())],input=''.join(f'{i}\n' for i in range(a.cases)),text=True,capture_output=True,check=True);actual=[json.loads(s) for s in run.stdout.splitlines()];assert len(actual)==a.cases
 rig=Rig(a.game);state={};active=False
 def memory(u,kind,at,width,value,_):
  if not active:return
  key='reads' if kind==UC_MEM_READ else 'writes';n=int.from_bytes(u.mem_read(at,width),'little') if kind==UC_MEM_READ else value
  for v in (at,width,n):state[key]=mix(state[key],v)
  state['read_count' if kind==UC_MEM_READ else 'write_count']+=1
 def loader():
  sp=rig.u.reg_read(UC_X86_REG_ESP);assert int.from_bytes(rig.u.mem_read(sp+4,4),'little')==seed
  state['calls']+=1;v=int.from_bytes(rig.u.mem_read(BASE+36,4),'little')^seed;rig.u.mem_write(BASE+36,struct.pack('<I',v));rig.finish(0)
 rig.stubs[0x43edb0]=loader
 rig.u.hook_add(UC_HOOK_MEM_READ|UC_HOOK_MEM_WRITE,memory,begin=BASE,end=BASE+512*40-1)
 differences=[]
 for seed,got in enumerate(actual):
  words=[i if j==0 else (0xabc00000+seed*5120+i*10+j)&0xffffffff for i in range(512) for j in range(10)]
  for k in range(3):
   left=1+(seed*37+k*17)%511;right=1+(seed*13+k*71)%511
   for j in range(10):words[left*10+j],words[right*10+j]=words[right*10+j],words[left*10+j]
  if seed%4:words[(1+seed%511)*10]=(0x200,0xffff,0)[seed%4-1]
  rig.u.mem_write(BASE,struct.pack('<5120I',*words));state=dict(reads=OFFSET,writes=OFFSET,read_count=0,write_count=0,calls=0);active=True
  try:rig.call(0x43e340,seed)
  finally:active=False
  image=OFFSET
  for n in struct.unpack('<5120I',rig.u.mem_read(BASE,512*40)):image=mix(image,n)
  want=dict(state,image=image)
  if want!=got:differences.append(dict(seed=seed,want=want,got=got))
 report=dict(source_sha256=SOURCE_SHA256,body='43E340',cases=a.cases,differences=len(differences),details=differences[:3],comparison='complete table and ordered source reads/stores through64bit FNV hashes',foreign_bodies=['43EDB0 ambient loader fixture'])
 a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='details'}));return bool(differences)
if __name__=='__main__':raise SystemExit(main())
