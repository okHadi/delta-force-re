#!/usr/bin/env python3
"""Check456F30 reads and complete47B590 outputs with original475280 decoding."""
import argparse,hashlib,json,random,struct,subprocess,tempfile
from pathlib import Path
import pefile,unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
SHA='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
MASK=0xffffffff;STACK=0x2008000;RETURN=0x4000000
class Original:
 images={}
 def __init__(self,game):
  if str(game) not in self.images:
   raw=(game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA;self.images[str(game)]=pefile.PE(data=raw,fast_load=True).get_memory_mapped_image()
  self.u=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32);self.u.mem_map(0x400000,0xa00000);self.u.mem_write(0x400000,self.images[str(game)]);self.u.mem_map(0x1100000,0x10000);self.u.mem_map(0x2000000,0x10000);self.u.mem_map(0x10000000,0x4000000);self.next=0x10000000;self.failure=None;self.calls=[]
 def get(self,a,n=4):return int.from_bytes(self.u.mem_read(a,n),'little')
 def put(self,a,v,n=4):self.u.mem_write(a,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
 def text(self,a):
  out=bytearray()
  while True:
   c=self.get(a,1);a+=1
   if not c:return out.decode('ascii')
   out.append(c)
 def alloc(self,n):
  a=self.next;self.next=(a+max(n,1)+31)&~31
  if self.next>0x14000000:raise RuntimeError('Original fixture allocation extent')
  return a
 def finish(self,result=0):
  sp=self.u.reg_read(UC_X86_REG_ESP);ra=self.get(sp);self.u.reg_write(UC_X86_REG_ESP,sp+4);self.u.reg_write(UC_X86_REG_EAX,result&MASK);self.u.reg_write(UC_X86_REG_EIP,ra)
 def service(self,site,argc,fn):
  def hook(u,pc,size,_):
   sp=u.reg_read(UC_X86_REG_ESP);args=[self.get(sp+4+i*4)for i in range(argc)];self.calls.append([site,args])
   try:self.finish(fn(args))
   except Exception as e:self.failure=str(e);u.emu_stop()
  self.u.hook_add(unicorn.UC_HOOK_CODE,hook,begin=site,end=site)
 def call(self,site,*args):
  sp=STACK-4*(len(args)+1);self.put(sp,RETURN)
  for i,v in enumerate(args):self.put(sp+4+i*4,v)
  self.u.reg_write(UC_X86_REG_ESP,sp);self.u.emu_start(site,RETURN,count=300000000)
  if not self.failure and self.u.reg_read(UC_X86_REG_EIP)!=RETURN:raise RuntimeError('Original instruction budget reached')
  return self.u.reg_read(UC_X86_REG_EAX)

def crc_cases(game,probe):
 rng=random.Random(45630);cases=[]
 for i in range(220):cases.append([rng.getrandbits(32),rng.choice([1,2,3,7,8,31,32,127,256,513]),rng.choice([0x1100200,0xfffffffe,0]),rng.randrange(4),0])
 for size in [1,2,8,32]:
  for fail in range(1,2*size+1):cases.append([77,size,0x1100200,3,fail])
 for fail in [1,2,3,4,8]:cases.append([9,0,0xfffffffe,3,fail])
 for ptr in [0x4f8720,0x4f8721,0x4f8b18]:
  for mode in [0,1]:cases.append([33,8,ptr,mode,0])
 expected=[]
 for seed,count,pointer,mode,fault in cases:
  r=Original(game);u=r.u;u.mem_map(0,0x1000);u.mem_map(0xfffff000,0x1000);nr=0;trace=1469598103934665603;failed=False
  def byte(a):return ((a*13+seed*17)^(a>>5))&255
  for i in range(min(count if count else 8,1024)):
   at=(pointer+i)&MASK
   if not 0x4f8720<=at<0x4f8b20:r.put(at,byte(at),1)
  def event(v):
   nonlocal trace
   for i in range(4):trace=((trace^((v>>(8*i))&255))*1099511628211)&0xffffffffffffffff
  def read(uc,kind,at,width,value,_):
   nonlocal nr,failed
   pc=u.reg_read(UC_X86_REG_EIP)
   if not 0x456f3f<=pc<=0x456f5f:return
   event(at);event(width);nr+=1
   if nr==fault:failed=True;u.emu_stop();return
   if width==1 and mode&1:
    for i in range(256):r.put(0x4f8720+4*i,r.get(0x4f8720+4*i)^((seed+nr)&MASK))
   if width==4 and mode&2:
    nxt=(pointer+nr//2)&MASK;r.put(nxt,r.get(nxt,1)^0xa5,1)
  u.hook_add(unicorn.UC_HOOK_MEM_READ,read)
  try:value=r.call(0x456f30,pointer,count)
  except Exception:
   if not failed:raise
  expected.append(f'{int(failed)} {nr} {trace} {0 if failed else value}')
 out=subprocess.run([str(probe)],input='\n'.join(' '.join(map(str,x))for x in cases)+'\n',text=True,capture_output=True,check=True).stdout.splitlines()
 for i,(a,b)in enumerate(zip(expected,out)):
  if a!=b:raise AssertionError(f'CRC case{i} {cases[i]} expected {a} got {b}')
 assert len(out)==len(expected)
 return len(cases)

def terrain_case(game,probe,mission,directory,hidden=None):
 directory.mkdir(parents=True,exist_ok=True)
 meta=json.loads(subprocess.run([str(probe),'assets',str(game),mission,str(directory)],text=True,capture_output=True,check=True).stdout)
 r=Original(game);u=r.u;byname={x['name'].lower():x for x in meta['images']};blob=b'';cursor=0;decodes=[];stores=[];fault=False
 cells={'color_map':0xbd519c,'elev_map':0xbd51ac,'detail_map':0xbd51bc,'detail_color':0xbd51cc,'detail_elev':0xbd51dc,'detail_shade':0xbd51ec,'sky_map':0xbd51fc,'sky_palette':0xbd520c,'water_map':0xbd521c,'water_palette':0xbd522c}
 for x in meta['images']:u.mem_write(cells[x['key']],x['name'].encode()+b'\0')
 for a,n in [(0xbd5284,1048576),(0xbd52ac,1048576),(0xbd5330,1048576),(0xbd531c,262144),(0xbd5438,1048576)]:r.put(a,r.alloc(n))
 r.put(0xbd5248,meta['configured_water']);r.put(0xbd4f94,8)
 def exists(a):
  name=r.text(a[0]).lower();return int(name in byname and byname[name]['present'] and byname[name]['key']!=hidden)
 def open_file(a):
  nonlocal blob,cursor
  x=byname[r.text(a[0]).lower()]
  if not x['present'] or x['key']==hidden:raise RuntimeError('Required file open failed')
  blob=(directory/(x['key']+'.pcx')).read_bytes();cursor=0;return 1
 def read_file(a):
  nonlocal cursor
  data=blob[cursor:cursor+a[1]]
  if len(data)!=a[1]:raise RuntimeError('Required file read failed')
  u.mem_write(a[0],data);cursor+=len(data);return len(data)
 def seek(a):
  nonlocal cursor
  signed=a[0]if a[0]<0x80000000 else a[0]-0x100000000;cursor=(0 if a[1]==0 else cursor if a[1]==1 else len(blob))+signed;return cursor
 def length(a):return len((directory/(byname[r.text(a[0]).lower()]['key']+'.pcx')).read_bytes())
 def error(a):raise RuntimeError('Source401840: '+r.text(a[0]))
 def flip(a):
  # Explicit row-flip service fixture; output CRCs precede this callback.
  ptr,width,size=a;data=bytes(u.mem_read(ptr,size));rows=size//width;u.mem_write(ptr,b''.join(data[i*width:(i+1)*width]for i in range(rows-1,-1,-1)));return 0
 for site,argc,fn in [(0x47b400,2,lambda a:1),(0x497f60,4,lambda a:r.alloc(a[0])),(0x480bb0,5,lambda a:0),(0x475a10,1,lambda a:(u.mem_write(a[0],bytes(0x34))or 0)),(0x48fd00,1,lambda a:0),(0x48fd70,0,lambda a:0),(0x4747f0,1,lambda a:0),(0x47a9f0,3,flip),(0x47ebc0,3,lambda a:0),(0x47eb70,3,lambda a:0),(0x401840,1,error),(0x499040,1,exists),(0x499120,1,open_file),(0x499280,2,read_file),(0x4992d0,2,seek),(0x499020,0,lambda a:0),(0x499320,1,length),(0x498050,3,lambda a:r.alloc(a[0])),(0x498120,1,lambda a:r.alloc(a[0])),(0x4980d0,1,lambda a:0),(0x498160,1,lambda a:0)]:r.service(site,argc,fn)
 # Observe each real decoder return and verify every decoded byte/palette.
 def decoder_call(uc,pc,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);desc=r.get(sp+4);name=r.text(r.get(sp+8)).lower();ret=r.get(sp)
  def done(uc,pc,size,_):
   if pc!=ret:return
   u.hook_del(handle)
   if u.reg_read(UC_X86_REG_EAX)==0:
    x=byname[name];pixels=bytes(u.mem_read(r.get(desc+0x20),r.get(desc+0xc)));palette=bytes(u.mem_read(r.get(desc+0x30),768));assert pixels==(directory/(x['key']+'.pixels')).read_bytes(),x['key'];assert palette==(directory/(x['key']+'.palette')).read_bytes(),x['key'];decodes.append(x['key'])
  handle=u.hook_add(unicorn.UC_HOOK_CODE,done,begin=ret,end=ret)
 u.hook_add(unicorn.UC_HOOK_CODE,decoder_call,begin=0x475280,end=0x475280)
 def store(uc,kind,at,width,value,_):
  if 0xbd67d4<=at<=0xbd67f4:stores.append([at,width,value&MASK])
 u.hook_add(unicorn.UC_HOOK_MEM_WRITE,store,begin=0xbd67d4,end=0xbd67f7)
 result=r.call(0x47b590,0x1100000);values=[r.get(0xbd67d4+4*i)for i in range(9)]
 if hidden in ['elev_map','color_map','detail_map','detail_color','detail_shade','detail_elev','sky_palette']:
  assert r.failure and '401840' in r.failure;fault=True
  order=['elev_map','color_map','detail_map','detail_color','detail_shade','detail_elev','sky_palette'];addresses=[0xbd67d8,0xbd67d4,0xbd67dc,0xbd67e0,0xbd67e8,0xbd67e4,0xbd67f4];prefix=order.index(hidden);assert stores==[[address,4,meta['values'][(address-0xbd67d4)//4]]for address in addresses[:prefix]],(hidden,stores)
 else:
  assert not r.failure and result==1,(r.failure,result)
  expected=meta['values'][:]
  if hidden=='sky_map':expected[7]=0
  if hidden in ['water_map','water_palette']:expected[6]=0
  assert values==expected,(mission,hidden,values,expected)
 return {'mission':mission,'hidden':hidden,'failure':fault,'values':values,'stores':stores,'decoded':decodes,'calls':len(r.calls)}

def run(a):
 n=crc_cases(a.game,a.probe);root=a.report.parent/'terrain-content-original-assets';results=[]
 for mission in ['C05M07.BMS','C06M01.BMS','C02M03.BMS']:
  results.append(terrain_case(a.game,a.probe,mission,root/mission))
 for hidden in ['elev_map','color_map','detail_map','detail_color','detail_shade','detail_elev','sky_palette','sky_map','water_palette','water_map']:
  results.append(terrain_case(a.game,a.probe,'C02M03.BMS',root/('edge-'+hidden),hidden))
 report={'source_sha256':SHA,'crc_cases':n,'terrain_cases':len(results),'differences':0,'results':results,'boundaries':'Original47B590+475280+456F30 instructions; explicit file,allocation,config,row-flip,palette,progress,cleanup service fixtures. Typed helper projects successful startup content only.'}
 a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k]for k in ['crc_cases','terrain_cases','differences']}))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
