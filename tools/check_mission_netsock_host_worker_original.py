#!/usr/bin/env python3
"""Check the full host entry with real receiver/poll/queue bodies and required fixtures."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,pefile,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,STACK
from check_network_receive_original import Scanner,ENCODE_SIMPLE
SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2';MASK=0xffffffff;FRAME=STACK-12
ROOT=0x10004930

def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA
 r=Rig(a.game);u=r.u;image=pefile.PE(data=raw).get_memory_mapped_image();u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,image)
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls={i.address for i in md.disasm(image[0x4930:0x4a3e],ROOT)if i.mnemonic in['push','pop','call','ret']};wire=Scanner(raw).encode(ENCODE_SIMPLE,b'\x09')
 row=None;reads=writes=calls=sleeps=clockcalls=remaining=0;trace=0;failed=active=False;restore=pending=None;events=[]
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,value,n=4):u.mem_write(at,(value&((1<<(8*n))-1)).to_bytes(n,'little'))
 def norm(at):return 0x30000000+at-FRAME if FRAME<=at<FRAME+12 else at
 def event(n):
  nonlocal trace
  for i in range(4):trace=((trace^((n>>(8*i))&255))*1099511628211)&0xffffffffffffffff
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def call_event(site,args):
  nonlocal calls
  events.append(('C',site,[norm(n)for n in args]));event(3);event(site);event(len(args))
  for n in args:event(norm(n))
  calls+=1
  if calls==row[14]:stop();return False
  return True
 def foreign(site):
  nonlocal sleeps,clockcalls,remaining
  argc={0x10005934:3,0x1000595e:6,0x10005916:0,0x1000592e:1,0x1100100:0,0x1100200:0,0x1100300:1,0x1100400:0,0x1100500:0,0x100046c0:2,0x100048c0:0,0x100028c0:0,0x10004830:0,0x100020d0:1,0x10002d90:0}[site];sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(argc)]
  if site not in[0x10005934,0x1000595e,0x10005916,0x1000592e]and not call_event(site,args):return
  result=0
  if site in[0x1100100,0x1100500]:
   result=(row[8]+clockcalls*row[9]+(17 if site==0x1100500 else 0))&MASK;clockcalls+=1
   if row[11]&4:put(0x10006088,0x1100500)
   if row[11]&256 and clockcalls==1:put(0x10019b08,1)
  elif site==0x1100200:
   if row[11]&1:put(0x10019ca2,0)
   if row[11]&2048:put(0x10019ca2,MASK)
   result=row[7]
  elif site==0x1100300:
   assert args==[5];sleeps+=1
   if row[11]&2:put(0x10006010,0x1100600);put(0x10006044,0x1100600)
   if sleeps>=row[1]:put(0x10019b08,1)
   remaining=row[2]
  elif site==0x1100400:result=row[15]
  elif site==0x10005934:put(args[2],5 if remaining else 0);result=row[16]
  elif site==0x1000595e:
   remaining=max(0,remaining-1)
   if row[3]==1:result=MASK
   else:u.mem_write(args[1],wire);put(args[4],2,2);put(args[4]+2,row[5],2);put(args[4]+4,row[4]);put(args[5],16);result=len(wire)
  elif site==0x10005916:result=0x2733
  elif site==0x1000592e:result=((args[0]&255)<<8)|((args[0]>>8)&255)
  elif site==0x100046c0:
   if row[11]&512:put(FRAME,0xcafe0000|(get(FRAME)&65535))
   if row[11]&64:put(0x10019b08,1)
  elif site==0x100048c0:
   if row[11]&128:put(0x10019ca2,0);put(0x10019c5c,0x1204000);put(0x10019c64,0x1000)
  elif site==0x100028c0:
   if row[11]&8:put(0x10019b34,row[10])
  elif site==0x10004830:
   if row[11]&16:put(0x10019b34,0xabcdef01)
  r.finish(result)
  if site in[0x10005934,0x1000595e,0x10005916,0x1000592e,0x1100300]:u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+argc*4)
 for site in[0x10005934,0x1000595e,0x10005916,0x1000592e,0x1100100,0x1100200,0x1100300,0x1100400,0x1100500,0x100046c0,0x100048c0,0x100028c0,0x10004830,0x100020d0,0x10002d90]:r.stubs[site]=lambda site=site:foreign(site)
 def code(uc,pc,size,_):
  nonlocal pending
  if pending:put(*pending);pending=None
  if not active:return
  if pc in[0x10005830,0x100052e0,0x10003ad0]:
   sp=u.reg_read(UC_X86_REG_ESP);argc=5 if pc==0x100052e0 else 1 if pc==0x10003ad0 else 0;call_event(pc,[get(sp+4+i*4)for i in range(argc)])
 u.hook_add(unicorn.UC_HOOK_CODE,code)
 def memory(uc,kind,at,width,value,_):
  nonlocal reads,writes,restore,pending
  if not active:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if kind==unicorn.UC_MEM_WRITE and at==FRAME+8 and 0x100052e0<=pc<0x1000565e and row[11]&32:pending=(at,0,width)
  if not ROOT<=pc<0x10004a3e:return
  if 0x2000000<=at<0x2010000 and(pc in controls or not FRAME<=at<FRAME+12):return
  if kind==unicorn.UC_MEM_WRITE:
   value&=(1<<(width*8))-1;events.append(('W',norm(at),width,norm(value)));event(2);event(norm(at));event(width);event(norm(value));writes+=1
   if writes==row[13]:restore=(at,get(at,width),width);stop()
  else:
   events.append(('R',norm(at),width));event(1);event(norm(at));event(width);reads+=1
   if reads==row[12]:stop();return
   if row[11]&1024 and at==FRAME:put(FRAME+4,0x11223344)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 rng=random.Random(ROOT);rows=[]
 def base():return[0,2,2,0,0x04030201,0x3412,0,16,100,1001,0,0,0,0,0,0x12345678,0,0]
 for mutation in[0,1,2,4,8,16,32,64,128,256,511,512,1024,2048,4095]:
  for mask in[0,1,0x80000000,MASK]:x=base();x[6]=mask;x[11]=mutation;rows.append(x)
 for initial in[0,1,MASK]:
  for packets in[0,1,3]:
   for recv_mode in[0,1]:x=base();x[0]=initial;x[2]=packets;x[3]=recv_mode;rows.append(x)
 for clock in[0,1,0xfffffc17,0xfffffc18,0xfffffff0,MASK]:
  for step in[0,999,1000,1001,0x80000000,MASK]:x=base();x[8]=clock;x[9]=step;rows.append(x)
 for mask in[0,1,2,15,0x80000000,MASK]:
  for random_value in[0,15,16,31,32,0x7fffffff,0x80000000,MASK]:x=base();x[6]=mask;x[7]=random_value;rows.append(x)
 for field,limit in[(12,34),(13,8),(14,30)]:
  for n in range(1,limit):x=base();x[field]=n;rows.append(x)
 for _ in range(a.cases):
  x=base();x[0]=int(rng.randrange(9)==0);x[1]=rng.randrange(1,4);x[2]=rng.randrange(4);x[3]=rng.randrange(2);x[4]=rng.choice([0,0x04030201,MASK]);x[5]=rng.randrange(65536);x[6]=rng.choice([0,1,7,0x80000000,MASK]);x[7]=rng.getrandbits(32);x[8]=rng.getrandbits(32);x[9]=rng.getrandbits(32);x[10]=rng.getrandbits(32);x[11]=rng.randrange(4096);x[15]=rng.getrandbits(32);x[16]=rng.choice([0,1,MASK]);rows.append(x)
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 for row in rows:
  reads=writes=calls=sleeps=clockcalls=0;remaining=row[2];trace=1469598103934665603;failed=False;restore=pending=None;events=[];u.mem_write(0x1200000,b'\xa5'*0x10000);u.mem_write(FRAME,b'\x5a'*12)
  for at,value in[(0x10006088,0x1100100),(0x10006044,0x1100200),(0x10006010,0x1100300),(0x10006048,0x1100400),(0x10019b08,row[0]),(0x10019b34,0x12345678),(0x10019c5c,0x1200000),(0x10019c64,0x2000),(0x10019ca2,row[6]),(0x100096b4,77),(0x100096b8,0),(0x10019854,0x1202000),(0x1001985c,0x2000)]:put(at,value)
  for at in[0x10019b0c,0x10019b20]:
   for offset in range(0,20,4):put(at+offset,7 if offset==12 else 0)
  active=True;result=0
  try:r.call(ROOT);result=u.reg_read(UC_X86_REG_EAX)
  except(unicorn.UcError,RuntimeError):
   if not failed:print('FAIL',row,hex(u.reg_read(UC_X86_REG_EIP)));raise
  finally:active=False
  if failed:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in[(FRAME,12),(0x10019b08,0x30),(0x10019c5c,12),(0x10019ca2,4),(0x10006010,4),(0x10006044,8),(0x10006088,4),(0x1200000,0x4020)]:
   for value in u.mem_read(at,n):state=((state^value)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events)
 p=subprocess.run([str(a.probe),str(a.game/'Netsocknl.dll')],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',text=True,capture_output=True);assert p.returncode==0,p.stderr;lines=p.stdout.splitlines();assert len(lines)==len(rows),(len(lines),len(rows));bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i][:90]}for i,(x,y)in enumerate(zip(lines,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'body':'10004930','cases':len(rows),'differences':len(bad),'mismatches':bad[:5],'scope':'fullblockingworker orderedreads/stores/calls; real nestedreceiver/poll/queue_destroy; controlled current imports/handlers/stop writes; zero OS traffic'},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=128);p.add_argument('--report',type=Path,required=True);p.add_argument('--limit',type=int,default=0);run(p.parse_args())
