#!/usr/bin/env python3
"""Replay the seven periodic bodies from the supplied executable."""
import argparse,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256,STACK
MASK=0xffffffff
ENTRY=[0x4438c0,0x448c30,0x445c80,0x446200,0x4482e0,0x446da0,0x448130]
DEF=0x1210000;TEXT=0x13f0000

def run(a):
 r=Rig(a.game);u=r.u;row=None;frame=frame_size=0;trace=[];reads=writes=calls=0;failed=active=False;restore=None;pending={};top_seen=False
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,v,n=4):u.mem_write(at,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
 def string(at,s):u.mem_write(at,s.encode()+b'\0')
 def initialize():
  nonlocal frame,frame_size
  mode,seed,*_=row;seq=seed or 1
  def next_word():
   nonlocal seq
   seq=(seq*1664525+1013904223)&MASK;return seq
  frame=STACK-0x500 if mode==3 else STACK-0x20 if mode==4 else STACK-0x140
  frame_size=0x130 if mode==0 else 0x4f0 if mode==3 else 0x10 if mode==4 else 0
  u.mem_write(0x6e8090,bytes(0x1a00+17*0x9b84));u.mem_write(DEF,bytes(0xa00))
  for base in [0x7cca80,0x7dde80]:u.mem_write(base,bytes(5*0x114))
  for at in range(0x6e8090,0x6e9a90,4):put(at,next_word())
  for i in range(17):
   c=0x6e9a90+i*0x9b84
   for off in range(0x194,0x1c0,4):put(c+off,next_word())
   put(c+8,next_word() if seed&64 else 0x7cca80+i*3*0x114);put(c+0x9c,i%3);put(c+0x18c,int(i<seed%6),1);put(c+0x1c4,5,2)
  for i in range(5):
   d=DEF+i*0x200;put(d+0x20,0xfc3 if i%3==0 else 0xfc4 if i%3==1 else 777)
   for base in [0x7cca80,0x7dde80]:
    actor=base+i*0x114;put(actor+4,d)
    for off in [0x24,0x28,0x2c]:put(actor+off,next_word())
    put(actor+0x6c,next_word(),1);put(actor+0x6d,next_word(),1)
  for i in range(0,0x4f0,4):put(frame+i,next_word())
  for at,val in [(0x67f53c,seed%8),(0x67f524,4 if seed&1 else 0),(0x6e5590,1 if seed&2 else 0),(0x67f528,0 if seed&32 else 1),(0x67f52c,0 if seed&8 else 1),(0x8cf97c,(seed>>4)%4)]:put(at,val)
  for at in [0x67dbe8]:put(at,next_word())
  put(0x4f8218,seed%4)
  for at in [0x6e5760,0x6e575c,0x6e55a8,0x67f530,0x67f548]:put(at,next_word())
  for at in [0x8cfa18,0x8cfa1c,0x8cf9a0]:put(at,seed%6-1)
  if seed&0x100:
   for off in [0x194,0x1a0,0x1b0,0x1b4,0x1bc]:put(0x6f3614+off,get(0x6e9a90+off))
  if seed&0x200:put(0x6f3614+0x1ac,get(0x6e9a90+0x1ac))
  if seed&0x400:put(0x6f3614+0x1a8,get(0x6e9a90+0x1a8))
  if seed&0x800:put(0x6f3614+8,0)
  if seed&0x1000:put(0x6e9a90+0x18c,0x80,1)
  if seed&0x2000:put(0x6e9a90+0x1c4,0x8005,2)
  for at,s in [(0x67f498,'pw' if seed&16 else ''),(0x6e573c,'named'),(0x6e571c,'default'),(0x67f478,'addr'),(0x67f4dc,'route'),(TEXT,'text')]:string(at,s)
 def watched(at,n):
  spans=[(0x6e8090,0x78ee54),(0x7cca80,0x7cca80+5*0x114),(0x7dde80,0x7dde80+5*0x114),(DEF,DEF+0xa00),(TEXT,TEXT+16),(0x67f478,0x67f4f0),(0x6e571c,0x6e575c)]
  if frame_size:spans.append((frame,frame+frame_size))
  spans += [(x,x+4)for x in [0x67f53c,0x67f524,0x6e5590,0x67f528,0x67f52c,0x8cf97c,0x67dbe8,0x4f8218,0x6e5760,0x6e575c,0x6e55a8,0x67f530,0x67f548,0x8cfa18,0x8cfa1c,0x8cf9a0,0x6e5764,0x6e5768]]
  return any(lo<=at and at+n<=hi for lo,hi in spans)
 ranges=[]
 for name in ['4438c0','448c30','445c80','446200','4482e0','446da0','448130']:
  record=json.loads(Path('.local/reports/mission-session-periodic-'+name+'-source.json').read_text());record=record.get('structuredContent',record)
  ranges += [(int(x['start'],16),int(x['end'],16))for x in record['normalized_result']['procedure']['body']['ranges']]
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def memory(uc,kind,at,n,value,_):
  nonlocal reads,writes,restore
  ip=u.reg_read(UC_X86_REG_EIP)
  if not active or failed or not any(lo<=ip<=hi for lo,hi in ranges)or not watched(at,n):return
  if kind==unicorn.UC_MEM_READ:
   trace.append(f'R{at},{n};');reads+=1
   if reads==row[3]:restore=(at,bytes(u.mem_read(at,n)));stop();return
   if row[2]&8 and reads==7:put(0x7dde80+0x24,get(0x7dde80+0x24)^0x12345678)
  else:
   value &= (1<<(8*n))-1;trace.append(f'W{at},{n},{value};');writes+=1
   if writes==row[4]:restore=(at,bytes(u.mem_read(at,n)));stop();return
   if row[2]&16 and writes==4:put(0x8cfa18,1)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def log_call(site,args):
  nonlocal calls
  trace.append('C'+str(site)+''.join(','+str(x)for x in args)+';');calls+=1
  if calls==row[5]:stop();return False
  return True
 def foreign(site):
  if failed:return
  sp=u.reg_read(UC_X86_REG_ESP)
  count={0x49a620:3,0x4423f0:3,0x441750:4,0x404cf0:2,0x446110:2,0x4550c0:4}.get(site)
  if site==0x493a80:count=3 if get(sp+8)==0x4cc570 else 14
  args=[get(sp+4+i*4)for i in range(count)]
  if not log_call(site,args):return
  result=0
  if site==0x49a620:
   u.mem_write(args[0],bytes([args[1]&255])*args[2]);result=args[0]
   if row[2]&1:put(0x67f53c,(get(0x67f53c)+1)%6)
  elif site in [0x4423f0,0x441750]:
   if row[2]&32:put(0x4f8218,MASK);put(0x6f3614+0x18c,1,1)
  elif site==0x404cf0:result=TEXT
  elif site==0x446110:string(args[0],'net')
  elif site==0x493a80:string(args[0],'formatted')
  r.finish(result)
 for site in [0x49a620,0x4423f0,0x441750,0x404cf0,0x446110,0x493a80,0x4550c0]:r.stubs[site]=lambda s=site:foreign(s)
 def code(uc,at,n,_):
  nonlocal top_seen
  if not active or failed:return
  if at in pending:
   site=pending.pop(at)
   if site==0x446da0 and row[2]&2:put(0x67f53c,(row[1]>>5)%6)
   if site==0x448130 and row[2]&4:put(0x67f53c,(get(0x67f53c)+1)%6)
  if at in [0x446da0,0x448130]:
   if not top_seen and at==ENTRY[row[0]]:top_seen=True;return
   sp=u.reg_read(UC_X86_REG_ESP);args=[] if at==0x446da0 else [get(sp+4),get(sp+8)]
   if log_call(at,args):pending[get(sp)]=at
 u.hook_add(unicorn.UC_HOOK_CODE,code)
 def snapshot():
  spans=[(frame,frame_size),(0x6e8090,0x1a00)]
  for i in range(17):spans += [(0x6e9a90+i*0x9b84+0x18c,1),(0x6e9a90+i*0x9b84+0x194,0x30)]
  spans += [(0x7cca80,5*0x114),(0x7dde80,5*0x114)]+[(at,4)for at in [0x67f53c,0x8cfa18,0x8cfa1c,0x4f8218]]
  return b''.join(bytes(u.mem_read(at,n))for at,n in spans if n).hex()
 seeds=list(range(1,33))+[0,37,41,49,53,61,0x101,0x301,0x701,0x805,0x1005,0x2005,0x80000105,0xffffffff]
 rows=[[mode,seed,mutation,0,0,0]for mode in range(7)for seed in seeds for mutation in [0,1,2,4,8,16,32]]
 for mode in range(7):
  seed=5 if mode in [2,4]else 17
  for field,limits in [(3,[1,2,7,50,449,500]),(4,[1,4,50,448,449]),(5,[1,2,3,4,5,8])]:
   for limit in limits:
    case=[mode,seed,0,0,0,0];case[field]=limit;rows.append(case)
 if a.cases is not None:rows=rows[:a.cases]
 expected=[]
 for row in rows:
  initialize();trace=[];reads=writes=calls=0;failed=False;restore=None;pending={};top_seen=False;active=True
  try:r.call(ENTRY[row[0]],*([0x6e9a90,0x6f3614]if row[0]==6 else []))
  except RuntimeError:
   if not failed:raise
  except unicorn.UcError as error:raise RuntimeError((row,str(error)))from error
  active=False
  if restore:u.mem_write(*restore)
  result=u.reg_read(UC_X86_REG_EAX)if row[0]==6 and not failed else 0
  expected.append(f'{int(failed)} {reads} {writes} {calls} {result} {snapshot()} '+''.join(trace))
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,row))for row in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,p.stderr;lines=p.stdout.splitlines();assert len(lines)==len(rows),(len(lines),len(rows))
 bad=[]
 for i,(native,original)in enumerate(zip(lines,expected)):
  if native!=original:
   # Keep short field/trace differences readable; complete fixture row is retained.
   ns=native.split(' ',6);os=original.split(' ',6);at=next((j for j,(x,y)in enumerate(zip(ns,os))if x!=y),None)
   bad.append({'row':rows[i],'field':at,'native':ns[at][:1400]if at is not None else native[:1400],'original':os[at][:1400]if at is not None else original[:1400]})
   if len(bad)==8:break
 report={'source_sha256':SOURCE_SHA256,'cases':len(rows),'differences':sum(x!=y for x,y in zip(lines,expected)),'mismatches':bad,'bodies':[hex(x)for x in ENTRY]};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k]for k in ['cases','differences']}));return int(bool(report['differences']))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int);p.add_argument('--report',type=Path,required=True);raise SystemExit(run(p.parse_args()))
