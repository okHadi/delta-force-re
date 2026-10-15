"""Check initial player poses against the original DF1 instructions."""
import argparse,hashlib,io,json,struct,subprocess,sys
from pathlib import Path
import pefile,unicorn
from unicorn.x86_const import *
from PIL import Image
from mission import Resolver,terrain_name
p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path);args=p.parse_args()
raw=(args.game/'Df.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
if sha!='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff':raise SystemExit('Unsupported original executable hash')
pe=pefile.PE(data=raw);u=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
for at,size in [(0x400000,0xa00000),(0x1000000,0x200000),(0x2000000,0x10000),(0x3000000,0x100000),(0x4000000,0x1000)]:u.mem_map(at,size)
for s in pe.sections:
 if s.PointerToRawData and not s.Characteristics&0x80:u.mem_write(0x400000+s.VirtualAddress,raw[s.PointerToRawData:s.PointerToRawData+s.SizeOfRawData])
def put(at,v):u.mem_write(at,struct.pack('<I',v&0xffffffff))
def word(at):return struct.unpack('<I',u.mem_read(at,4))[0]
def signed(v):return v if v<0x80000000 else v-0x100000000
def run(fn,*values,end=0x4000000):
 sp=0x2008000;u.mem_write(sp,struct.pack('<'+'I'*(len(values)+1),0x4000000,*[v&0xffffffff for v in values]));u.reg_write(UC_X86_REG_ESP,sp);u.emu_start(fn,end,count=10000)
 return u.reg_read(UC_X86_REG_EAX)
def hook(machine,at,size,user):
 if at==0x4000000:u.emu_stop();return
 if at not in (0x4828c0,0x49a620):return
 sp=u.reg_read(UC_X86_REG_ESP)
 if at==0x49a620:u.mem_write(word(sp+4),bytes([word(sp+8)&255])*word(sp+12))
 u.reg_write(UC_X86_REG_EAX,word(sp+4));u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp))
u.hook_add(unicorn.UC_HOOK_CODE,hook)
probe=str(args.probe.resolve());cases=[(v,-v if v!=-32768 else 32767)for v in range(-32768,32768)]+[(0,-32768)]
native=subprocess.check_output([probe,'angles'],input=''.join(f'{a} {b}\n'for a,b in cases).encode()).splitlines()
assert len(native)==len(cases),(len(native),len(cases))
for (yaw,pitch),line in zip(cases,native):
 u.mem_write(0x1100000+52,struct.pack('<2h',yaw,pitch));u.reg_write(UC_X86_REG_EBP,0x1100000);u.reg_write(UC_X86_REG_EBX,0x1000000);u.reg_write(UC_X86_REG_ESP,0x2008000);u.emu_start(0x4497b5,0x4497ed,count=100)
 assert tuple(map(int,line.split()))==struct.unpack('<2I',u.mem_read(0x1000030,8)),(yaw,pitch,line)
put(0x67f520,0)
for flags in range(256):
 u.mem_write(0x1100000,bytes(96));put(0x110000c,flags);run(0x449710,0x1000000,0x1100000,0,end=0x4497a3);assert (u.reg_read(UC_X86_REG_EIP)==0x4497a3)==(not flags&0x90),flags
rows=json.loads(subprocess.check_output([probe,str(args.game),'--all']));resolver=Resolver(args.game);missing=[];starts=[];image_cache={}
def read(name):return resolver.read(*resolver.find(name))[0]
def height(mission):
 fields={}
 for line in read(terrain_name(mission)).decode('ascii').splitlines():
  words=line.split()
  if words and words[0]=='elev_map':fields['height']=words[1]+'.PCX'
 name=fields['height']
 if name not in image_cache:
  im=Image.open(io.BytesIO(read(name)));im.load();image_cache[name]=im.transpose(Image.Transpose.FLIP_TOP_BOTTOM).tobytes()
 return image_cache[name]
for row in rows:
 b=read(row['mission']);counts=struct.unpack_from('<4I',b,152);offset=268+sum(counts[:2])*96;markers=[b[offset+i*96:offset+(i+1)*96]for i in range(counts[2])];valid=[(i,r)for i,r in enumerate(markers)if struct.unpack_from('<I',r)[0]==6001 and not struct.unpack_from('<I',r,12)[0]&0x90]
 if not valid:assert row['start']is None;missing.append(row['mission']);continue
 index,marker=valid[0];u.mem_write(0x3000000,height(b));put(0xbd5284,0x3000000);u.mem_write(0x8ace80,bytes(256*0x114))
 for i,r in enumerate(markers):
  if struct.unpack_from('<I',r,12)[0]&0x90:continue
  at=0x8ace80+i*0x114;put(at,struct.unpack_from('<I',r)[0]);x,y,z=struct.unpack_from('<3i',r,16);ground=signed(run(0x47df40,x,y));u.mem_write(at+0x24,struct.pack('<3I',x&0xffffffff,y&0xffffffff,(z+ground)&0xffffffff));u.mem_write(0x1100000,r);u.reg_write(UC_X86_REG_EBP,0x1100000);u.reg_write(UC_X86_REG_EBX,at);u.emu_start(0x4497b5,0x4497ed,count=100)
 put(0x67f53c,0);put(0x8cf9f4,struct.unpack_from('<I',b,236)[0]);put(0x8cf900,counts[2]);put(0x6e9a98,0x1000000);run(0x43f610,0x6e9a90);put(0x1000004,0x1001000);put(0x1001028,3);put(0x6ba3f0,0x1000000);put(0x6ba3ec,0);put(0xc23a18,0xb333);run(0x42a6e0)
 for key,at in [('actor',0x1000024),('camera',0x6ba3f4)]:
  words=struct.unpack('<6I',u.mem_read(at,24));expected={'position':[signed(v)for v in words[:3]],'angles':list(words[3:])};assert row['start'][key]==expected,(row['mission'],key,row['start'][key],expected)
 assert row['start']['marker_index']==index
 assert row['start']['marker_object_id']==struct.unpack_from('<I',marker,8)[0]
 starts.append(row['mission'])
campaign=[r for r in rows if r['mission'].lower().startswith(('c02m','c03m','c04m','c05m','c06m'))];assert len(campaign)==40 and all(r['start']for r in campaign)
report={'source_sha256':sha,'angle_cases':len(cases),'sp_flag_cases':256,'missions':len(rows),'campaign_starts':40,'starts':starts,'missing_sp_start':missing,'original_functions':['4497B5–4497ED','47DF40','43F610','42A6E0'],'mocked_function':'4828C0 type lookup returns raw type IDs for the equality test','mismatches':0}
text=json.dumps(report,indent=2);print(text)
if args.report:args.report.write_text(text+'\n')
