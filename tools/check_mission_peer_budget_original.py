#!/usr/bin/env python3
"""Compare actual444E30 instructions, including exact shared access order."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn import UcError
from unicorn.x86_const import UC_X86_REG_EIP
from check_projectiles_original import Rig,SOURCE_SHA256
C=0x6e9a90;A=0x1301000;KEYS=[0x6e5570,0x6e5574,0x6e5578,0x6e557c,0x6e5580,0x6e5584]

def run(args):
 rig=Rig(args.game);rng=random.Random(0x444e30);rows=[];expected=[];active=False;trace=[];mutation=0;faults=0;count_bins={};write_branches={};edges=[0,1,4,5,14,15,59,60,61,100,0x7fffffff,0x80000000,0xffffffff]
 def put(at,n,width=4):rig.u.mem_write(at,(n&((1<<(width*8))-1)).to_bytes(width,'little'))
 def read(at,width=4):return int.from_bytes(rig.u.mem_read(at,width),'little')
 def interesting(at):return at in KEYS or C<=at<C+17*0x9b84 or A<=at<A+17*0x114
 def read_effect(at):
  if mutation&32 and at==A+0x6c:put(C+0x9b84+0x18c,1,1);put(C+0x9b84+0x1c4,5,2);put(C+0x9b84+8,A+0x114);put(A+0x114+0x6c,0,1)
  if mutation&64 and at==C+0x18c:put(0x6e5570,10)
  if mutation&128 and at==0x6e5578:put(0x6e5584,0xfffffffe)
 def write_effect(at):
  if mutation&1 and at==0x6e5580:put(0x6e5570,9)
  if mutation&2 and at==0x6e5584:put(0x6e5574,77)
  if mutation&4 and at==0x6e557c:put(0x6e5580,1)
  if mutation&8 and at==0x6e5578:put(0x6e5584,2)
  if mutation&16 and at==0x6e5578:put(0x6e557c,0x7fffffff)
 def memory(u,kind,at,size,value,_):
  if active and interesting(at):
   if kind==unicorn.UC_MEM_READ:trace.append(f'R{at},{size};');read_effect(at)
   else:trace.append(f'W{at},{size},{value&((1<<(size*8))-1)};');write_effect(at)
 def unmapped(u,kind,at,size,value,_):
  if active:trace.append(f'R{at},{size};')
  return False
 rig.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory);rig.u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED,unmapped)
 pairs=[(a,b) for a in edges for b in edges];pairs += [(rng.getrandbits(32),rng.getrandbits(32)) for _ in range(args.cases)]
 for case,(initial,budget) in enumerate(pairs):
  mutation=0 if case<len(edges)**2 else rng.randrange(256);values=[initial,budget,*[rng.getrandbits(32) for _ in range(4)]];clients=[];wanted_count=case%18
  for i in range(17):
   active_client=int(i<wanted_count);state=5;present=1;flags=0
   if case%4==1:active_client=rng.randrange(2);state=rng.choice([0,4,5,5,0xffff]);flags=rng.randrange(256)
   if case>=len(edges)**2 and case%23==0 and i==5:active_client=1;state=5;present=0
   clients.append([active_client,state,present,flags])
  rows.append([mutation,*values,*[n for client in clients for n in client]])
  for at,n in zip(KEYS,values):put(at,n)
  for i,(yes,state,present,flags) in enumerate(clients):client=C+i*0x9b84;put(client+0x18c,yes,1);put(client+0x1c4,state,2);put(client+8,A+i*0x114 if present else 0);put(A+i*0x114+0x6c,flags,1)
  trace=[];active=True;fault=False
  try:rig.call(0x444e30)
  except UcError:
   assert rig.u.reg_read(UC_X86_REG_EIP)==0x444edd and read(C+5*0x9b84+8)==0;fault=True
  finally:active=False
  faults+=fault
  selected=[read(at) for at in KEYS];count_bins[str(selected[4])]=count_bins.get(str(selected[4]),0)+1
  sequence=''.join(t[1:].split(',')[0]+',' for t in trace if t.startswith('W'));write_branches[sequence]=write_branches.get(sequence,0)+1
  after=[]
  for i in range(17):client=C+i*0x9b84;after += [read(client+0x18c,1),read(client+0x1c4,2),read(client+8),read(A+i*0x114+0x6c,1)]
  expected.append(' '.join(map(str,[int(fault),*selected,*after]))+' '+''.join(trace))
 process=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,r)) for r in rows)+'\n',text=True,capture_output=True)
 if process.returncode:raise RuntimeError(process.stderr)
 actual=process.stdout.splitlines();assert len(actual)==len(expected),(len(actual),len(expected));mismatches=[]
 for case,(got,want) in enumerate(zip(actual,expected)):
  if got!=want:
   g,w=got.split(),want.split();field=next(i for i,(a,b) in enumerate(zip(g,w)) if a!=b);detail={'case':case,'field':field,'actual':g[field][:150],'expected':w[field][:150]}
   if field==len(g)-1:
    a,b=g[field].split(';'),w[field].split(';');step=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)));detail.update({'step':step,'actual_step':a[step:step+4],'expected_step':b[step:step+4]})
   mismatches.append(detail)
   if len(mismatches)<4:args.report.with_name(args.report.stem+f'.failure-{case}.json').write_text(json.dumps({'input':rows[case],'actual':got,'expected':want},indent=2))
 report={'source_sha256':SOURCE_SHA256,'body':'444E30','cases':len(rows),'mismatches':mismatches,'invalid_actor_original_read_faults':faults,'write_orders':write_branches,'comparison':'all shared cells,17 current client/actor fields,complete ordered read/write addresses,widths,values,callback mutations and fault state','arithmetic':'wrapping32-bit,unsigned divides/comparison,signed branches/clamps','foreign_bodies':[],'audio_output':'none'}
 args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report));assert not mismatches
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
