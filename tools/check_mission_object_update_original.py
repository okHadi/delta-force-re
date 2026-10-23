#!/usr/bin/env python3
"""Compare captured callback dispatch with real source message words."""
import argparse,json,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256
A=0x1200000;P=0x1201000;D=0x1210000;TOKEN=0x76000000;RNG=0xbd0060;BODIES=(0x429bc0,0x4519a0,0x4534a0,0x45c340,0x45c3a0,0x45c400,0x453810,0x4533f0,0x453930);MESSAGES=(0,1,4,0x80000000,0xffffffff)
def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=2160);p.add_argument('--report',type=Path);a=p.parse_args();rows=subprocess.run([str(a.probe.resolve())],input=''.join(f'{i}\n' for i in range(a.cases)),capture_output=True,text=True,check=True).stdout.splitlines();rig=Rig(a.game);seed=0;trace=[];calls=0;active=False
 def get(at,width=4):return int.from_bytes(rig.u.mem_read(at,width),'little')
 def put(at,n,width=4):rig.u.mem_write(at,int(n&((1<<(8*width))-1)).to_bytes(width,'little'))
 def foreign(site):
  nonlocal calls
  sp=rig.u.reg_read(UC_X86_REG_ESP);n={0x43f020:2,0x43ef80:4,0x446fb0:3,0x447610:4,0x44f540:2,0x44df90:1,0x44ed40:1,0x44f270:1,0x453270:1,0x447980:2,0x43e7d0:4}[site];args=list(struct.unpack('<%dI'%n,rig.u.mem_read(sp+4,n*4)));values=[0]*6;payload=''
  if site==0x446fb0:values[:3]=[args[0],0,args[2]];payload=bytes(rig.u.mem_read(args[1],24)).hex()
  elif site==0x44f540:values[0]=TOKEN;put(args[1],0)
  else:values[:n]=[TOKEN if value==A else value for value in args]
  trace.append('C'+str(site)+''.join(','+str(value) for value in values)+','+payload+';')
  if calls==0 and seed%11==0:put(A+0x24,get(A+0x24)^0xffffffff);put(RNG,0xcafebabe)
  calls+=1;rig.finish(0)
 for site in (0x43f020,0x43ef80,0x446fb0,0x447610,0x44f540,0x44df90,0x44ed40,0x44f270,0x453270,0x447980,0x43e7d0):rig.stubs[site]=lambda site=site:foreign(site)
 def memory(machine,access,at,width,value,user):
  if not active:return
  if at==D+0x138:
   trace.append('D1,312;')
   if seed%13==0:put(A+0x24,get(A+0x24)^0xffffffff);put(RNG,0x98765432)
  elif at==0xa158c0:
   trace.append('G'+str(at)+';')
   if seed%5==0:put(A+0x6c,get(A+0x6c,1)^0x20,1)
 hook=rig.u.hook_add(unicorn.UC_HOOK_MEM_READ,memory);diffs=[];call_count=0
 for seed,row in enumerate(rows):
  image=bytearray(0x114)
  for offset,n in ((0,1),(4,D),(0xc,P),(0x1c,seed//9%2),(0x24,1),(0x28,2),(0x2c,0x7fff0000),(0x7c,(seed//45%2)*65536),(0xf8,0x123),(0xfc,0xabcdef)):struct.pack_into('<I',image,offset,n)
  image[0x6c]=(0,4,0x80,0xff)[seed//135%4];struct.pack_into('<H',image,0x74,(1,0,0x8000,0xffff)[seed//540%4]);rig.u.mem_write(A,bytes(image));rig.u.mem_write(P,bytes(0xb4));put(0x67f528,(0,1,0x80000000)[seed//45%3]);put(RNG,0x12345678);put(D+0x138,0x77);put(0xa158c0,0x11122233);trace.clear();calls=0;active=True;rig.call(BODIES[seed%9],A,MESSAGES[seed//9%5]);active=False;call_count+=calls;image=bytearray(rig.u.mem_read(A,0x114));struct.pack_into('<I',image,4,1);struct.pack_into('<I',image,0xc,0);wanted=f'{get(RNG)} {image.hex()} '+''.join(trace)
  if wanted!=row:diffs.append(dict(seed=seed,body=hex(BODIES[seed%9]),message=MESSAGES[seed//9%5],want=wanted,got=row))
  if len(diffs)>=3:break
 rig.u.hook_del(hook);report=dict(source_sha256=SOURCE_SHA256,cases=len(rows),bodies=list(BODIES),message_words=list(MESSAGES),foreign_calls=call_count,differences=len(diffs),details=diffs)
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='details'}));return bool(diffs)
if __name__=='__main__':raise SystemExit(main())
