#!/usr/bin/env python3
"""Compare the closed provider with actual493A80 and its complete CRT call tree."""
import argparse,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_GDTR,UC_X86_REG_CS,UC_X86_REG_DS,UC_X86_REG_ES,UC_X86_REG_SS
from check_projectiles_original import Rig,SOURCE_SHA256
A=0x1200000;D=0x1300000
FORMATS=[0x4cae34,0x4cad48,0x4cadd0,0x4cad50,0x4cad5c,0x4cadd8,0x4cae08,0x4cadbc,0x4cae14,0x4cc67c,0x4cc570,0x4cc278,0x4ccc60,0x4ccc80]
TEXT=['dfcamp%02i.bin','Win%d','Lose%d','Briefing%d','Unable to find mission %d debriefing won text','Unable to find mission %d debriefing lose text','%s ~ ~ %s','%s ~ %s : %d ~ ~ %s',"Unable to find '%s'","WPN%03i","%i","%s %s","STR_PWNAME%02i","STR_SPWNAME%02i"]
def run(a):
 r=Rig(a.game);r.stubs.pop(0x493a80);u=r.u;u.mem_map(0x3000000,0x1000);u.mem_write(0x3000000,bytes(8)+bytes.fromhex('ffff0000009bcf00ffff00000093cf00'));u.reg_write(UC_X86_REG_GDTR,(0,0x3000000,23,0))
 for reg,selector in [(UC_X86_REG_CS,8),(UC_X86_REG_DS,16),(UC_X86_REG_ES,16),(UC_X86_REG_SS,16)]:u.reg_write(reg,selector)
 rng=random.Random(0x493a80);active=False;trace=[];pending=[];reads=writes=start_reads=0;failed=False;rejected=None;v=None;sources=[];lengths=[]
 def put(at,n):u.mem_write(at,bytes([n&255]))
 def get(at):return u.mem_read(at,1)[0]
 def flush(*_):
  while pending:pending.pop(0)()
 def memory(machine,kind,at,width,value,_):
  nonlocal reads,writes,start_reads,failed,rejected
  if not active or not(A<=at<A+0x100000 or D<=at<D+0x100000):return
  owned=(A<=at and at+width<=A+4096) or (D<=at and at+width<=D+4096)
  if kind==unicorn.UC_MEM_READ:
   reads+=1
   if reads==v[4] or not owned:trace.append(f'F{at},{width};');failed=True;machine.emu_stop();return
   n=get(at);trace.append(f'R{at},{width},{n};')
   if at==sources[0]:
    start_reads+=1
    if start_reads==2 and v[3]&2:pending.append(lambda:put(sources[0]+1,0))
   if at==sources[0]+lengths[0] and n==0:
    if v[3]&1:pending.append(lambda:put(sources[0],ord('Q')))
    if v[3]&8:pending.append(lambda:(put(at,ord('Y')),put(at+1,0)))
  else:
   writes+=1;n=value&((1<<(width*8))-1)
   if writes==v[5] or not owned:rejected=(at,bytes(u.mem_read(at,width)));trace.append(f'E{at},{width},{n};');failed=True;machine.emu_stop();return
   trace.append(f'W{at},{width},{n};')
   if writes==1 and v[3]&4:pending.append(lambda:put(sources[1],ord('X')))
 u.hook_add(unicorn.UC_HOOK_CODE,flush);u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 rows=[]
 for index in range(14):
  for value in [0,1,9,10,99,100,0x7fffffff,0x80000000,0xffffffff]:
   for layout in [0,1,2]:
    for mutation in [0,1,2,4,8,16]:rows.append([index,value,layout,mutation,0,0,'5354415453','4b494c4c53','5041594c4f4144'])
 for _ in range(a.cases):rows.append([rng.randrange(14),rng.getrandbits(32),rng.randrange(3),rng.randrange(32),rng.choice([0,1,2,6,12,20]),rng.choice([0,1,2,8,16,30]),*(rng.randbytes(rng.randrange(20)).hex() or '-' for _ in range(3))])
 expected=[]
 for v in rows:
  index,value,layout,mutation,_,_=v[:6];source=A+16 if layout==2 else A;dest=D if layout==0 else A+1 if layout==1 else A;sources=[source,A+128,A+256]
  u.mem_write(A,b'\xa5'*4096);u.mem_write(D,b'\xa5'*4096);lengths=[]
  for at,hexdata in zip(sources,v[6:]):
   data=bytes.fromhex(hexdata) if hexdata!='-' else b'';lengths.append(len(data));u.mem_write(at,data+b'\0')
  args=[value] if index<6 or index in (9,10,12,13) else [0 if mutation&16 else sources[0],sources[1]] if index in (6,11) else [0 if mutation&16 else sources[0],sources[1],value,sources[2]] if index==7 else [0 if mutation&16 else sources[0]]
  trace=[];pending=[];reads=writes=start_reads=0;failed=False;rejected=None;active=True
  try:r.call(0x493a80,dest,FORMATS[index],*args);flush()
  except (RuntimeError,unicorn.UcError):
   if not failed:raise
  finally:active=False
  if rejected:u.mem_write(*rejected)
  digest=1469598103934665603
  for at,n in sorted([(A,4096),(D,4096)]+[(at,len(s)+1) for at,s in zip(FORMATS,TEXT)]):
   for c in bytes(u.mem_read(at,n)):digest=((digest^c)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {0 if failed else u.reg_read(UC_X86_REG_EAX)} {digest} '+''.join(trace))
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,v)) for v in rows)+'\n',text=True,capture_output=True);assert not p.returncode,p.stderr;actual=p.stdout.splitlines();assert len(actual)==len(expected)
 diffs=[{'case':i,'input':rows[i],'actual':x,'expected':y} for i,(x,y) in enumerate(zip(actual,expected)) if x!=y]
 report={'source_sha256':SOURCE_SHA256,'body':'493A80 with full4B7158/4BD880/4BE184/4C1530/4B7140 CRTtree','cases':len(rows),'differences':len(diffs),'mismatches':diffs[:3],'coverage':'Exact current string reads and byte stores; integer/padding; return count; finalNUL; nullstrings; input/output overlap; postscan mutation; fieldmutation; read/writefailureprefix','normalization':'Pure immutable literal guard reads excluded; captured scalar args correspond to stable original call frames','CRT_stubs':False,'environment':'FlatWin32GDT CS8 DS/ES/SS16'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(diffs)}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=400);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
