#!/usr/bin/env python3
"""Compare ambient callbacks and token storage with the supplied game code."""
import argparse,json,struct,subprocess,functools
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EAX,UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256

def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
 cases=[(m,s) for m in ['line','token','channel','extension','lazy'] for s in range(a.cases)]+[('lookup',s) for s in range(1535)]+[('parse',s) for s in range(4)]+[('resolve',s) for s in range(4)]
 proc=subprocess.run([str(a.probe.resolve()),str(a.game.resolve())],input=''.join(f'{m} {s}\n' for m,s in cases),text=True,capture_output=True,check=True)
 rows=[json.loads(v) for v in proc.stdout.splitlines()];assert len(rows)==len(cases)
 rig=Rig(a.game);rig.u.mem_map(0x21000000,0x10000);rig.u.mem_map(0x22000000,0x4000000);b=rig.u
 def put(at,n):b.mem_write(at,struct.pack('<I',n&0xffffffff))
 def get(at):return int.from_bytes(b.mem_read(at,4),'little')
 def text(at,s):b.mem_write(at,s.encode()+b'\0')
 def literal(at):
  out=b''
  while b.mem_read(at,1)!=b'\0':out+=b.mem_read(at,1);at+=1
  return out+b'\0'
 @functools.lru_cache(maxsize=None)
 def archive_file(name):
  for archive in ['Dfbase.pff','Df.pff']:
   raw=(a.game/archive).read_bytes();count,table=struct.unpack_from('<I',raw,8)[0],struct.unpack_from('<I',raw,16)[0]
   for i in range(count):
    flags,offset,size=struct.unpack_from('<III',raw,table+i*32);entry=raw[table+i*32+16:table+i*32+32].split(b'\0')[0].decode()
    if flags==0 and entry.upper()==name.upper():return raw[offset:offset+size]
  raise RuntimeError(name)
 loop=archive_file('LOOP.DEF');state={'heap':0x22000000,'position':0}
 def args(n):return struct.unpack('<'+'I'*n,b.mem_read(b.reg_read(UC_X86_REG_ESP)+4,n*4))
 def allocate():
  size=args(1)[0];out=state['heap']+16;state['heap']=(out+size+31)&~15;rig.finish(out)
 def load_all():
  name,archive=args(2);assert archive==0x21001000
  try:data=archive_file(literal(name)[:-1].decode())
  except RuntimeError:put(archive+0xac,1);rig.finish(0);return
  out=state['heap']+16;state['heap']=(out+len(data)+31)&~15;b.mem_write(out,data);put(archive+0xac,0);rig.finish(out)
 def diagnostic():
  name,label=args(2);assert label==0x4ce314;state['diagnostics']+=1
  if state['seed']%8==7:put(0x4f2ea8+3*40,0x8f0050)
  rig.finish(0)
 def opened():state['position']=0;rig.finish(0)
 def file_read():
  destination,size=args(2);position=state['position'];b.mem_write(destination,loop[position:position+size]);state['position']+=size;rig.finish(size)
 rig.stubs.update({0x499040:lambda:rig.finish(1),0x499120:opened,0x499320:lambda:rig.finish(len(loop)),0x499020:lambda:rig.finish(0),0x498050:allocate,0x497f60:allocate,0x499280:file_read,0x4980d0:lambda:rig.finish(0),0x49e750:load_all,0x499750:diagnostic})
 initial_table=bytes(b.mem_read(0x4f2ea4,512*40))
 errors=[];lookup_ready=False
 for (mode,seed),got in zip(cases,rows):
  if mode!='lookup':lookup_ready=False
  record=0x21000000;start=0x21000100;row=0x8f0000+(seed%8)*40
  b.mem_write(0x8f0000,bytes((seed*13+i*7)&255 for i in range(512)))
  put(0x6e06c4,0x8f0000);put(0x6e06c8,seed%8);put(0x6e06cc,0xaabbccdd)
  if mode=='line':
   words=[f'ambient{seed%10}','WIND',str(seed*3),str(seed*5),str(seed*7),'-1','257','unused','unused']
   words+=['unused']*(30-len(words))
   if seed%7==0:words[0]='range'
   if seed%7==1:words[0]='version'
   for i,s in enumerate(words):put(record+4+i*4,start+i*32);text(start+i*32,s)
   if seed%5==1:b.mem_write(start,literal(0x4cc0a0))
   if seed%3==0:b.mem_write(start+7*32,literal(0x4cc0a8))
   if seed%3==1:b.mem_write(start+8*32,literal(0x4cc0b0))
   put(record,seed%13)
   rig.call(0x43ec40 if seed%2 else 0x43ebf0,record if seed%17 else 0);result=b.reg_read(UC_X86_REG_EAX)
   want=dict(result=result,count=get(0x6e06c8),range=get(0x6e06cc),row=bytes(b.mem_read(row,40)).hex())
  elif mode=='token':
   lines=['a b c','a, b,','"a b",c\t"d" ','','a"b"c',' \t, ','//x','x\t"a,b"\t']
   for i in range(30):put(0x8e4430+i*4,0x12300000+seed*30+i)
   b.mem_write(0x8e4040,bytes((seed+i)&255 for i in range(1004)))
   line='x'*1100 if seed%11==0 else lines[seed%8];text(start,line)
   b.reg_write(UC_X86_REG_ESI,record);rig.call(0x4565f0,start)
   want=dict(record=bytes(b.mem_read(record,124)).hex(),buffer=bytes(b.mem_read(0x8e4040,1004)).hex())
  elif mode=='lazy':
   first=0x8f0000;second=first+40;third=second+40;id=3;state['diagnostics']=0;state['seed']=seed;b.mem_write(first,bytes(120))
   text(first,'first');text(second,'second');text(third,'third');text(first+12,'NO_WAVE' if seed%4==3 else 'MISSILE');text(second+12,'MISSILE' if seed%4==1 else 'OTHER');text(third+12,'OTHER');put(second+0x18,0x12340000);b.mem_write(first+0x20,bytes([seed&255]));b.mem_write(third+0x20,bytes([seed&255]));put(0x6e06c4,first);put(0x6e06c8,2);put(0x4f2ea8+id*40,first)
   for address,value in [(0xd096e4,0),(0xd096d8,0x21001000),(0xd096dc,0),(0xd0973c,0)]:put(address,value)
   rig.call(0x43ee80,id);result=b.reg_read(UC_X86_REG_EAX);hash=14695981039346656037
   if result and result!=0x12340000:
    size=get(result+4)+16
    for value in b.mem_read(result,size):hash=((hash^value)*1099511628211)&((1<<64)-1)
   want=dict(result=2 if result==0x12340000 else 1 if result else 0,hash=hash,flags=b.mem_read(first+0x20,1)[0],target=get(0x4f2ea8+id*40),other_flags=b.mem_read(third+0x20,1)[0],diagnostics=state['diagnostics'])
  elif mode=='channel':
   enabled=seed%2;slot=(seed//2)%16;value=seed*17;put(0xd08940,enabled);b.mem_write(0x4a1703+slot*0x34,b'\x5d');rig.call(0x4950a0,slot,value);want=dict(result=b.reg_read(UC_X86_REG_EAX),value=b.mem_read(0x4a1703+slot*0x34,1)[0]);put(0xd08940,1)
  elif mode=='extension':
   names=['a.b.c','abc','','.cfg','many..dots','file.old'];name=names[seed%6];text(start,name);text(start+64,'XYZ');suffix=start+64
   if seed%3==0 and '.' in name:suffix=start+name.index('.')+1
   rig.call(0x456560,start,suffix);want=dict(bytes=bytes(b.mem_read(start,128)).hex())
  elif mode=='lookup':
   if not lookup_ready:b.mem_write(0x4f2ea4,initial_table);text(start,'LOOP.DEF');put(0x503228,0x04960552);rig.call(0x43e340,start);lookup_ready=True
   name=literal(0x4f2eda+(seed%511)*40)[:-1].decode()
   if (seed//511)%3==0:name=name.lower()
   if (seed//511)%3==1:name=name.upper()
   if seed==1533:name=''
   if seed==1534:name+='_MISSING'
   text(start,name);rig.call(0x43f490,start);want=dict(id=b.reg_read(UC_X86_REG_EAX))
  elif mode=='parse':
   text(start,'LOOP.DEF');put(0x6e06cc,seed);put(0x503228,0x04960552);rig.call(0x43edb0,start);count=get(0x6e06c8);want=dict(count=count,range=get(0x6e06cc),rows=bytes(b.mem_read(get(0x6e06c4),count*40)).hex())
  else:
   for target in [0x499120,0x499280,0x499020]:rig.stubs.pop(target,None)
   def open_archive():
    name,archive=args(2);assert archive==0x21001000
    try:state['file']=archive_file(literal(name)[:-1].decode());put(archive+0xac,0);state['position']=0;rig.finish(0)
    except RuntimeError:put(archive+0xac,1);rig.finish(0xffffffff)
   def read_archive():
    destination,size,archive=args(3);assert archive==0x21001000;position=state['position'];data=state['file'][position:position+size];b.mem_write(destination,data);state['position']+=len(data);rig.finish(len(data))
   def seek_archive():
    offset,origin,archive=args(3);assert archive==0x21001000;offset=offset if offset<0x80000000 else offset-0x100000000;state['position']=(0 if origin==0 else state['position'] if origin==1 else len(state['file']))+offset;rig.finish(state['position'])
   def exists():
    name=args(1)[0]
    try:archive_file(literal(name)[:-1].decode());rig.finish(1)
    except RuntimeError:rig.finish(0)
   rig.stubs.update({0x49e630:open_archive,0x49e6b0:read_archive,0x49e6e0:seek_archive,0x499040:exists})
   b.mem_write(0x4f2ea4,initial_table);text(start,'LOOP.DEF');put(0x503228,0x04960552);put(0xd08940,1);put(0xd096d8,0x21001000);put(0xd096e4,0);put(0xd096dc,0);put(0xd0973c,0);text(0x8cf96c,'NO_SCRIPT.MIS');text(start+64,'SNIPER.PWF');text(start+128,'SNIPER.PWF' if seed%2 else 'NO_ALT.PWF')
   rig.call(0x43e340,start);rig.call(0x43e4a0,start+64,start+128);image=bytearray(b.mem_read(0x4f2ea4,512*40))
   for i in range(512):
    pointer=struct.unpack_from('<I',image,i*40+4)[0];normal=pointer
    if pointer:
     for j,context in enumerate([0x6e0518,0x6e05e0]):
      base,count=get(context+0x38),get(context+0x28)
      if pointer>=base and pointer-base<count*80 and (pointer-base)%80==0:normal=1+j*10000+(pointer-base)//80
     base,count=get(0x6e06c4),get(0x6e06c8)
     if pointer>=base and pointer-base<count*40 and (pointer-base)%40==0:normal=20001+(pointer-base)//40
    struct.pack_into('<I',image,i*40+4,normal)
   want=dict(table=image.hex(),primary=get(0x6e06bc),alternate=get(0x6e06c0),ambient=get(0x6e06c8))
  if want!=got:errors.append(dict(mode=mode,seed=seed,want=want,got=got))
 report=dict(source_sha256=SOURCE_SHA256,cases=len(cases),bodies=['43EBF0','43EC40','4565F0','456720','43EDB0','456EB0','4950A0','4B4CBA','456560','43EE80','494C30','499410','494C50','43E4A0','43E340','49CE00','49D8C0','49D0F0','43F490'],mismatches=len(errors),details=errors[:4],fixtures=['Complete caller token records and reused tokenizer bytes','Real supplied PFF member bytes; allocator/read/seek/load-all/size/exists backends','Required499750 diagnostic with current definition mutation','Actual startup503228 key04960552','Original CRT text/numeric and script decode instructions execute'])
 a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='details'}));return bool(errors)
if __name__=='__main__':raise SystemExit(main())
