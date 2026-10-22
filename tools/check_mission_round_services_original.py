#!/usr/bin/env python3
"""Compare DF1 round services, field access order, and file/time boundaries."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EDI,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256

CELLS=[0x503428,0x67f520,0x67f53c,0x67f52c,0x6e5588,0x6e5570]
BASE=0x6e9a90;STRIDE=0x9b84;SQUADS=0x6e8090
FORMATS={0x4cc72c:'s',0x4cc748:'iiiii',0x4cc764:'',0x4cc784:'',0x4cc7a8:'',0x4cc7c8:'',0x4cc7e8:'',0x4cc804:'',0x4cc81c:'',0x4cc830:'',0x4cc840:'i',0x4cc854:'ii',0x4cc86c:'i',0x4cc884:'i',0x4cc898:'ii',0x4cc8b0:'i',0x4cc8c8:'',0x4cc8f8:'isiiii',0x4cc918:'isiii',0x4cc934:''}

def run(args):
 rig=Rig(args.game);rng=random.Random(0x448d20);rows=[];expected=[];count=[0]*5;print_counts={};active=False;access=[]
 def read(at):return rig.peek(at)&0xffffffff
 def put(at,n):rig.poke_u(at,n)
 def argv(n):return struct.unpack('<'+'I'*n,rig.u.mem_read(rig.u.reg_read(UC_X86_REG_ESP)+4,n*4))
 def text(at):
  value=bytearray()
  while True:
   b=rig.u.mem_read(at+len(value),1)[0]
   if not b:return value.decode('ascii')
   value.append(b)
 def interesting(at):
  if at in CELLS or SQUADS<=at<BASE:return True
  if BASE<=at<BASE+17*STRIDE:
   offset=(at-BASE)%STRIDE
   return offset<512 and not 0xc<=offset<=0x2b
  return False
 def memory(u,kind,address,size,value,_):
  if active and interesting(address):
   access.append(('R'+str(address)+','+str(size)+';') if kind==unicorn.UC_MEM_READ else ('W'+str(address)+','+str(size)+','+str(value&((1<<(size*8))-1))+';'))
 rig.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 for mode in range(5):
  for case in range(args.cases):
   mutation=rng.randrange(256);arg=rng.getrandbits(32) if mode==0 else rng.choice([0,1,2,3,0xffffffff]);first=rng.randrange(17);second=rng.randrange(17);edi=rng.choice([0,1,2,63]);opened=rng.choice([0,123,123,123])
   cells={0x503428:rng.choice([0,1,1,0x100]),0x67f520:rng.choice([0,1,1]),0x67f53c:rng.choice([0,1,2,3,4,5,6,0xffffffff]),0x67f52c:rng.randrange(2),0x6e5588:rng.getrandbits(32),0x6e5570:rng.getrandbits(32)}
   if mode==3 and case%3:cells[0x503428]=cells[0x67f520]=1
   squads=rng.randbytes(0x1a00);clients=[]
   for i in range(17):
    c=bytearray(rng.randbytes(512));c[0x18c]=rng.randrange(2);c[0x18d]=rng.randrange(2);struct.pack_into('<H',c,0x1c4,rng.choice([0,2,5,5,5]));struct.pack_into('<I',c,0x9c,rng.choice([0,1,2,63]));c[0xc:0x2c]=bytes(32);name=f'CLIENT_{i}'.encode();c[0xc:0xc+len(name)]=name;clients.append(c)
   if mode==2 and case%4!=0:
    game=case%6;cells[0x67f53c]=game
    offset=0x1a0 if game<=2 else 0x194 if game<=4 else 0x1bc
    common=rng.getrandbits(32)
    for slot in (first,second):
     struct.pack_into('<I',clients[slot],offset,common)
     if game<=2:
      struct.pack_into('<I',clients[slot],0x1b0,0);struct.pack_into('<I',clients[slot],0x1b4,0)
     if case%4 in (2,3):struct.pack_into('<I',clients[slot],0x1ac,common)
     if case%4==3:struct.pack_into('<I',clients[slot],0x1a8,common)
   rows.append([mode,arg,first,second,edi,mutation,opened,*[cells[a] for a in CELLS],squads.hex(),*[c.hex() for c in clients]])
   for a,n in cells.items():put(a,n)
   rig.u.mem_write(SQUADS,squads)
   for i,c in enumerate(clients):rig.u.mem_write(BASE+i*STRIDE,bytes(c))
   rig.u.mem_write(0x67f478,b'GAME\0');put(0x4c6764,0x1302000);trace=[];access=[];row_prints=0
   def open_file():
    assert tuple(text(a) for a in argv(2))==('scores.txt','a');trace.append('Oscores.txt,a;')
    if mutation&1:put(0x67f53c,((read(0x67f53c)+1)&0xffffffff)%7);at=BASE+0x18c;rig.u.mem_write(at,bytes([rig.u.mem_read(at,1)[0]^1]))
    rig.finish(opened)
   def local_time():
    dst=argv(1)[0];trace.append('T;');rig.u.mem_write(dst,struct.pack('<8H',2026,10,4,22,18,3,4,500));rig.finish();rig.u.reg_write(UC_X86_REG_ESP,rig.u.reg_read(UC_X86_REG_ESP)+4)
   def output():
    nonlocal row_prints
    file,fmt=argv(2);assert file==opened
    values=argv(2+len(FORMATS[fmt]))[2:];trace.append('P'+str(file)+','+str(fmt)+''.join(',S'+text(value) if kind=='s' else ',I'+str(value) for kind,value in zip(FORMATS[fmt],values))+';');print_counts[hex(fmt)]=print_counts.get(hex(fmt),0)+1
    if mutation&2 and fmt==0x4cc72c:put(0x67f53c,4)
    if mutation&4 and fmt==0x4cc748:put(0x67f53c,5)
    if mutation&8 and fmt in (0x4cc804,0x4cc81c,0x4cc830):put(0x67f53c,2);put(SQUADS+0xb4,123)
    if mutation&16 and fmt in (0x4cc840,0x4cc854,0x4cc86c):put(0x67f53c,4);put(SQUADS+0x114,0xffffffc3)
    if mutation&32 and fmt==0x4cc8c8:
     at=BASE+16*STRIDE+0x18c;rig.u.mem_write(at,bytes([rig.u.mem_read(at,1)[0]^1]))
    if mutation&64 and fmt in (0x4cc8f8,0x4cc918):
     if row_prints==0:rig.u.mem_write(BASE+16*STRIDE+0x18c,b'\1');put(BASE+16*STRIDE+0x194,0xffffffc3);put(0x67f53c,4)
     row_prints+=1
    rig.finish()
   def close_file():
    file=argv(1)[0];trace.append('X'+str(file)+';')
    if mutation&128:put(0x6e5588,17)
    rig.finish()
   rig.stubs.update({0x49a41c:open_file,0x49bac0:output,0x49a840:close_file,0x1302000:local_time})
   rig.u.reg_write(UC_X86_REG_EDI,edi);active=True
   try:rig.call([0x446100,0x446da0,0x448130,0x448d20,0x444f60][mode],*([arg] if mode in (0,3) else [BASE+first*STRIDE,BASE+second*STRIDE] if mode==2 else []))
   finally:active=False
   result=rig.u.reg_read(UC_X86_REG_EAX) if mode==2 else 0
   expected.append(' '.join(map(str,[0,result,*[read(a) for a in CELLS]]))+' '+rig.u.mem_read(SQUADS,0x1a00).hex()+' '+' '.join(rig.u.mem_read(BASE+i*STRIDE,512).hex() for i in range(17))+' '+(''.join(trace) or '-')+' '+(''.join(access) or '-'));count[mode]+=1
 process=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,r)) for r in rows)+'\n',text=True,capture_output=True)
 if process.returncode:raise RuntimeError(process.stderr)
 actual=process.stdout.splitlines();assert len(actual)==len(expected),(len(actual),len(expected))
 for case,(got,want) in enumerate(zip(actual,expected)):
  if got!=want:
   args.report.parent.mkdir(parents=True,exist_ok=True);args.report.with_suffix('.failure.json').write_text(json.dumps({'case':case,'input':rows[case],'actual':got,'expected':want},indent=2))
   g,w=got.split(),want.split();field=next(i for i,(a,b) in enumerate(zip(g,w)) if a!=b);detail={'case':case,'mode':rows[case][0],'field':field,'actual':g[field][:160],'expected':w[field][:160]}
   if field in (len(g)-1,len(g)-2):
    a,b=g[field].split(';'),w[field].split(';');step=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)));detail.update({'step':step,'actual_step':a[step:step+4],'expected_step':b[step:step+4]})
   raise AssertionError(detail)
 report={'source_sha256':SOURCE_SHA256,'cases':len(rows),'mismatches':0,'cases_by_setter_collect_compare_log_clear':count,'original_bodies':['446100','446DA0','448130','448D20','444F60'],'comparison':'All64 squad images,17 client images,cells,complete ordered field reads/writes with widths,formatted-write arguments and file/time callback mutations','formatted_calls':print_counts,'inherited_edi_inputs':[0,1,2,63],'foreign_fixtures':['49A41C file open','49BAC0 formatted write','49A840 file close','GetLocalTime'],'audio_output':'none'}
 args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=300);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
