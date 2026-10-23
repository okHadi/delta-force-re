#!/usr/bin/env python3
"""Replay49A550 with current FILE cells and explicit character/lock providers."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import unicorn as uc
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectile_hits_original import Replay,HASH,STOP
from check_mission_script_registry_original import fnv
F,O=0x2201000,0x2202000

def run(a):
 source=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(source).hexdigest()==HASH
 r=Replay(source);r.write(0x501d64,STOP+0x100);r.write(0x501d68,STOP+0x200);rng=random.Random(0x49a550);rows=[];expected=[]
 state={}
 def effect(site):
  u=r.u;arg=r.read(u.reg_read(UC_X86_REG_ESP)+4)[0];state['trace'].append(f'C,{site},{arg};');value=0;mutation=state['mutation']
  if site==0x501d64:
   if mutation&1:r.write(F+12,r.read(F+12)[0]^0x30)
  elif site==0x4ba3c0:
   i=state['index'];chars=state['chars'];value=chars[i] if i<len(chars) else 0xffffffff;state['index']+=1
   if value==0xffffffff:r.u.mem_write(F+12,bytes([r.u.mem_read(F+12,1)[0]|(0x20 if mutation&2 else 0x10)]))
   if mutation&4 and state['index']==1:r.write(F+16,19)
  u.reg_write(UC_X86_REG_EAX,value)
 r.hook(STOP+0x100,lambda:effect(0x501d64));r.hook(STOP+0x200,lambda:effect(0x501d68));r.hook(0x4ba3c0,lambda:effect(0x4ba3c0))
 def access(u,kind,at,size,value,_):
  if not (F<=at and at+size<=F+32 or O<=at and at+size<=O+2048):return
  if kind==uc.UC_MEM_READ:value=int.from_bytes(u.mem_read(at,size),'little')
  state['trace'].append(('R' if kind==uc.UC_MEM_READ else 'W')+f',{at},{size},{value};')
 r.u.hook_add(uc.UC_HOOK_MEM_READ|uc.UC_HOOK_MEM_WRITE,access)
 for i in range(a.cases):
  limit=rng.choice([2,3,4,10,700,1024]);flags=rng.getrandbits(24);mutation=i%8;chars=[rng.randrange(256) for _ in range(rng.randrange(100))]
  if i%3==0:chars.insert(rng.randrange(len(chars)+1),0xffffffff)
  state.update(trace=[],mutation=mutation,index=0,chars=chars);r.u.mem_write(F,bytes(32));r.u.mem_write(O,bytes([0xa5])*2048);r.write(F+12,flags);r.write(F+16,7)
  result=r.call(0x49a550,O,limit,F);image=bytes(r.u.mem_read(F,32))+bytes(r.u.mem_read(O,2048));expected.append(f'{result} {fnv([image])} '+''.join(state['trace']));rows.append(' '.join(map(str,[limit,flags,mutation,len(chars),*chars])))
 done=subprocess.run([str(a.probe)],input='\n'.join(rows)+'\n',text=True,capture_output=True);assert done.returncode==0,done.stderr
 actual=done.stdout.splitlines();assert len(actual)==len(expected)
 for i,(x,y) in enumerate(zip(actual,expected)):assert x==y,(i,x,y)
 # The real nested character/refill bodies confirm the native CR/Ctrl-Z fixture.
 text=Replay(source);node,buffer=0x2204000,0x2206000;data=b'A\r\nB\rX\nC\x1aZ\n';position=0
 text.write(0x501d64,STOP+0x100);text.write(0x501d68,STOP+0x200)
 def zero_return():text.u.reg_write(UC_X86_REG_EAX,0)
 text.hook(STOP+0x100,zero_return);text.hook(STOP+0x200,zero_return);text.hook(0x4ba020,zero_return)
 def allocate():text.u.mem_write(buffer,bytes(4096));text.u.reg_write(UC_X86_REG_EAX,buffer)
 text.hook(0x49dc80,allocate)
 def read_file():
  nonlocal position
  esp=text.u.reg_read(UC_X86_REG_ESP);descriptor,destination,count=text.read(esp+4,3);assert descriptor==7
  chunk=data[position:position+count]
  if chunk:text.u.mem_write(destination,chunk)
  position+=len(chunk);text.u.reg_write(UC_X86_REG_EAX,len(chunk))
 text.hook(0x4ba6e0,read_file);text.u.mem_write(F,bytes(32));text.u.mem_write(node,bytes(32));text.write(F+8,node,1,7)
 text_results=[]
 for wanted,left in [(b'A\n',8),(b'BX\n',4),(b'C',2),(b'Z\n',0),(None,0)]:
  value=text.call(0x49a550,O,700,F);found=None
  if value:
   found=bytearray();at=O
   while (c:=text.u.mem_read(at,1)[0]):found.append(c);at+=1
   found=bytes(found)
  assert found==wanted and text.read(F+4)[0]==left,(found,wanted,text.read(F,6))
  text_results.append({'line':None if found is None else found.hex(),'remaining':left,'flags':text.read(F+12)[0]})
 report={'source_sha256':HASH,'original':'49A550','cases':a.cases,'differences':0,'comparison':'FullFILE/output bytes and ordered reads/writes/calls; source-time flags/descriptor mutations and EOF/error/line limits.','foreign':'4BA3C0 character provider and current501D64/68 locks for mutation cases.','nested_text_original':text_results,'nested_bodies':['49A550','4BA3C0','4BA490','4BA4C0','4BA640'],'nested_foreign':['4BA020 regular-file status','49DC80 buffer allocator','4BA6E0 raw OS-read provider','501D64/68 locks']};print(json.dumps(report,indent=2))
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--game',type=Path,required=True);p.add_argument('--probe',type=Path,required=True);p.add_argument('--cases',type=int,default=1024);p.add_argument('--report',type=Path);run(p.parse_args())
