#!/usr/bin/env python3
"""Replay lazy DF1 sample loads, conversion, allocation and stream routing."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn import UcError
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EBX,UC_X86_REG_EIP
from check_projectiles_original import Rig,SOURCE_SHA256
A=0x1300000;D=0x1302000;E=0x1301000
KEYS=[0xd09730,0xd096e0,0xd096d8,0xd09738,0xd09398,0x5013a0,0xd096d0,0x4cdff0,E]
FUNCTIONS=[0x49d050,0x494c50,0x498120,0x498230,0x4992d0,0x499280,0x499020,0x498d50]

def run(args):
 rig=Rig(args.game);rng=random.Random(0x49d050);rows=[];expected=[];counts=[0]*8;faults=[0]*8;active=False;access=[];calls={}
 def read(at):return rig.peek(at)&0xffffffff
 def put(at,v):rig.poke_u(at,v)
 def argv(n):return struct.unpack('<'+'I'*n,rig.u.mem_read(rig.u.reg_read(UC_X86_REG_ESP)+4,n*4))
 def interesting(at):return at in KEYS or A<=at<A+64 or D-16<=at<D+256
 def memory(u,kind,at,size,value,_):
  if active and interesting(at):access.append(f'R{at},{size};' if kind==unicorn.UC_MEM_READ else f'W{at},{size},{value&((1<<(size*8))-1)};')
 rig.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def wav(bits,length,tag,marked=False,silent=False):
  b=bytearray(rng.randbytes(256));struct.pack_into('<I',b,0,tag if marked else tag^0xffffffff);struct.pack_into('<I',b,0x18,rng.choice([11025,22050,44100,0xffffffff,0x80000001]));struct.pack_into('<H',b,0x22,bits);struct.pack_into('<I',b,0x28,length)
  if silent:b[0x2c:0x2c+length]=bytes(length)
  return b
 for mode in range(8):
  for case in range(args.cases):
   mutation=rng.randrange(64);allocated=rng.choice([0,D,D,D]);returned=rng.getrandbits(32);arg=rng.choice([0,1,64,96]);arg2=rng.getrandbits(32)
   cells={0xd09730:rng.randrange(2),0xd096e0:rng.randrange(2),0xd096d8:55,0xd09738:E,0xd09398:rng.randrange(2),0x5013a0:rng.getrandbits(32),0xd096d0:17,0x4cdff0:rng.getrandbits(32),E:77}
   bits=rng.choice([8,16,16,24]);length=rng.choice([0,1,2,4,8,31,32,64]);tag=cells[0x4cdff0]
   asset=bytearray(rng.randbytes(64));struct.pack_into('<I',asset,0x18,31);struct.pack_into('<I',asset,0x1c,108);asset[0x23]=rng.choice([0,0,0,1]);struct.pack_into('<I',asset,0x24,91)
   data=bytearray(rng.randbytes(16))+wav(bits,length,tag,case%7==0,case%9==0);incoming=wav(bits,length,tag,case%7==0,case%9==0)
   if mode==3:arg=rng.choice([0,D]);arg2=rng.choice([0,1,0xffffff,0x1000000,0xffffffff])
   rows.append([mode,arg,arg2,mutation,allocated,returned,*[cells[k] for k in KEYS],asset.hex(),data.hex(),incoming.hex()])
   for k,v in cells.items():put(k,v)
   rig.u.mem_write(A,bytes(asset));rig.u.mem_write(D-16,bytes(data));put(0x4c6854,0x1303000);trace=[];access=[];loop_fault=False
   def service(site,n):
    values=argv(n);trace.append('C'+str(site)+''.join(','+str(v) for v in values)+';');calls[hex(site)]=calls.get(hex(site),0)+1
    if site==0x497f60:
     if mutation&1:put(A+0x18,77);put(A+0x1c,100)
     if mutation&2:put(0x5013a0,91)
     rig.finish(allocated);return
    if site in (0x498d60,0x49e6e0):
     if mutation&4:put(A+0x1c,88);put(0xd09730,read(0xd09730)^1)
    if site in (0x498d30,0x49e6b0):
     rig.u.mem_write(values[0],bytes(incoming[:min(values[1],len(incoming))]))
     if mutation&8:rig.u.mem_write(A+0x23,b'\x09');put(A+0x24,91)
     if mutation&16:put(0x4cdff0,read(0x4cdff0)+1)
    if site==0x4c6854 and mutation&32:put(0xd096e0,77)
    rig.finish(returned)
    if site==0x4c6854:rig.u.reg_write(UC_X86_REG_ESP,rig.u.reg_read(UC_X86_REG_ESP)+4)
   def guard():
    nonlocal loop_fault
    count=rig.u.reg_read(UC_X86_REG_ECX)
    if not count or (rig.u.reg_read(UC_X86_REG_EIP)==0x494cfd and count&0x80000000):loop_fault=True;rig.u.emu_stop()
   for site,n in [(0x497f60,4),(0x498d60,2),(0x498d30,2),(0x49e6e0,3),(0x49e6b0,3)]:rig.stubs[site]=lambda site=site,n=n:service(site,n)
   rig.stubs[0x1303000]=lambda:service(0x4c6854,1);rig.stubs[0x494cc2]=guard;rig.stubs[0x494cfd]=guard
   fault=False;active=True
   try:rig.call(FUNCTIONS[mode],*([A,arg] if mode==0 else [D] if mode==1 else [arg] if mode==2 else [arg,arg2] if mode in (3,4) else [D,arg] if mode==5 else []))
   except (UcError,RuntimeError):
    if loop_fault:fault=True
    elif rig.u.reg_read(UC_X86_REG_EIP)==0x494d1b and rig.u.reg_read(UC_X86_REG_EBX)==0:fault=True
    else:raise
   finally:active=False
   result=rig.u.reg_read(UC_X86_REG_EAX) if mode!=0 and not fault else 0
   expected.append(' '.join(map(str,[int(fault),result,*[read(k) for k in KEYS]]))+' '+rig.u.mem_read(A,64).hex()+' '+rig.u.mem_read(D-16,272).hex()+' '+(''.join(trace) or '-')+' '+(''.join(access) or '-'));counts[mode]+=1;faults[mode]+=int(fault)
 process=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,r)) for r in rows)+'\n',text=True,capture_output=True)
 if process.returncode:raise RuntimeError(process.stderr)
 actual=process.stdout.splitlines();assert len(actual)==len(expected),(len(actual),len(expected))
 for case,(got,want) in enumerate(zip(actual,expected)):
  if got!=want:
   args.report.parent.mkdir(parents=True,exist_ok=True);args.report.with_suffix('.failure.json').write_text(json.dumps({'case':case,'input':rows[case],'actual':got,'expected':want},indent=2));g,w=got.split(),want.split();field=next(i for i,(a,b) in enumerate(zip(g,w)) if a!=b);detail={'case':case,'mode':rows[case][0],'field':field,'actual':g[field][:100],'expected':w[field][:100]}
   if field>=len(g)-2:
    a,b=g[field].split(';'),w[field].split(';');step=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)));detail.update({'step':step,'actual_step':a[step:step+4],'expected_step':b[step:step+4]})
   raise AssertionError(detail)
 report={'source_sha256':SOURCE_SHA256,'cases':len(rows),'mismatches':0,'bodies':[hex(f) for f in FUNCTIONS],'cases_by_body':counts,'conversion_faults_or_nonterminating_loop_guards':faults,'calls':calls,'comparison':'complete asset/allocation images,cells,ordered memory reads/writes with widths,required calls under mutations','foreign_fixtures':['497F60 allocation','498D30/498D60 loose file','49E6B0/49E6E0 archive','_lclose'],'audio_output':'none'}
 args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=300);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
