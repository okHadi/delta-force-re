import hashlib,json,struct,subprocess
from pathlib import Path
import argparse
parser = argparse.ArgumentParser(description="Compare native terrain with original DF1 x86 code.")
parser.add_argument("game", type=Path)
parser.add_argument("probe", type=Path)
parser.add_argument("--report", type=Path)
args = parser.parse_args()
import pefile,unicorn
from unicorn.x86_const import *
source=(args.game/'Df.exe').read_bytes();pe=pefile.PE(data=source)
assert hashlib.sha256(source).hexdigest()=="abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff"
u=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
for address,size in ((0x49b000,4096),(0x4cd000,4096),(0x477000,4096),(0x489000,4096),(0x4fb000,4096),(0xbd0000,0x3000),(0xc94000,4096),(0xc97000,8192),(0xcb8000,0x7000),(0x2000000,65536),(0x3500000,4096),(0x4000000,4096)):
 u.mem_map(address,size)
for address in (0x477000,0x489000,0x49b000,0x4cd000):
 off=pe.get_offset_from_rva(address-0x400000);u.mem_write(address,source[off:off+4096])
u.mem_write(0x4fb2f8,struct.pack('<I',0xbd0868))
u.mem_write(0x2008000,struct.pack('<I',0x4000000));u.reg_write(UC_X86_REG_ESP,0x2008000);u.reg_write(UC_X86_REG_FPCW,0x037f)
u.emu_start(0x477510,0x4000000,count=1000000,timeout=10000000)
assert u.reg_read(UC_X86_REG_EIP)==0x4000000
u.mem_write(0xc94aa0,struct.pack('<I',0x3500000))
mask=0xffffffff
for seed in range(40):
 state=seed
 def next():
  global state
  state=(state*1664525+1013904223)&mask
  return state
 width=320+160*(seed%4);height=200+40*(seed%5);focal=width//2
 position=[(next()&0x7fffffff)-0x40000000 for _ in range(3)];yaw=next();pitch=next()
 if seed<4:yaw=seed*0x40000000;pitch=0
 ctx=bytearray(256)
 for off,value in ((4,position[0]),(8,position[1]),(12,position[2]),(16,yaw),(20,pitch),(0x34,0),(0x38,0),(0x3c,width-1),(0x40,height-1),(0x54,focal),(0x58,width//2),(0x5c,height//2)):
  struct.pack_into('<I',ctx,off,value&mask)
 u.mem_write(0x3500000,bytes(ctx));u.mem_write(0xc975a8,struct.pack('<'+'I'*width,*[(height-1)*width+x for x in range(width)]))
 sp=0x2008000;u.mem_write(sp,struct.pack('<I',0x4000000));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_EFLAGS,2)
 u.emu_start(0x489a00,0x4000000,timeout=10000000,count=1000000)
 assert u.reg_read(UC_X86_REG_EIP)==0x4000000
 expected=[]
 for i in range(width//2):
  values=list(struct.unpack('<12I',u.mem_read(0xcb8d84+i*48,48)));values[0]=(values[0]-0xc985a8)//4;expected.append(values)
 acceleration=list(struct.unpack('<3I',u.mem_read(0xcbedc4,12)))
 actual=json.loads(subprocess.check_output([str(args.probe.resolve()),'--camera-fixture',str(seed)]))
 assert actual['columns']==expected,('columns',seed)
 assert actual['acceleration']==acceleration,('acceleration',seed,actual['acceleration'],acceleration)
report={'source_sha256':hashlib.sha256(source).hexdigest(),'procedure':'0x489a00','unicorn_version':unicorn.__version__,'cases':40,'columns':sum((320+160*(i%4))//2 for i in range(40)),'mismatches':0}
(args.report or Path('.local/reports/terrain-camera-differential.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
