#!/usr/bin/env python3
"""Compare DLL receive/name bodies and raw queue pop with exact supplied images."""
import argparse,hashlib,json,random,struct,subprocess
from pathlib import Path
import pefile,unicorn,capstone
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,STACK,SOURCE_SHA256
DLL_SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2'
MASK=0xffffffff
ROOT=[0x10001c80,0x10003c00,0x10001aa0,0x4b97a0]
def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==DLL_SHA
 r=Rig(a.game);u=r.u;u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,pefile.PE(data=raw).get_memory_mapped_image());row=None;frames=[];reads=writes=calls=sleeps=0;trace=0;failed=False;active=False;restore=None;first=True
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);stack_control=set()
 for data,base,start,end in[(pefile.PE(data=raw).get_memory_mapped_image(),0x10000000,0x100019e0,0x10003cd3),(pefile.PE(str(a.game/'Df.exe')).get_memory_mapped_image(),0x400000,0x4b97a0,0x4b97ff)]:
  for ins in md.disasm(data[start-base:end-base],start):
   if ins.mnemonic in['push','pop','call','ret']:stack_control.add(ins.address)
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,value,n=4):u.mem_write(at,(value&((1<<(8*n))-1)).to_bytes(n,'little'))
 def q():return 0x10019b0c if row[2]==1 else 0x10019c24
 def head():return 0x1200000+(0xfff0 if row[13]else 0)
 def norm(at):
  for i,(base,site,size)in enumerate(frames):
   if base<=at<base+size:return 0x30000000+i*0x100+at-base
  return at
 def event(n):
  nonlocal trace
  for i in range(4):trace=((trace^((n>>(i*8))&255))*1099511628211)&0xffffffffffffffff
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def called(site,args):
  nonlocal calls
  event(3);event(site);event(len(args))
  for n in args:event(norm(n))
  calls+=1
  if calls==row[9]:stop();return False
  return True
 own={0x10001be0:(3,0x14),0x10001c30:(3,0x14),0x10003c00:(5,0x18),0x10001a20:(2,0xc),0x100019e0:(1,0),0x10001a50:(2,0),0x10001aa0:(2,0xc)}
 def code(uc,at,size,_):
  nonlocal first,sleeps
  if not active:return
  if first:first=False;return
  sp=u.reg_read(UC_X86_REG_ESP)
  if at in own:
   count,frame_size=own[at]
   if at!=0x10001aa0 and not called(at,[get(sp+4+i*4)for i in range(count)]):return
   if frame_size:frames.append((sp-4 if frame_size==0x14 else sp,at,frame_size))
  elif at in [0x1100100,0x1100200,0x10003aa0,0x4c5520]:
   count=2 if at in[0x10003aa0,0x4c5520]else 1;args=[get(sp+4+i*4)for i in range(count)]
   if not called(at,args):return
   if at==0x4c5520:u.reg_write(UC_X86_REG_EIP,0x10001aa0);return
   if at==0x1100100:
    sleeps+=1
    if row[6]&1:put(0x10006010,0x1100300)
    if sleeps>=row[5]:put(q()+0x10,0)
   if at==0x1100200 and row[6]&4:
    put(q()+0xc,0)
    for base,site,size in frames:
     if site==0x10003c00:put(base+0xc,0x1200040)
   ra=get(sp);u.reg_write(UC_X86_REG_ESP,sp+4+(4 if at==0x1100100 else 0));u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_EIP,ra)
 u.hook_add(unicorn.UC_HOOK_CODE,code)
 def stack_allowed(at):
  for base,site,size in frames:
   off=at-base
   if site in[0x10001be0,0x10001c30]and(0<=off<4 or 8<=off<size):return True
   if site==0x4b97a0 and(0<=off<0x88 or 0x94<=off<size):return True
   if site not in[0x10001be0,0x10001c30,0x4b97a0]and 4<=off<size:return True
  return False
 def memory(uc,kind,at,width,value,_):
  nonlocal reads,writes,restore
  if not active:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not(0x100019e0<=pc<0x10001b03 or 0x10001be0<=pc<0x10001ce7 or 0x10003c00<=pc<0x10003cd3 or 0x4b97a0<=pc<0x4b97ff):return
  if 0x2000000<=at<0x2010000 and(pc in stack_control or not stack_allowed(at)):return
  if kind==unicorn.UC_MEM_WRITE:
   event(2);event(norm(at));event(width);event(norm(value&MASK));writes+=1
   if writes==row[8]:restore=(at,get(at,width),width);stop();return
   if row[6]&2:
    for base,site,size in frames:
     if site==0x10003c00 and at==get(base+8):put(head()+4,row[3]+1)
  else:
   event(1);event(norm(at));event(width);reads+=1
   if reads==row[7]:stop();return
   if row[6]&8 and at==q()+4:put(q()+8,head())
   if row[6]&32 and at==0x100198ac:put(frames[0][0]+(8 if row[0]==2 else 4),0x1212800)
   if row[6]&64 and at in[0x121409c,0x1214158]:
    for base,site,size in frames:
     if site==0x10001a20:put(base+8,0x1212800)
   if row[6]&128 and at==head():put(head(),0x1211800)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 rng=random.Random(0x10003c00);rows=[]
 def base(op):return [op,7,1,7,1,0,0,0,0,0,0x10000,1,0,0,13]
 for op in range(4):
  for flags in range(8):
   for role in[0,1,2]:x=base(op);x[1]=flags;x[2]=role;rows.append(x)
 for op in[0,1]:
  for n in[0,1,2,3,4,7,8,15,31,64]:
   for alias in range(5):
    for wrap in[0,1]:x=base(op);x[3]=n;x[12]=alias;x[13]=wrap;rows.append(x)
 for op in range(4):
  for mutation in[1,2,4,8,32,64,128,255]:x=base(op);x[6]=mutation;x[5]=3;rows.append(x)
  for field,limit in[(7,25),(8,15),(9,8)]:
   for n in range(1,limit):x=base(op);x[field]=n;x[5]=2;rows.append(x)
 for _ in range(a.cases):
  x=base(rng.randrange(4));x[2]=rng.randrange(3);x[3]=rng.randrange(65);x[4]=rng.choice([0,1,2,MASK,0x80000000]);x[5]=rng.randrange(4);x[6]=rng.randrange(256);x[10]=rng.choice([0xffff,0x10000,0x10001,0x80010000]);x[11]=rng.randrange(2);x[12]=rng.randrange(5);x[13]=rng.randrange(2);x[14]=rng.randrange(256);rows.append(x)
 expected=[]
 for row in rows:
  trace=1469598103934665603;reads=writes=calls=sleeps=0;failed=False;restore=None;first=True;frames=[]
  for start,size in[(0x1200000,0x20000),(0xd0a500,0x100),(0x2000000,0x10000)]:u.mem_write(start,bytes(size))
  for i in range(0x400):put(0x1211000+i,(i+row[14])&255,1);put(0x1211800+i,(i*3+row[14])&255,1);put(0x1212000+i,0xcc,1);put(0x1212800+i,0xdd,1)
  for i in range(0x178):put(0x1214000+i,(i+row[14])&255,1)
  u.mem_write(0x1214000,b'ALPHA\0');u.mem_write(0x12140bc,b'BRAVO\0')
  for at,value in[(0x10019880,row[1]&1),(0x10019884,row[1]&2),(0x100198ac,row[1]&4),(0x100199b0,row[2]),(0x100198b8,2),(0x10019afc,0x1214000),(0x121409c,row[11]),(0x1214158,row[11]),(0x501320,row[1]&1),(0x10006010,0x1100100),(0x10006068,0x1100200),(q(),0x1200000),(q()+4,head()),(q()+8,head()if row[4]==0 else 0x1200030),(q()+0xc,row[4]),(q()+0x10,1 if row[5]else 0),(head(),0x1211000),(head()+4,row[3]),(head()+8,17),(head()+0xc,0x1211800),(0x1213000,1),(0x1213004,0xdeadbeef),(0x1213008,0xcafebabe)]:put(at,value)
  destination=0x1211001 if row[12]==1 else 0x1212000;address=head()+4 if row[12]==3 else 0x1213004;port=head()if row[12]==4 else 0x1213008;length=head()+4 if row[12]==2 else 0x1213000
  args=[destination,length,address]if row[0]==0 else[destination,length,q(),address,port]if row[0]==1 else[row[10],destination]if row[0]==2 else[row[10]]
  frame=STACK-0x90 if row[0]==3 else STACK;size=[0x10,0x18,0xc,0x98][row[0]];frames=[(frame,ROOT[row[0]],size)];u.mem_write(frame,bytes([0x5a])*size)
  if row[0]==3:put(frame+0x87,0,1)
  active=True
  try:r.call(ROOT[row[0]],*args)
  except RuntimeError:
   if not failed:raise
  finally:active=False
  if restore:put(*restore)
  result=0 if failed else u.reg_read(UC_X86_REG_EAX);state=1469598103934665603
  for at,n in[(q(),20),(head(),16),(0x1211000,0x1400),(0x1212800,0x900),(0xd0a500,0x100)]:
   for value in u.mem_read(at,n):state=((state^value)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}')
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',text=True,capture_output=True);assert p.returncode==0,p.stderr;lines=p.stdout.splitlines();assert len(lines)==len(expected)
 bad=[{'input':rows[i],'actual':x,'expected':y}for i,(x,y)in enumerate(zip(lines,expected))if x!=y];a.report.write_text(json.dumps({'dll_sha256':DLL_SHA,'df_sha256':SOURCE_SHA256,'cases':len(rows),'differences':len(bad),'mismatches':bad[:5],'comparison':'fullcanonicalbytehash andorderedread/store/callhash;host/clientgates,queuewrap/lock/copywidth/aliases,currentpostfreefields,sourceframes,namecopy/lookup/clientunchanged,DFphysicalD0A500,errorprefix','network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
