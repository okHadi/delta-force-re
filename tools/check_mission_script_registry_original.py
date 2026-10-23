#!/usr/bin/env python3
"""Compare the supplied game's offline registry instructions and explicit foreign effects."""
import argparse, hashlib, json, random, struct, subprocess
from pathlib import Path
import pefile
import unicorn as uc
from check_projectile_hits_original import Replay, HASH, STOP, STACK, pack
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EFLAGS
N,O,W,T,R,B,S=0x2201000,0x2202000,0x2204000,0x2206000,0x2207000,0x2208000,0x2209000
G=0x8e3d50
FUNCTIONS=[0x455c90,0x455c30,0x455ea0,0x49ba00,0x49ad30,0x49dff0,0x43e430,0x43e470,0x49d400,0x49cf70]
CALLS={0x49a620:3,0x497f60:4,0x499120:1,0x499280:2,0x4992d0:2,0x499020:0,0x49ba00:2,0x493a80:3,0x49cf50:2,0x499f1f:1,0x4980d0:1,0x49db20:1,0x495160:1,0x479f00:2,0x501d60:0,0x49cfd0:2,0x49d0e0:1,0x498230:2}
def put(b,o,v):b[o:o+4]=pack(v)
def text(s,n=512):return bytearray(s+bytes(n-len(s)))
def fnv(blocks):
 h=1469598103934665603
 for b in blocks:
  for c in b:h=((h^c)*1099511628211)&0xffffffffffffffff
 return h
class Original:
 def __init__(self,source):
  self.r=Replay(source);self.r.u.mem_map(0x2300000,0x200000);self.case_hooks=[];self.mode=0
  for site in CALLS:
   if site not in (0x501d60,0x49ba00):self.r.hook(site,lambda site=site:self.effect(site))
  self.r.hook(STOP+0x100,lambda:self.effect(0x501d60))
  def token(u,at,size,_):
   if self.mode==3:return
   self.effect(0x49ba00);esp=u.reg_read(UC_X86_REG_ESP);ret=self.r.read(esp)[0];u.reg_write(UC_X86_REG_ESP,esp+4);u.reg_write(UC_X86_REG_EIP,ret)
  self.r.u.hook_add(uc.UC_HOOK_CODE,token,begin=0x49ba00,end=0x49ba00)
 def case(self,case):
  mode,a,b,mutation,fail,segments,file=case;r=self.r;self.mode=mode
  for hook in self.case_hooks:r.u.hook_del(hook)
  self.case_hooks=[];self.calls=[];self.access=[];self.offset=0;self.next=0;self.reads=0;self.thread_calls=0;self.mutated=False;self.pending=False;self.allocations=[]
  for at,data in segments:r.u.mem_write(at,bytes(data))
  def string(at):
   out=bytearray()
   while (v:=r.u.mem_read(at,1)[0]):out.append(v);at+=1
   return bytes(out)
  def effect(site):
   esp=r.u.reg_read(UC_X86_REG_ESP);args=r.read(esp+4,CALLS[site]) if CALLS[site] else ();self.calls.append(str(site)+''.join(','+str(v) for v in args)+';');v=0
   if site==0x49a620:r.u.mem_write(args[0],bytes([args[1]&255])*args[2]);v=args[0]
   elif site==0x497f60:
    self.next+=1
    if self.next!=fail:
     v=0x2300000+self.next*0x1000;size=max(args[0],1);r.u.mem_write(v,bytes(size));self.allocations.append((v,size))
     if mutation&1 and self.next==1:r.write(0x8e3d58,B)
   elif site==0x499120:self.offset=0;v=0xffffffff if mutation&2 else 0
   elif site==0x499280:
    at,size=args
    if self.offset+size>len(file):raise RuntimeError('short registry file')
    if size:r.u.mem_write(at,file[self.offset:self.offset+size])
    self.offset+=size;self.reads+=1;v=size
    if mutation&4 and self.reads==3:r.write(0x8e3d54,1)
   elif site==0x4992d0:self.offset=((self.offset+args[0]) if args[1] else args[0])&0xffffffff;v=self.offset
   elif site==0x49ba00:
    s=string(args[0]);dot=s.find(b'.')
    if dot>=0:r.u.mem_write(args[0]+dot,b'\0')
    v=args[0] if s else 0
   elif site==0x493a80:
    s=string(args[2])+(b'.pwf' if args[1]==0x4cca6c else b'.bin');r.u.mem_write(args[0],s+b'\0');v=len(s)
   elif site==0x49cf50:
    if mutation&8:r.u.mem_write(N,b'changed\0')
   elif site==0x499f1f:v=R
   elif site==0x4980d0:
    if mutation&16 and args[0]==S:r.write(B+4,0)
   elif site==0x501d60:
    v=T+(32 if mutation&512 and self.thread_calls else 0);self.thread_calls+=1
   elif site==0x49cfd0:v=0 if mutation&32 else S
   elif site==0x49d0e0:r.u.mem_write(args[0]+0x23,b'\1');r.write(args[0]+0x24,0x220a000)
   elif site==0x498230:
    v=(args[0]-16)&0xffffffff
    if mutation&64:r.write(S+0x24,0x220b000)
   r.u.reg_write(UC_X86_REG_EAX,v)
  self.effect=effect
  r.write(0x501d60,STOP+0x100)
  def access(u,kind,at,size,value,_):
   if not any(at>=base and at+size<=base+len(data) for base,data in segments) and not any(at>=base and at+size<=base+n for base,n in self.allocations) and not (0x5013f4<=at<0x5014f4 or 0x4ceb80<=at<0x4ceb88):return
   if kind==uc.UC_MEM_READ:value=int.from_bytes(u.mem_read(at,size),'little')
   self.access.append(('R' if kind==uc.UC_MEM_READ else 'W')+f',{at},{size},{value};')
   if kind==uc.UC_MEM_READ and not self.mutated and ((mutation&128 and at==0x67f51c) or (mutation&256 and at==a)):self.pending=True;self.mutated=True
  self.case_hooks.append(r.u.hook_add(uc.UC_HOOK_MEM_READ|uc.UC_HOOK_MEM_WRITE,access))
  if mutation&384:
   def advance(u,at,size,_):
    if self.pending:
     self.pending=False
     if mutation&128:r.write(0x67f51c,0xffffff80)
     else:r.u.mem_write(b,bytes([r.u.mem_read(b,1)[0]^1]))
   self.case_hooks.append(r.u.hook_add(uc.UC_HOOK_CODE,advance))
  result=0;error=0
  try:
   args={0:(a,),1:(),2:(),3:(a,b),4:(a,b),5:(a,),6:(a,),7:(a,),8:(a,b),9:(a,)}[mode]
   stack=b+0x168 if mode==0 else W+0x28 if mode==3 else STACK
   r.write(stack,STOP,*args);r.u.reg_write(UC_X86_REG_ESP,stack);r.u.reg_write(UC_X86_REG_EFLAGS,2)
   r.u.emu_start(FUNCTIONS[mode],STOP,timeout=10000000,count=2000000)
   assert r.u.reg_read(UC_X86_REG_EIP)==STOP
   result=r.u.reg_read(UC_X86_REG_EAX)
   if mode in (2,7,9):result=0
   if mode==6:result=int(bool(result&255))
  except Exception:error=1;result=0
  blocks=[bytes(r.u.mem_read(at,len(data))) for at,data in segments]+[bytes(r.u.mem_read(at,size)) for at,size in self.allocations]
  return f'{result} {error} {fnv(blocks)} {fnv(["".join(self.access).encode()])} '+(''.join(self.calls) or '-')
def baseline():
 g=bytearray(0x1f0);return [(G,g),(N,text(b'dfdlg05.dbf')),(O,text(b'.')),(W,bytearray([0xa5])*0x158),(T,bytearray(64)),(R,bytearray(256)),(B,bytearray(16)),(S,bytearray(256))]
def case(mode,a=N,b=O,mutation=0,fail=0,file=b''):
 return [mode,a,b,mutation,fail,baseline(),file]
def run(args):
 source=(args.game/'Df.exe').read_bytes();assert hashlib.sha256(source).hexdigest()==HASH
 original=Original(source);rng=random.Random(0x455c90);cases=[];counts={}
 def add(c):cases.append(c);counts[hex(FUNCTIONS[c[0]])]=counts.get(hex(FUNCTIONS[c[0]]),0)+1
 for i in range(args.cases):
  mode=i%10;j=i//10;c=case(mode)
  d=dict(c[5])
  if mode==0:
   count=j%4;ids=j%3;data=bytearray(pack(0x30474c44,256,28,ids,28,count,28+ids*32)+bytes(ids*32))
   for row_index in range(count):
    row=bytearray(52+(row_index%3)*68);put(row,0x1c,row_index%3);row[4:9]=b'dlg01';data+=row
   if j%11==1:put(data,0,0)
   c[2]=W;c[6]=bytes(data);c[3]=rng.choice([0,2,4,8]);c[4]=rng.choice([0,0,0,1,2,3])
   if j%7==0:c[1]=0
  elif mode==1:
   if i%3:put(d[G],8,B);put(d[B],4,B+8 if i%2 else 0)
   c[3]=j%2;c[4]=1 if j%17==0 else 0
  elif mode==2:
   put(d[G],8,B);put(d[G],16,0x220a000);put(d[G],0x1ec,R);put(d[B],0,S);put(d[B],4,B+8);put(d[B],8,0x220b000);c[3]=16
  elif mode==3:
   d[W][:]=bytes([0xa5])*32
   value=rng.choice([b'..alpha.beta',b'',b'....',b'abc',b'a..b',bytes([128,46,129])]);d[N][:]=text(value);c[1]=0 if i%3==0 else N;put(d[T],0x10,N if i%2 else 0);c[3]=512 if j%2 else 0
  elif mode==4:
   value=bytes(rng.randrange(1,256) for _ in range(i%100));d[N][:]=text(value);d[O][:]=text(value if i%3 else value+b'x');c[1]+=i%4;c[2]+=i%4
   if i%11==0:c[2]=c[1]
   else:c[3]=256 if j%2 else 0
  elif mode==5:d[N][:]=text(rng.choice([b' -2147483649',b'+4294967295',b'\t00123x',b'-0',b'999999999999999999999999',b'bad']))
  elif mode==6:c[3]=128 if i%3 else 0;c[1]=R;c[5].append((0x67f51c,bytearray(pack(rng.choice([0,1,200,256,0x80000000,0xffffffff])))))
  elif mode==7:c[1]=0 if i%3==0 else N;d[N][:]=text(b'' if j%2 else b'text')
  elif mode==8:c[2]=0 if j%5==0 else R;d[S][0x23]=j%2;put(d[S],0x24,0x220a000);put(d[R],0xc4,0x12345678);c[3]=rng.choice([0,32,64])
  else:c[1]=R;put(d[R],0x1c,S);put(d[R],0x20,B);put(d[R],0x38,j%2)
  add(c)
 for archive in sorted(args.game.glob('*.pff'))+sorted(args.game.glob('*.PFF')):
  with archive.open('rb') as f:
   header=f.read(20)
   if len(header)<20 or header[4:8]!=b'PFF3':continue
   count,_,table=struct.unpack_from('<III',header,8);f.seek(table);directory=f.read(count*32)
   for index in range(count):
    record=directory[index*32:index*32+32]
    if record[16:32].split(b'\0')[0].lower()==b'dfdlg05.dbf' and struct.unpack_from('<I',record)[0]==0:
     offset,size=struct.unpack_from('<II',record,4);f.seek(offset);data=f.read(size)
     c=case(0,N,W,file=data);add(c)
 expected=[original.case(c) for c in cases]
 rows=[' '.join(map(str,c[:5]))+' '+str(len(c[5]))+' '+ ' '.join(str(at)+' '+(bytes(data).hex() or '-') for at,data in c[5])+' '+(c[6].hex() or '-') for c in cases]
 run=subprocess.run([str(args.probe),str(args.game/'Df.exe')],input='\n'.join(rows)+'\n',text=True,capture_output=True);assert run.returncode==0,run.stderr
 actual=run.stdout.splitlines();assert len(actual)==len(expected),(len(actual),len(expected))
 for i,(a,b) in enumerate(zip(actual,expected)):
  if a!=b:raise AssertionError((i,cases[i][:5],a,b))
 report={'source_sha256':HASH,'cases':len(cases),'differences':0,'functions':counts,'scope':'Actual bodies with explicit foreign service fixtures; complete watched bytes, ordered reads/writes, and ordered calls.'};print(json.dumps(report,indent=2))
 if args.report:args.report.write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--game',type=Path,required=True);p.add_argument('--probe',type=Path,required=True);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path);run(p.parse_args())
