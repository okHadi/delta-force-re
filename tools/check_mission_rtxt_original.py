#!/usr/bin/env python3
"""Compare raw RTXT fixup/load/named/index bodies and current-memory order."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256
A=0x1200000;B=0x1201000;Q=0x1203000;K=0x1203100;F=0x1203200;G=0x499e70

def put(p,at,n,width=4):p[at:at+width]=(n&((1<<(width*8))-1)).to_bytes(width,'little')
def text(p,at,s):p[at:at+len(s)+1]=s.encode()+b'\0'
def raw_file(count):
 p=bytearray([0xcc])*1024;put(p,0,0x54585452);put(p,4,0x100);put(p,8,0x12345678);put(p,12,count)
 for i in range(3):
  for off,n in [(16+i*16,i*4),(20+i*16,0x100+i),(24+i*16,0x200+i),(28+i*16,0x300+i)]:put(p,off,n)
 for at,s in [(0x40,'one'),(0x44,'two'),(0x48,'three'),(0x114,'Alpha'),(0x11a,'Beta'),(0x184,'KEY'),(0x188,'Key'),(0x194,'OTHER')]:text(p,at,s)
 for at,n in [(0x100,2),(0x104,0x80),(0x108,2),(0x10c,0x90),(0x110,1)]:put(p,at,n)
 return p

def run(args):
 rig=Rig(args.game);u=rig.u;active=False;v=None;trace=[];global_reads=0;failed=False
 def wr(at,n,width=4):u.mem_write(at,(n&((1<<(width*8))-1)).to_bytes(width,'little'))
 def rd(at,width=4):return int.from_bytes(u.mem_read(at,width),'little')
 def valid(at,width):return any(at>=base and at-base+width<=size for base,size in [(A,1024),(B,1024),(G,0x28),(Q,0x400)])
 def memory(u,kind,at,width,value,_):
  nonlocal global_reads,failed
  if not active or 0x2000000<=at<0x2010000:return
  writing=kind==unicorn.UC_MEM_WRITE
  if writing:trace.append(f'W{at},{width},{value&0xffffffff};')
  else:trace.append(f'R{at},{width};')
  if not valid(at,width):failed=True;u.emu_stop();return
  mutation=v[1]
  if writing:
   if at==G+0x14 and mutation&64:wr(A+0x100,0)
  else:
   if at==G:
    global_reads+=1
    if mutation&4 and global_reads==2:wr(G,B)
   if at==G+4 and mutation&1:wr(G+4,2)
   if at==G+8 and mutation&32:wr(G+8,0xdeadbeef)
   if at==G+0x18 and mutation&16:wr(G+0x18,1)
   if at==G+0x14 and mutation&128:wr(G+0x14,A+0x10c)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(u,kind,at,width,value,_):
  nonlocal failed
  if active:trace.append(f'R{at},{width};');failed=True
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED,invalid)
 def load():
  from unicorn.x86_const import UC_X86_REG_ESP
  name=rd(u.reg_read(UC_X86_REG_ESP)+4);trace.append(f'C{0x499410},{name};')
  if v[1]&256:wr(A+12,2)
  rig.finish(v[5])
 rig.stubs[0x499410]=load
 rng=random.Random(0x499ea0);rows=[]
 for mode in [0,1]:
  for mutation in [0,1,4,32,256,5,37,257]:
   for loaded in ([A] if mode==0 else [A,0,0xffffffff,0x80000000]):rows.append([mode,mutation,0,0,0,loaded,3])
 for mode in [0,1]:
  for count in [0,1,2]:rows.append([mode,0,0,0,0,A,count])
 for mode in [2,3]:
  for count in [0,3,0x7fffffff,0x80000000,0xffffffff]:
   for index in ([0,1] if mode==2 else [0,1,2,3,0x7ffffffe,0x7fffffff,0x80000000,0xffffffff]):
    for group in ([0,1,2,3,4] if mode==2 else [0,1]):
     for key in ([0,1,2,3,4,5] if mode==2 else [0]):rows.append([mode,0,index,group,key,A,count])
 for mutation in [2,4,8,32,64]:
  for group in [0,2]:
   for key in [0,1,2]:rows.append([2,mutation,0,group,key,A,3])
 for mutation in [16,128,144,192]:rows.append([2,mutation,0,0,2,A,3])
 for _ in range(args.cases):
  mode=rng.randrange(4);mutation=rng.choice([0,1,4,32,256]) if mode<2 else rng.choice([0,32,64]);rows.append([mode,mutation,rng.choice([0,1,2,3,0x80000000,0xffffffff]) if mode==3 else 0,rng.randrange(5) if mode==2 else rng.randrange(2),rng.randrange(6),rng.choice([A,0,0xffffffff,0x80000000]),3])
 expected=[]
 for v in rows:
  mode,mutation,index,group,key,loaded,count=v;a=raw_file(count);b=raw_file(count);put(b,4,B+0x100);strings=bytearray(0x400);text(strings,0,['Alpha','alpha','Beta','Missing','Missing'][group]);text(strings,0x100,['KEY','Key','OTHER','key','Missing','Missing'][key]);text(strings,0x200,'test.bin');u.mem_write(A,bytes(a));u.mem_write(B,bytes(b));u.mem_write(Q,bytes(strings));u.mem_write(G,bytes([0xa5])*0x28)
  if mode>=2:
   wr(A+12,3);u.reg_write(UC_X86_REG_EAX,A);rig.call(0x499ea0);wr(A+12,count);u.mem_write(G,bytes([0xa5])*0x28)
  trace=[];global_reads=0;failed=False;active=True
  try:
   if mode==0:u.reg_write(UC_X86_REG_EAX,A);rig.call(0x499ea0)
   elif mode==1:rig.call(0x499f1f,F)
   elif mode==2:rig.call(0x49a092,0 if index else A,G+12 if group==4 else Q,G+16 if key==5 else K)
   else:rig.call(0x49a076,0 if group else A,index)
  except (unicorn.UcError,RuntimeError):
   if not failed:raise
  finally:active=False
  result=0 if failed else u.reg_read(UC_X86_REG_EAX)
  expected.append(f'{int(failed)} {result} '+bytes(u.mem_read(A,1024)).hex()+' '+bytes(u.mem_read(B,1024)).hex()+' '+bytes(u.mem_read(G,0x28)).hex()+' '+''.join(trace))
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True);assert p.returncode==0,p.stderr;actual=p.stdout.splitlines();assert len(actual)==len(expected);bad=[]
 for i,(a,b) in enumerate(zip(actual,expected)):
  if a!=b:
   bad.append({'case':i,'input':rows[i],'actual':a,'expected':b})
   if len(bad)<4:args.report.with_name(args.report.stem+f'.failure-{i}.json').write_text(json.dumps(bad[-1],indent=2))
 report={'source_sha256':SOURCE_SHA256,'bodies':['499EA0','499F1F','499F08','49A092','499FA5','499F84','499F9D','49A076','499F60'],'cases':len(rows),'differences':len(bad),'mismatches':bad[:2],'comparison':'whole2rawallocations,499E70..94globalcells,returnvalue,exactorderedreads/writes/filecalls,currentcount/group/index/table/published-resultaliases,case-sensitivegroups/keys,signedindex/count,null/faultprefix,failedloads'};args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
