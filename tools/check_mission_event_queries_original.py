#!/usr/bin/env python3
"""Compare event query results and reached source reads with Df.exe."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
from check_game_messages_original import Harness,SOURCE_HASH
from unicorn import UC_HOOK_MEM_READ,UcError
BASES=(0x7dde80,0x867e80,0x8ace80,0x7cca80)
def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=520);p.add_argument('--report',type=Path);a=p.parse_args();raw=a.game.read_bytes()
 if hashlib.sha256(raw).hexdigest()!=SOURCE_HASH:raise ValueError('Source hash differs')
 rows=[json.loads(x) for x in subprocess.run([str(a.probe.resolve())],input=''.join(f'{i}\n' for i in range(a.cases)),text=True,capture_output=True,check=True).stdout.splitlines()]
 h=Harness(raw);u=h.machine();reads=[];counts={};seed=0
 def put(at,n):u.mem_write(at,struct.pack('<I',n&0xffffffff))
 def is_actor(at):return any(base<=at<base+5*0x114 for base in BASES)
 def read(machine,access,at,width,value,user):
  if at not in (0x8cfa18,0x8cfa1c) and not is_actor(at):return
  reads.append(at)
  if at in (0x8cfa18,0x8cfa1c):
   counts[at]=counts.get(at,0)+1;n=counts[at];put(at,-1 if seed%11==0 else 2 if seed%7==0 and n>1 else 5)
  if seed%13==0 and at==BASES[3]+0x90:put(at,BASES[0])
 hook=u.hook_add(UC_HOOK_MEM_READ,read);diffs=[]
 for seed,row in enumerate(rows):
  for g,base in enumerate(BASES):
   image=bytearray(5*0x114)
   for i in range(5):
    at=i*0x114;struct.pack_into('<I',image,at,int((seed+i+g)%3!=0));struct.pack_into('<I',image,at+0x70,99|(((seed+i+g)%5-2)&0xffff)<<16);struct.pack_into('<ii',image,at+0x24,(seed*13+i*7+g)%41-20,(seed*7+i*11+g)%41-20);struct.pack_into('<I',image,at+0x90,BASES[(g+1)%4]+((i+2)%5)*0x114 if (seed+i)%4 else 0)
   u.mem_write(base,bytes(image))
  reads.clear();counts.clear();first=seed%5-2;second=(seed//5)%5-2
  if seed%2:h.call(u,0x460560,(first,second))
  else:
   param=0x1300000;u.mem_write(param,bytes(12)+struct.pack('<5i',first,-15,-12,15,12));h.call(u,0x460330,(param,))
  from unicorn.x86_const import UC_X86_REG_EAX
  want=dict(result=u.reg_read(UC_X86_REG_EAX),reads=list(reads))
  if row!=want:diffs.append(dict(seed=seed,want=want,got=row))
  if len(diffs)>=3:break
 u.hook_del(hook)
 # An invalid reached link faults in the original rather than returning no match.
 put(BASES[3],1);put(BASES[3]+0x70,0xffff0000);put(BASES[3]+0x90,0x123);put(0x8cfa18,1);fault=False
 try:h.call(u,0x460560,(-1,2))
 except UcError:fault=True
 if not fault:raise ValueError('Original invalid link did not fault')
 report=dict(source_sha256=SOURCE_HASH,cases=len(rows),differences=len(diffs),invalid_link_fault=fault,details=diffs)
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='details'}));return bool(diffs)
if __name__=='__main__':raise SystemExit(main())
