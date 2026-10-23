#!/usr/bin/env python3
"""Compare HUD queue bodies and the bounded formatter with original instructions."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import *
from check_projectiles_original import Rig,STACK,SOURCE_SHA256
MASK=(1<<64)-1
ENTRY=[0x479f00,0x479e90,0x404d80,0x404dd0,0x493a80]

def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
 rig=Rig(a.game);u=rig.u;u.mem_map(0x3000000,0x1000);u.mem_write(0x3000000,bytes(8)+bytes.fromhex('ffff0000009bcf00ffff00000093cf00'));u.reg_write(UC_X86_REG_GDTR,(0,0x3000000,23,0))
 for reg in [UC_X86_REG_DS,UC_X86_REG_ES,UC_X86_REG_SS]:u.reg_write(reg,16)
 u.reg_write(UC_X86_REG_CS,8)
 rows=[]
 for mode in range(5):
  for seed in [0,1,3,5,7,9,15,31,63,119,120,121,159,160,255,256,257,258,259,511,0x7fffffff,0x80000000,0xffffffff]:
   for mutation in ([0,1,2,4,16,32,63]if mode<4 else [0]):rows.append((mode,seed,mutation))
 rng=random.Random(49380);rows.extend((4,n,0)for n in [0xffffff9c,0xffffff9d,0xfffffff6,0xfffffff7,0xfffffff5,9,10,98,99,100,999,1000,*[rng.getrandbits(32)for _ in range(512)]])
 expected=[];active=False;mode=seed=mutation=0;h=0;reads=writes=0;argument=None
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,n,width=4):u.mem_write(at,(n&((1<<(width*8))-1)).to_bytes(width,'little'))
 def text(at,s):u.mem_write(at,s.encode()+b'\0')
 def word(n):
  nonlocal h
  for byte in (n&MASK).to_bytes(8,'little'):h=((h^byte)*1099511628211)&MASK
 def event(kind,at,width,n):
  for v in [kind,at,width,n]:word(v)
 def args(n):return struct.unpack('<'+'I'*n,u.mem_read(u.reg_read(UC_X86_REG_ESP)+4,n*4))
 def call(site,argv):
  word(3);word(site);word(len(argv))
  for n in argv:word(n)
 def clear():
  dest,value,count=args(3);call(0x49a620,[dest,value,count]);u.mem_write(dest,bytes([value&255])*count)
  if mutation&1:
   if argument is not None:put(argument+4,get(argument+4)^0x80);put(argument,0x1302001)
   put(0xbd4814,get(0xbd4814)+5)
  if mutation&2:put(0xbd34b0,ord('Z'),1)
  rig.finish(dest)
 def lookup():
  call(0x49a092,list(args(3)))
  if mutation&4:put(0x503238,0);put(0x1303000,0x1302001)
  rig.finish(0 if seed&8 else 0x1303000)
 def format_fixture():
  dest,fmt,value=args(3);call(0x493a80,[dest,fmt,value]);signed=value if value<0x80000000 else value-(1<<32);s=f'ID{signed:03d}';text(dest,s);rig.finish(len(s))
 def code(machine,at,size,_):
  nonlocal argument
  if not active:return
  if at==0x479f00:
   argument=u.reg_read(UC_X86_REG_ESP)+4
   if mode==3:call(at,list(args(2)))
  elif at==0x404d80 and mode==3:call(at,list(args(2)))
 def memory(machine,access,at,size,value,_):
  nonlocal reads,writes
  if not active or mode==4:return
  ip=u.reg_read(UC_X86_REG_EIP)
  if not any(lo<=ip<=hi for lo,hi in [(0x479f00,0x479f94),(0x479e90,0x479ec3),(0x404d80,0x404dcf),(0x404dd0,0x404e21)]):return
  if at>=STACK-0x100 and at<STACK+0x100:
   if not(mode==3 and STACK-0x24<=at and at+size<=STACK-4):return
  elif not(any(lo<=at and at+size<=hi for lo,hi in [(0xbd34b0,0xbd4820),(0x1302000,0x1303020),(0x4c82fc,0x4c8314),(0x4ced1a,0x4ced20),(0x503238,0x50323c),(0x67dbc9,0x67dbca)])):return
  if access==unicorn.UC_MEM_READ:
   event(1,at,size,get(at,size));reads+=1
   if reads==7 and mutation&16:
    put(0x1302032,ord('X'),1)
    if argument is not None:put(argument+4,get(argument+4)^0x100)
  else:
   event(2,at,size,value&((1<<(size*8))-1));writes+=1
   if writes==7 and mutation&32:put(0xbd34b0,0x41424344)
 u.hook_add(unicorn.UC_HOOK_CODE,code);u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 for mode,seed,mutation in rows:
  active=False;h=1469598103934665603;reads=writes=0;argument=None
  u.mem_write(0xbd34b0,bytes((i*37+seed)&255 for i in range(0x1360)))
  for i in range(40):put(0xbd34b0+i*0x7c+119,0,1)
  for i in range(4):put(0xbd4810+i*4,seed+i)
  put(0x503238,0x22000000 if seed&1 else 0);put(0x67dbc9,0x20 if seed&2 else 0,1);put(0x1303000,0 if seed&4 else 0x1302000)
  text(0x1302000,chr(97+seed%26)*(seed%160));text(0x4c82fc,'ID%03i');text(0x4c8304,'Triggered Text');text(0x4ced1a,'&');text(0x4ced1c,'');u.mem_write(STACK-0x24,bytes([0xa5])*32);u.mem_write(0x1304000,bytes([0xa5])*32)
  rig.stubs[0x49a620]=clear;rig.stubs[0x49a092]=lookup
  if mode==4:rig.stubs.pop(0x493a80,None)
  else:rig.stubs[0x493a80]=format_fixture
  active=True
  pointer=0xbd34b0+(seed%4)*0x7c if seed&0x100 else 0x1302000
  argv=([pointer,0xfeed0000|seed],[],[0x4c8304,0x1304000],[seed],[0x1304000,0x4c82fc,seed])[mode]
  rig.call(ENTRY[mode],*argv);active=False
  result=u.reg_read(UC_X86_REG_EAX) if mode in [2,4]else 0
  expected.append(' '.join(map(str,[result,get(STACK+4)if mode==0 else 0,get(STACK+8)if mode==0 else 0,get(0x503238),get(0x67dbc9,1),0 if mode==4 else h]))+' '+u.mem_read(0xbd34b0,0x1370).hex()+' '+u.mem_read(STACK-0x24 if mode==3 else 0x1304000,32).hex())
 actual=subprocess.run([str(a.probe.resolve())],input=''.join(f'{m} {s} {x}\n'for m,s,x in rows),text=True,capture_output=True,check=True).stdout.splitlines()
 bad=[{'row':rows[i],'expected':x,'actual':y}for i,(x,y)in enumerate(zip(expected,actual))if x!=y]
 report={'source_sha256':SOURCE_SHA256,'cases':len(rows),'differences':len(bad)+abs(len(expected)-len(actual)),'mismatches':bad[:2],'trace':'FNV64 over ordered current reads/stores and calls. Caller stack argument accesses normalize to caller-owned call fields. Formatter internal trace excluded; actual493A80 final output/return compared independently with flat32-bit segment descriptors.','foreign_fixtures':['49A620 clear/current argument mutation','49A092 entry/current pointer mutation','493A80 in404DD0; standalone493A80 executes actual instructions']}
 a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k]for k in ['cases','differences']}));return bool(report['differences'])
if __name__=='__main__':raise SystemExit(main())
