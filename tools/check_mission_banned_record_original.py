#!/usr/bin/env python3
"""Compare 448690 reads, string edits, IP stores and current count changes."""
import argparse,json,subprocess,struct
from pathlib import Path
import unicorn as uc
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256,STACK
FRAME=STACK-24

def run(a):
 r=Rig(a.game);active=False;seed=0;calls=0;trace=[]
 def put(at,w,n):r.u.mem_write(at,(n&((1<<(8*w))-1)).to_bytes(w,'little'))
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def memory(u,kind,at,w,n,_):
  if not active or not(at==0x6e5888 or 0x6e5890<=at<0x6e5890+20 or 0x6e6090<=at<0x6e6090+80 or 0x1300000<=at<0x1300300 or FRAME<=at<FRAME+8 or 0x4cc6ec<=at<0x4cc6ee):return
  trace.append(f'R{at},{w},{get(at,w)};'if kind==uc.UC_MEM_READ else f'W{at},{w},{n&((1<<(8*w))-1)};')
 r.u.hook_add(uc.UC_HOOK_MEM_READ|uc.UC_HOOK_MEM_WRITE,memory)
 def atoi():
  nonlocal calls
  arg=get(r.u.reg_read(UC_X86_REG_ESP)+4);trace.append(f'C{0x49afa0},{arg};');i=calls;calls+=1
  if seed&1:put(0x6e5888,4,seed+i)
  if i==1 and seed&2:put(FRAME+4,4,0x12345678+seed)
  if i==2 and seed&4:put(0x1300000,4,1)
  r.finish(seed*0x1234567+i*0x89abcdef)
 r.stubs[0x49afa0]=atoi
 rows=subprocess.run([str(a.probe.resolve())],input=''.join(f'{i}\n'for i in range(a.cases)),capture_output=True,text=True,check=True).stdout.splitlines();assert len(rows)==a.cases;diff=[]
 for seed,row in enumerate(rows):
  r.u.mem_write(0x1300000,bytes(0x300));put(0x6e5888,4,seed%5);put(0x1300000,4,seed%3);put(0x1300004,4,0x1300100);put(0x1300008,4,0x1300200)
  ip=[b'192.168.1.2',b'1',b'.2..4',b'',b'1.2.3.4.5',b'-1.256.999.0'][seed%6];label=[b'A'*16,b'',b'a',b'fifteen letters'][seed%4];r.u.mem_write(0x1300100,ip+b'\0');r.u.mem_write(0x1300200,label+b'\0');calls=0;trace=[];active=True
  r.call(0x448690,0x1300000 if seed%13 else 0,1);active=False;expected=f'{r.u.reg_read(UC_X86_REG_EAX)} '+''.join(trace)
  if row!=expected:diff.append({'seed':seed,'actual':row,'expected':expected})
  if len(diff)>=2:break
 report={'source_sha256':SOURCE_SHA256,'cases':len(rows),'differences':len(diff),'details':diff,'scope':'448690 complete body; ordered memory, three in-place separators, raw32 IP shifts, label bounds/copy, cached vs current count and frame aliases; required49AFA0 calls use a synchronous fixture'}
 a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items()if k!='details'}));assert not diff
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1024);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
