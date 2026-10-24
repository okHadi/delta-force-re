#!/usr/bin/env python3
"""Compare raw actor/resource cleanup with original instructions and declared providers."""
import argparse,hashlib,json,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_ESI
from check_projectile_hits_original import Replay,HASH
REGIONS=[(0x42fc5c,0x9edc),(0x6ba43c,0x1da14),(0x78f27c,0x14079c),(0xbfc0fc,0x3cd0),(0x500afc,4),(0x4cc990,32),(0xc23810,8),(0x1300000,0x2000)]
ENTRIES=[0x42cd30,0x439c40,0x439c10,0x42c950,0x42cf40,0x449620,0x47ec50,0x482910]
RANGES=list(zip(ENTRIES,[0x42cd58,0x439c61,0x439c37,0x42c99e,0x42cf54,0x449701,0x47eccb,0x48295a]))
class Fault(Exception):pass
def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path);a=p.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rows=[];expected=[];faults=0
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def put(at,v,w=4):r.u.mem_write(at,(v&((1<<(w*8))-1)).to_bytes(w,'little'))
 for case in range(a.cases):
  op=case%8;seed=case//8;mutation=(case//3)%16;fault=0 if case%3 else [1,2,3,4,7,63,66,100,10200,20000][(case//3)%10]
  for base,n in REGIONS:r.u.mem_write(base,bytes([seed&255])*n)
  for i in range(64):put(0x6ba4a0+i*100,0x1500000+i*32 if(i+seed)%4 else 0)
  for i in range(10):
   for off in (0,4,12,16):put(0xbfc100+i*0x614+off,0x1600000+i*128+off if(i+off+seed)%5 else 0)
  put(0xc23810,0x1300000);put(0xc23814,0xffffffff if seed%7==0 else 3)
  for i in range(8):put(0x4cc990+i,ord('A')+i if seed%3 else 0,1)
  put(0x4cc998,0,1);instance=(0x42fc60+[0,1,-1,155,156,255*156,256*156,0x80000000][seed%8])&0xffffffff
  rows.append(f'{op} {seed} {mutation} {fault}');trace=1469598103934665603;entries=operations=stores=calls=0;pending=[]
  def emit(kind,at,w,v):
   nonlocal trace,entries
   entries+=1
   for x in (ord(kind),at,w,v):
    for i in range(4):trace=((trace^((x>>(i*8))&255))*1099511628211)&0xffffffffffffffff
  def stop():
   nonlocal operations
   if fault:
    operations+=1
    if operations==fault:raise Fault()
  def access(u,kind,at,w,v,user):
   nonlocal stores
   pc=u.reg_read(UC_X86_REG_EIP)
   if not any(lo<=pc<=hi for lo,hi in RANGES):return
   if not any(b<=at and at+w<=b+n for b,n in REGIONS):
    if 0x2000000<=at<0x2010000:return
    raise Fault()
   if kind==unicorn.UC_MEM_READ:emit('R',at,w,get(at,w));pending.append(stop)
   else:
    emit('W',at,w,v&((1<<(w*8))-1));stores+=1
    if stores==1:
     if mutation&4:pending.append(lambda:put(0xc23810,0x1301000))
     if mutation&8:pending.append(lambda:put(0xc23814,1))
    pending.append(stop)
  def code(u,at,size,user):
   nonlocal calls
   while pending:pending.pop(0)()
   n={0x4980d0:1,0x49a620:3,0x42ebe0:0}.get(at)
   if n is None:return
   args=list(r.read(u.reg_read(UC_X86_REG_ESP)+4,n))if n else[];emit('C',at,n,0)
   for v in args:emit('A',0,0,v)
   stop();calls+=1
   if calls==1 and mutation&1:put(0x6ba504,0x1700000);put(0xbfc104,0x1700010)
   if at==0x49a620:r.u.mem_write(args[0],bytes([args[1]&255])*args[2])
   elif at==0x42ebe0 and mutation&2:put(0x4cc990,ord('Z'),1);put(0x4cc991,0,1)
   u.reg_write(UC_X86_REG_EAX,0xffffffff)
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)]
  for site in(0x4980d0,0x49a620,0x42ebe0):r.hook(site,lambda:None)
  failed=False
  try:
   if op==0:r.call(ENTRIES[op],instance if seed%9 else 0)
   elif op==1:
    # Replay.call clears registers; set ESI at the real entry, without replacing it.
    entry_hook=r.u.hook_add(unicorn.UC_HOOK_CODE,lambda u,at,size,user:u.reg_write(UC_X86_REG_ESI,instance)if at==0x439c40 else None)
    try:r.call(ENTRIES[op])
    finally:r.u.hook_del(entry_hook)
   else:r.call(ENTRIES[op])
   while pending:pending.pop(0)()
  except(Fault,unicorn.UcError):failed=True;faults+=1
  for h in hooks:r.u.hook_del(h)
  r.clear_hooks();digest=1469598103934665603
  for base,n in REGIONS:
   for byte in bytes(r.u.mem_read(base,n)):digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {digest} {trace} {entries}')
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',capture_output=True,text=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y]
 signed_edges=[]
 for instance,flag in [(0x42fbc4,0x43985f),(0x42fbc5,0x439860),(0x42fc5f,0x439860),(0x8042fc5f,0x115a580),(0x8042fc60,0xff718b40)]:
  for address in (instance+4,flag):
   if not any(lo<=address and address+4<=hi+1 for lo,hi,permissions in r.u.mem_regions()):r.u.mem_map(address&~4095,4096)
  put(instance+4,0x1357);put(flag,0xa5,1);edge_trace=[]
  def free_edge():
   assert list(r.read(r.u.reg_read(UC_X86_REG_ESP)+4,1))==[0x1357]
   edge_trace.append('release');r.u.reg_write(UC_X86_REG_ESI,0xdeadbeef)
  r.hook(0x4980d0,free_edge)
  def edge_code(u,at,size,user):
   if at==0x439c40:u.reg_write(UC_X86_REG_ESI,instance)
  def edge_write(u,kind,at,w,value,user):
   if u.reg_read(UC_X86_REG_EIP)==0x439c5a:edge_trace.append((at,w,value&255))
  hooks=[r.u.hook_add(unicorn.UC_HOOK_CODE,edge_code),r.u.hook_add(unicorn.UC_HOOK_MEM_WRITE,edge_write)]
  r.call(0x439c40)
  for hook in hooks:r.u.hook_del(hook)
  r.clear_hooks();assert edge_trace==['release',(flag,1,0)] and get(flag,1)==0
  signed_edges.append({'instance':hex(instance),'flag':hex(flag)})
 report={'source_sha256':HASH,'bodies':[hex(x)for x in ENTRIES],'cases':a.cases,'differences':len(diffs),'fault_prefixes':faults,'signed_edges':signed_edges,'comparison':'Complete declared current images and ordered R/W/call tuple FNV64 + tuple count. Original internal bodies execute. Free/clear/event reset are declared foreign fixtures. CPU saved registers/stack/flags excluded.','details':diffs[:4]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report));return bool(diffs)
if __name__=='__main__':raise SystemExit(main())
