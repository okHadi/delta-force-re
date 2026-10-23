#!/usr/bin/env python3
"""Compare live429EA0 against actual instructions and current read fixtures."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EDX,UC_X86_REG_EBX,UC_X86_REG_ECX
from check_projectiles_original import Rig,SOURCE_SHA256
A=0x1200000;D=0x1210000
def run(args):
 rig=Rig(args.game);rng=random.Random(0x429ea0);rows=[];expected=[];active=False;trace=[];v=None;sines=0;grounds=0;definitions=0;failed=False;faults=0
 def put(at,n,width=4):rig.u.mem_write(at,(n&((1<<(width*8))-1)).to_bytes(width,'little'))
 def read(at):return int.from_bytes(rig.u.mem_read(at,4),'little')
 def invalid_division():
  at=rig.u.reg_read(UC_X86_REG_EIP)
  if at not in [0x42a041,0x42a158]:return False
  denominator=rig.u.reg_read(UC_X86_REG_EBX if at==0x42a041 else UC_X86_REG_ECX)
  if denominator>=0x80000000:denominator-=0x100000000
  numerator=(rig.u.reg_read(UC_X86_REG_EDX)<<32)|rig.u.reg_read(UC_X86_REG_EAX)
  if numerator>=0x8000000000000000:numerator-=0x10000000000000000
  if denominator==0:return True
  quotient=abs(numerator)//abs(denominator)
  if (numerator<0)!=(denominator<0):quotient=-quotient
  return not -0x80000000<=quotient<=0x7fffffff
 def foreign(site):
  nonlocal sines,grounds,failed
  sp=rig.u.reg_read(UC_X86_REG_ESP);a=[read(sp+4+i*4) for i in range(3 if site==0x477820 else 2)];values=[a[0],0,0,0,0,0] if site==0x477820 else [a[0],a[1],0,0,0,0]
  trace.append('C'+str(site)+''.join(','+str(n) for n in values)+';')
  if site==0x477820:
   if sines==0 and v[10]&1:put(A+0x24,read(A+0x24)+123);put(A+0x28,read(A+0x28)-456)
   sines+=1;put(a[1],v[11]);put(a[2],v[12]);rig.finish();return
  call=grounds;grounds+=1
  if call==0 and v[10]&2:put(A+0x24,read(A+0x24)+1000);put(A+0x28,read(A+0x28)-2000)
  if call==1 and v[10]&4:put(A+0x7c,0x123);put(A+0x34,0x9876)
  if call==2 and v[10]&16:failed=True;rig.u.emu_stop();return
  rig.finish((v[13]+call*0x13579)&0xffffffff)
 rig.stubs[0x477820]=lambda:foreign(0x477820);rig.stubs[0x47df40]=lambda:foreign(0x47df40)
 def memory(u,kind,at,size,value,_):
  nonlocal definitions
  if not active:return
  if at in [D+0xe4,D+0xdc]:
   trace.append(f'D1,{at-D};')
   if at==D+0xe4:definitions+=1
   elif v[10]&128:put(A+0x6c,read(A+0x6c)^4,1)
  if 0xbd0068<=at<0xbd0068+2561*4:trace.append(f'G{at};')
  if at==0x7936bc:
   trace.append('T0,60;')
   if v[10]&32:put(A+0x7c,0x80000000)
 def code(u,at,size,_):
  if active and at==0x42a041 and definitions==1 and v[10]&8:put(D+0xe4,0x40000)
 rig.u.hook_add(unicorn.UC_HOOK_MEM_READ,memory);rig.u.hook_add(unicorn.UC_HOOK_CODE,code)
 for index in range(2561):put(0xbd0068+index*4,0xb16b00b5^(index*0x10203))
 edges=[0,1,0x7fffffff,0x80000000,0xffffffff]
 for speed in edges:
  for target in edges:
   for limit in edges:
    for mutation in [0,1,2,4,8,16,32,128,3,12,40]:rows.append([0,4,0,speed,target,0x12345678,0x76543210,limit,0x20000,0x100000,mutation,0x100000,0x200000,0x7fffffff,0xffffff00,0x123400])
 for _ in range(args.cases):rows.append([0,rng.choice([0,4]),0,rng.choice(edges),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.choice([0,1,0x10000,0x20000,0xffffffff]),rng.choice(edges),rng.randrange(256),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32)])
 for v in rows:
  image=bytearray(0x114);struct.pack_into('<I',image,0,1);struct.pack_into('<I',image,4,D);image[0x6c]=v[1]&255
  for at,n in [(0x7c,v[3]),(0x88,v[4]),(0x30,v[5]),(0x8c,v[6]),(0x24,v[14]),(0x28,v[15])]:struct.pack_into('<I',image,at,n)
  rig.u.mem_write(A,bytes(image));put(D+0xdc,v[7]);put(D+0xe4,v[8]);put(0x7936bc,v[9]);trace=[];sines=0;grounds=0;definitions=0;failed=False;active=True
  try:rig.call(0x429ea0,A)
  except unicorn.UcError:
   assert invalid_division();failed=True;faults+=1
  except RuntimeError:
   if not failed:
    if invalid_division():failed=True;faults+=1
    else:raise RuntimeError(f'Original move stopped at{rig.u.reg_read(UC_X86_REG_EIP):08x}, input{v}, calls{trace}')
  finally:active=False
  image=bytearray(rig.u.mem_read(A,0x114));struct.pack_into('<I',image,4,1)
  expected.append(str(int(failed))+' '+image.hex()+' '+''.join(trace))
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True)
 if p.returncode:raise RuntimeError(p.stderr)
 actual=p.stdout.splitlines();assert len(actual)==len(expected)
 differences=[]
 for i,(a,b) in enumerate(zip(actual,expected)):
  if a!=b:
   detail={'case':i,'input':rows[i],'actual':a,'expected':b};differences.append(detail)
   if len(differences)<4:args.report.with_name(args.report.stem+f'.failure-{i}.json').write_text(json.dumps(detail,indent=2))
 report={'source_sha256':SOURCE_SHA256,'body':'429EA0 live join','cases':len(rows),'differences':len(differences),'mismatches':differences[:5],'original_divide_faults':faults,'comparison':'whole actor image,source-order definition/type/sine reads,all sine/terrain call arguments,current callback changes and partial failures','foreign_bodies':'explicit sine/terrain fixtures; no foreign body proof claimed','audio_output':'none'}
 args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences','original_divide_faults']}));assert not differences
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
