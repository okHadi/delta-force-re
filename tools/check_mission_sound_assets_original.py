#!/usr/bin/env python3
"""Compare real DF1 sound-asset bodies using synthetic PWF graphs."""
import argparse,json,random,struct,subprocess,zlib
from pathlib import Path
from unicorn import UcError
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EDI,UC_X86_REG_EAX,UC_X86_REG_EIP
from check_projectiles_original import Rig,STACK,SOURCE_SHA256
B=0x1300000;C=B;N=B+0x100;S=B+0x200;A=B+0x400;ALT=B+0xc00;AB=B+0x1000;P=B+0x2000;Q=B+0x2200;R=B+0x2400
KEYS=[0xd096e4,0xd096d8,0xd0973c,0xd096e8,0xd096dc,0xd09730,0xd09734,0xd09738,0xd096e0,*range(0xd096f0,0xd09730,4),0x5014f8]
FUNCTIONS=[0x49ce00,0x49d8c0,0x49cfd0,0x49d510,0x49d020,0x49d0f0,0x498070,0x499120]

def put(image,at,n,width=4):image[at:at+width]=n.to_bytes(width,'little')
def fixture(rng,case):
 m=bytearray(0xa000);f=bytearray(0x1200);m[0x4000:0x5800]=rng.randbytes(0x1800)
 for at,t in [(0x100,b'SOUND.PWF'),(0x120,b'MAPPED.PWF'),(0x140,b'SAMPLE_1'),(0x160,b'BANK_1'),(0x180,b'sample_1'),(0x1a0,b'MISSING')]:m[at:at+len(t)]=t
 for ptr in (P,Q,R):put(m,ptr-B+0xac,1)
 samples=rng.choice([0,1,2]);banks=rng.choice([0,1,2]);layers=rng.choice([0,1,2]);members=rng.choice([0,1,2])
 if case%3:samples=2;banks=2;layers=2;members=2
 put(m,8,samples);put(m,0xc,1);put(m,0x10,0x400);put(m,0x1c,A);put(m,0x28,banks);put(m,0x2c,0x500);put(m,0x38,B+0x800);put(m,0xc4,0x123)
 put(f,0,0x24);put(f,8,samples);put(f,0xc,1);put(f,0x10,0x400)
 for i in range(2):
  record=bytearray(0x34);name=f'SAMPLE_{i}'.encode();record[:len(name)]=name;put(record,0x18,0xa00+i*0x80);put(record,0x1c,rng.choice([0,64,128]));record[0x22]=rng.randrange(2);record[0x23]=rng.randrange(2);put(record,0x24,B+0x9000+i*16)
  f[0x24+i*0x34:0x24+(i+1)*0x34]=record;m[0x400+i*0x34:0x400+(i+1)*0x34]=record;m[0xc00+i*0x34:0xc00+(i+1)*0x34]=record
 for offset,count in [(0x400,banks),(0x440,1)]:put(f,offset,0x14);put(f,offset+4,count);put(f,offset+8,0x500)
 for bank in range(2):
  rec=bytearray(0x50);name=f'BANK_{bank}'.encode();rec[4:4+len(name)]=name;put(rec,0x24,layers)
  for layer in range(2):
   off=0x700+bank*0x100+layer*0x40;put(rec,0x28+layer*4,off);l=bytearray(0x30);put(l,0,members,2);put(l,8,rng.choice([0,0x40,0x140]),2)
   for member in range(2):
    member_at=0xb00+bank*0x100+layer*0x40+member*0x20;put(l,0x10+member*4,member_at);r=bytearray(rng.randbytes(0x1c));put(r,0,member%max(1,samples));f[member_at:member_at+0x1c]=r
   f[off:off+0x30]=l
  f[0x500+bank*0x50:0x550+bank*0x50]=rec;m[0x800+bank*0x50:0x850+bank*0x50]=rec
 return m,f

def run(args):
 rig=Rig(args.game);rng=random.Random(0x49ce00);rows=[];expected=[];counts=[0]*8;calls={};failures=[];allocation_failures={};open_paths={};loose_fallbacks=0
 def read(at):return rig.peek(at)&0xffffffff
 def poke(at,n):rig.poke_u(at,n)
 def argv(n):return struct.unpack('<'+'I'*n,rig.u.mem_read(rig.u.reg_read(UC_X86_REG_ESP)+4,n*4))
 for mode in range(8):
  for case in range(args.cases):
   mutation=rng.randrange(2048)&~512;scenario=case%8;tag=rng.choice([0,0x123,0xffffff,0x1000000,0xffffffff]);success=rng.choice([1,2,3,17]);edi=rng.getrandbits(32)
   fail_at=rng.choice([0,0,1,2,3,4,5,6]);fail_at=0 if mode==0 and fail_at in (1,2) else fail_at
   query=rng.choice([N+64,N+96,N+128,N+160]);query=rng.choice([0,1,64,80]) if mode==6 else query
   m,f=fixture(rng,case)
   if scenario==6:put(m,0x10,0)
   if mode==0 and case%17==0:mutation|=512
   cells={k:0 for k in KEYS};cells.update({0xd096dc:case%2,0xd096d8:0 if scenario in (0,3,4) else P,0xd0973c:0 if scenario in (0,1) else 1,0xd096e4:int(scenario in (0,5)),0xd096e8:1,0xd096e0:91,0xd09730:92,0xd09734:93,0xd09738:94,0x5014f8:B+0x9010})
   if scenario!=4:cells[0xd096f0]=Q;cells[0xd096f4]=R
   rows.append([mode,mutation,fail_at,query,tag,scenario,success,edi,*[cells[k] for k in KEYS],m.hex(),f.hex()])
   for k,v in cells.items():poke(k,v)
   rig.u.mem_write(B,bytes(m));rig.u.mem_write(STACK-0x2000,bytes(0x2000));pos=0;heap=B+0x4000;allocations=0;last_layer=0;cache_calls=0;open_calls=0;trace=[]
   def event(site,values):trace.append('C'+str(site)+''.join(','+str(v) for v in values)+';');calls[hex(site)]=calls.get(hex(site),0)+1
   def service(site,n):
    nonlocal pos,heap,allocations,last_layer,cache_calls,open_calls,loose_fallbacks
    values=list(argv(n));raw_destination=values[0] if values else 0
    if site==0x499280 and STACK-0x2000<=raw_destination<STACK:values[0]=S
    event(site,values)
    if site==0x497f60:
     allocations+=1
     if allocations==fail_at:
      allocation_failures[hex(values[3])]=allocation_failures.get(hex(values[3]),0)+1;rig.finish(0);return
     out=heap+16;heap=(out+values[0]+15)&~15;assert heap<=B+0x8000
     if mutation&32 and values[3]==0x4ce458 and last_layer:rig.u.mem_write(last_layer,b'\1\0');poke(C+0x1c,ALT)
     rig.finish(out);return
    if site==0x499120:
     pos=0
     if mutation&1:
      open_calls+=1
      if open_calls==1:poke(C+0x1c,ALT);poke(C+8,1)
     rig.finish(0xffffffff if scenario==6 else 123);return
    if site==0x4992d0:
     pos=values[0]
     if mutation&4 and pos==0x500:poke(C+0x38,AB)
     rig.finish(pos);return
    if site==0x499280:
     destination,count=values;assert count<=0x1200 and pos+count<=len(f),(mode,case,pos,count)
     if not mutation&512:
      data=bytes(f[pos:pos+count]);rig.u.mem_write(raw_destination,data)
      if destination==S:rig.u.mem_write(S,data)
      pos+=count
     if mutation&2 and destination==S:poke(C+0x10,0x440)
     if mutation&64 and destination==C:poke(C+8,1)
     if count==0x30:last_layer=raw_destination
     rig.finish(0 if mutation&512 else count);return
    if site==0x499020:
     if mutation&1024:poke(0x5014f8,B+0x9020);poke(C+0xc4,0x77)
     rig.finish(0);return
    if site==0x49d050:
     asset=values[0];rig.u.mem_write(asset+0x23,b'\1');poke(asset+0x24,B+0x9000+cache_calls*16);cache_calls+=1
     if mutation&8:poke(C+0xc4,0x42);poke(C+8,1)
     if mutation&16:poke(C+0x1c,ALT)
     rig.finish(0);return
    if site==0x498230:
     if values[0] and values[1]<0x1000000:poke(values[0]-12,values[1])
     rig.finish(values[0]-16 if values[0] else 0);return
    if site==0x495160:rig.finish(0);return
    if site==0x498cf0:
     loose_fallbacks+=int(scenario!=0)
     poke(0xd096e8,0 if scenario==0 else 1)
     if mutation&1:poke(0xd096d8,P)
     rig.finish(123 if scenario==0 else 0xffffffff);return
    if site==0x498fb0:
     if mutation&128:poke(0xd096d8,Q);poke(0xd096f0,R)
     rig.finish(N+32);return
    if site==0x49e630:
     archive=values[1];open_calls+=1;poke(archive+0xac,(0 if scenario==1 else 1) if archive==P else (0 if open_calls==success else 1))
     if mutation&256:poke(0xd096d8,Q);poke(Q+0xac,0);poke(0xd096f0,R);poke(R+0xac,0)
     rig.finish(200+open_calls);return
    if site==0x499750:rig.finish(0);return
    raise AssertionError(hex(site))
   for site,n in [(0x497f60,4),(0x499120,1),(0x4992d0,2),(0x499280,2),(0x499020,0),(0x49d050,2),(0x498230,2),(0x495160,1),(0x498cf0,1),(0x498fb0,2),(0x49e630,2),(0x499750,2)]:rig.stubs[site]=lambda site=site,n=n:service(site,n)
   if mode==7:rig.stubs.pop(0x499120)
   rig.u.reg_write(UC_X86_REG_EDI,edi)
   arguments=[N,C,tag] if mode==0 else [0 if scenario==7 else C] if mode==1 else [query,C] if mode in (2,3,4) else [C] if mode==5 else [query,4,0,0x55,tag] if mode==6 else [N]
   try:rig.call(FUNCTIONS[mode],*arguments)
   except (UcError,RuntimeError) as error:
    failures.append({'mode':mode,'case':case,'eip':hex(rig.u.reg_read(UC_X86_REG_EIP)),'error':str(error)});raise
   if mode==7:
    path='error' if any(t.startswith('C'+str(0x499750)+',') for t in trace) else 'secondary' if read(0xd09730) else 'primary' if read(0xd096e0) else 'loose'
    open_paths[path]=open_paths.get(path,0)+1
   result=0 if mode==1 else rig.u.reg_read(UC_X86_REG_EAX);after=bytes(rig.u.mem_read(B,0xa000))
   expected.append(' '.join(map(str,[0,result,*[read(k) for k in KEYS],zlib.crc32(after)]))+' '+after[:0xc8].hex()+' '+after[0x4000:0x5800].hex()+' '+(''.join(trace) or '-'));counts[mode]+=1
 process=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,r)) for r in rows)+'\n',text=True,capture_output=True)
 if process.returncode:raise RuntimeError(f'After {len(process.stdout.splitlines())} rows: '+process.stderr)
 actual=process.stdout.splitlines();assert len(actual)==len(expected),(len(actual),len(expected));mismatches=[]
 for case,(got,want) in enumerate(zip(actual,expected)):
  if got!=want:
   g,w=got.split(),want.split();field=next(i for i,(a,b) in enumerate(zip(g,w)) if a!=b);detail={'case':case,'mode':rows[case][0],'field':field,'actual':g[field][:120],'expected':w[field][:120]}
   if field==len(g)-1:
    a,b=g[field].split(';'),w[field].split(';');step=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)));detail.update({'step':step,'actual_step':a[step:step+3],'expected_step':b[step:step+3]})
   mismatches.append(detail)
   if len(mismatches)<=8:args.report.with_name(args.report.stem+f'.failure-{case}.json').write_text(json.dumps({'case':case,'input':rows[case],'actual':got,'expected':want},indent=2))
 report={'source_sha256':SOURCE_SHA256,'cases':len(rows),'cases_by_body':counts,'bodies':[hex(f) for f in FUNCTIONS],'mismatches':mismatches,'calls':calls,'allocation_failures_by_label':allocation_failures,'open_path_cases':open_paths,'loose_open_failures_then_fallback':loose_fallbacks,'comparison':'whole fixture memory CRC32,complete context and allocated heap bytes,current cells,ordered foreign calls under mutations','foreign_fixtures':['497F60 allocator','499120 open except its own mode','499280 read','4992D0 seek','499020 close','49D050 sample cache','498230 cache header','495160 voice callback','498CF0 loose open','498FB0 archive name','49E630 archive open','499750 error'],'incoming_edi':'explicit per case','full_context_allocation_failure_scope':'sets,layers,members; direct498070 also tests zero allocation; null sample/trigger read destinations require a separate backend fault fixture','audio_output':'none'}
 args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({**report,'mismatches':mismatches[:12]}));assert not mismatches
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=120);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
