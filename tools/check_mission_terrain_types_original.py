#!/usr/bin/env python3
"""Compare terrain classifier reads and authored type rows with original code."""
import argparse,json,random,shlex,subprocess
from pathlib import Path
import unicorn
from check_projectiles_original import Rig,SOURCE_SHA256
MASK=(1<<64)-1
def run(a):
 r=Rig(a.game);active=False;h=0;mutation=0;count_reads=0;used=False;failed=False;failed_old=0
 def put(at,n,w=4):r.u.mem_write(at,(n&((1<<(8*w))-1)).to_bytes(w,'little'))
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def trace(kind,at,width,value=0):
  nonlocal h
  for n in [kind,at,width,value]:
   for byte in (n&0xffffffff).to_bytes(4,'little'):h=((h^byte)*1099511628211)&MASK
 def hook(u,kind,at,width,n,_):
  nonlocal count_reads,used,failed,failed_old
  if not active or not (0x1200000<=at<0x1200300 or 0xbd4f98<=at<0xbd4fa8 or at==0xbd527c or 0xbd5334<=at<0xbd5434):return
  if kind==unicorn.UC_MEM_READ:
   trace(1,at,width)
   if at==0xbd527c:
    count_reads+=1
    if mutation==1 and count_reads==1 or mutation==7 and count_reads==2:put(at,0)
    if mutation==9 and count_reads==1:put(at,0x80000000)
   if not used:
    if mutation==2 and at==0x1200002 or mutation==10 and at==0xbd4f98:put(0xbd527c,3 if mutation==10 else 0);used=True
    if mutation==3 and at==0xbd4f98 or mutation==4 and at==0xbd4f99:put(0x1200002,231,1);used=True
    if mutation==5 and 0xbd4f9b<=at<0xbd4fa8 and (at-0xbd4f9b)%4==0:put(at,99,1);used=True
    if mutation==11 and at==0x1200000:put(0xbd4f98,42,1);used=True
  else:
   trace(2,at,width,n&((1<<(8*width))-1))
   if mutation==8 and at==0xbd533b:failed=True;failed_old=get(at,1);r.u.emu_stop()
   if mutation==6 and at==0xbd5334:put(0x1200003,254,1)
 r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,hook)
 inputs=[];expected=[];rng=random.Random(0x47b4a0)
 rows=[(seed,count,m) for seed in [0,17,255] for count in [0,1,2,3,0x80000000,0xffffffff] for m in range(12)]
 rows += [(rng.getrandbits(32),rng.choice([0,1,2,3,0xffffffff]),rng.randrange(12)) for _ in range(a.cases)]
 for seed,count,mutation in rows:
  for i in range(768):put(0x1200000+i,(seed+i*37+(i>>2)*11)&255,1)
  for i in range(16):put(0xbd4f98+i,(seed*3+i*53)&255,1)
  put(0xbd527c,count);r.u.mem_write(0xbd5334,b'\xa5'*256)
  h=1469598103934665603;count_reads=0;used=False;failed=False;active=True
  try:r.call(0x47b4a0,0x1200000)
  except RuntimeError:
   if not failed:raise
  finally:active=False
  if failed:put(0xbd533b,failed_old,1)
  expected.append(' '.join(map(str,[int(failed),*r.u.mem_read(0xbd5334,256),h])));inputs.append(f'0 {seed} {count} {mutation}')
 palette=bytes((i*37)&255 for i in range(768)).hex()
 def field(name,values):return ' '.join([json.dumps(name),str(len(values)),*[json.dumps(s) for s in values]])
 config_inputs=[]
 values=['0','255','-1','256','4294967297','-4294967297','  +17x','x','2147483648']
 names=['dirt','grass','snow','cement','sand','packed_dirt','unknown','GRASS']
 for n in [0,1,2,99,100,101,104]:
  fs=[field('TyPe',[values[i%len(values)],values[(i+1)%len(values)],values[(i+2)%len(values)],names[i%len(names)]]) for i in range(n)]
  fs.extend([field('type',['1','2','dirt']),field('other',['1','2','3','grass'])])
  config_inputs.append(' '.join(['1',palette,str(len(fs)),*fs]))
 for i in range(24):config_inputs.append(' '.join(['1',palette,'1',field('type',[values[i%9],values[(i+2)%9],values[(i+4)%9],names[i%8]])]))
 if a.stock:
  result=subprocess.run([str(a.probe),'--stock-fixture',str(a.game),a.stock],capture_output=True,text=True,check=True);config_inputs.append(result.stdout.strip())
 for line in config_inputs:
  tokens=shlex.split(line);palette=bytes.fromhex(tokens[1]);n=int(tokens[2]);cursor=3;r.call(0x47aaa0);put(0xbd4f90,0xbd4f98)
  for _ in range(n):
   name=tokens[cursor];argc=int(tokens[cursor+1]);strings=[name,*tokens[cursor+2:cursor+2+argc]];cursor+=2+argc
   put(0x1200000,len(strings));
   for i,s in enumerate(strings):
    addr=0x1210000+i*1024;r.u.mem_write(addr,s.encode()+b'\0');put(0x1200004+i*4,addr)
   r.call(0x47ab00,0x1200000)
  r.u.mem_write(0x1200000,palette);r.call(0x47b4a0,0x1200000);expected.append(' '.join(map(str,[0,*r.u.mem_read(0xbd5334,256)])));inputs.append(line)
 result=subprocess.run([str(a.probe)],input='\n'.join(inputs)+'\n',capture_output=True,text=True);assert result.returncode==0,result.stderr
 actual=result.stdout.splitlines();assert len(actual)==len(expected),(len(actual),len(expected));diff=[{'case':i,'input':inputs[i][:300],'actual':x[:400],'expected':y[:400]} for i,(x,y) in enumerate(zip(actual,expected)) if x!=y]
 report={'source_sha256':SOURCE_SHA256,'classifier_cases':len(rows),'config_cases':len(config_inputs),'stock':a.stock,'differences':len(diff),'mismatches':diff[:3]};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['classifier_cases','config_cases','differences']}));assert not diff
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=100);p.add_argument('--stock');p.add_argument('--report',type=Path,required=True);run(p.parse_args())
