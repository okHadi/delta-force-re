#!/usr/bin/env python3
"""Compare445CE0/raw indexed tail and43F5A0 current reads and sound call."""
import argparse,json,random,subprocess,struct
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256,RET_MAGIC,STACK

def run(a):
 r=Rig(a.game);u=r.u;active=False;trace=[];row=None;reads=0;failed=False;lookup=0
 def put(at,value):u.mem_write(at,(value&0xffffffff).to_bytes(4,'little'))
 def get(at):return int.from_bytes(u.mem_read(at,4),'little')
 def source(u,kind,at,width,value,_):
  nonlocal reads,failed
  if not active or row[0]!=1:return
  op,actor,msg,phrase,x,y,z,mutation=row
  if at not in [actor+0x2c,actor+0x28,actor+4,0x12100d4,actor+0x24,lookup]:return
  trace.append(f'R{at},{width};');reads+=1
  if mutation&32 and reads==3:failed=True;u.emu_stop();return
  if at==actor+0x28 and mutation&1:put(actor+0x2c,get(actor+0x2c)^0xffffffff)
  if at==actor+4 and mutation&2:put(actor+0x24,get(actor+0x24)^0xabcdef01)
  if at==0x12100d4 and mutation&4:put(at,get(at)^0xffffffff)
  if at==actor+0x24 and mutation&8:put(STACK+8,get(STACK+8)^0x13579bdf)
  if at==lookup and mutation&16:
   for off,mask in [(0x24,1),(0x28,2),(0x2c,3)]:put(actor+off,get(actor+off)^mask)
   put(STACK+8,get(STACK+8)^0xffffffff)
 u.hook_add(unicorn.UC_HOOK_MEM_READ,source)
 def broadcast():
  nonlocal failed
  sp=u.reg_read(UC_X86_REG_ESP);packet=get(sp+8);assert get(sp+12)==2;data=bytes(u.mem_read(packet,2));trace.append(f'B{get(sp+4)},{data[0]},{data[1]};')
  if row[7]&64:failed=True;u.emu_stop();return
  u.mem_write(packet,bytes([data[0]^0x42,data[1]]));r.finish()
 def sound():
  nonlocal failed
  sp=u.reg_read(UC_X86_REG_ESP);trace.append('S'+','.join(str(get(sp+4+i*4))for i in range(4))+';')
  if row[7]&128:failed=True;u.emu_stop();return
  r.finish()
 r.stubs[0x4423f0]=broadcast;r.stubs[0x43e7d0]=sound
 rng=random.Random(0x445ce0);rows=[]
 for op in [0,2]:
  for actor in [0,0x7cca7f,0x7cca80,0x7cca80+255*0x114,0x7dde80,0x867e80,0xffffffff]:
   for msg in [0,1,255,256,0xffffffff]:rows.append([op,actor,msg,0,0,0,0,rng.choice([0,64])])
 for mutation in [0,1,2,4,8,16,32,64,128,255]:
  for msg in [0,1,0xffffffff]:rows.append([1,0x7cca80,msg,rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),mutation])
 for _ in range(a.cases):
  op=rng.randrange(3);actor=rng.getrandbits(32)if op!=1 else rng.choice([0x7cca80,0x7dde80,0x867e80])+rng.randrange(16)*0x114;rows.append([op,actor,rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.randrange(256)])
 expected=[]
 for row in rows:
  op,actor,msg,phrase,x,y,z,mutation=row;trace=[];reads=0;failed=False
  if op==1:
   actual_phrase=phrase^0xffffffff if mutation&4 else phrase;actual_msg=msg^0x13579bdf if mutation&8 else msg;lookup=(0x4f7eac+(((actual_phrase*12+actual_msg)&0xffffffff)<<2))&0xffffffff
   try:u.mem_read(lookup,4)
   except unicorn.UcError:u.mem_map(lookup&~0xfff,0x1000)
   assert lookup not in [actor+0x24,actor+0x28,actor+0x2c,actor+4,0x12100d4] and not 0x2000000<=lookup<0x2010000
   put(lookup,lookup^0x55667788);put(actor+0x2c,z);put(actor+0x28,y);put(actor+4,0x1210000);put(0x12100d4,phrase);put(actor+0x24,x)
  active=True
  try:
   if op==2:
    u.mem_write(STACK,struct.pack('<III',RET_MAGIC,0,msg));u.reg_write(UC_X86_REG_ESP,STACK-4);u.reg_write(UC_X86_REG_EAX,actor);u.emu_start(0x445cf5,RET_MAGIC,count=10000)
   else:r.call(0x43f5a0 if op==1 else 0x445ce0,actor,msg)
  except RuntimeError:
   if not failed:raise
  finally:active=False
  expected.append(f'{int(failed)} {get(STACK+8)} '+''.join(trace))
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,r))for r in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,p.stderr;lines=p.stdout.splitlines();assert len(lines)==len(rows),(len(lines),len(rows));bad=[{'input':rows[i],'actual':x,'expected':y}for i,(x,y)in enumerate(zip(lines,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SOURCE_SHA256,'bodies':['445CE0','445CF5 indexed tail','43F5A0'],'cases':len(rows),'differences':len(bad),'mismatches':bad[:2],'comparison':'unsignedwrappedrawindex,low2wirebytes,mutableproducer,sourceorderedcurrentZ/Y/definition/phrase/X/message/table,signedXYZbits,Zwrap,capturedvaluesbeforecallback,mutation/errorprefix','foreign_bodies':['4423F0','43E7D0'],'audio_device_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
