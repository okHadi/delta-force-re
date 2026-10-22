#!/usr/bin/env python3
"""Compare live model geometry with the hash-bound original x86 routines.

Use local original files. Geometry helpers execute without hooks. The ground
query supplies only the terrain-height result. World rays execute 42DB80.
"""
from pathlib import Path
import sys,struct,json,random,subprocess,hashlib,argparse,tempfile
sys.path.insert(0,str(Path(__file__).resolve().parent))
import check_model_instances_original as base
import unicorn
from unicorn.x86_const import *
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('executable',type=Path)
parser.add_argument('probe',type=Path)
parser.add_argument('--models',type=Path,required=True)
parser.add_argument('--report',type=Path)
parser.add_argument('--chr',type=Path,help='Optional CHR for current-pose org0 callback checks')
args=parser.parse_args()
original=base.Original(args.executable);u=original.unicorn
ACTOR=0x2100000;ENTITY=0x2101000;MODEL=0x2102000;LOD=0x2103000;LIST=0x2104000;POINT=0x2105000
MESH=0x2110000;VERT=0x2120000;NORMAL=0x2140000;FACE=0x2180000;MATERIAL=0x2300000
terrain=0
calls={a:0 for a in (0x480ff0,0x45d170,0x45ce60,0x45cda0,0x42d2a0)}
def w(at,v):u.mem_write(at,struct.pack('<I',v&0xffffffff))
def r(at):return struct.unpack('<I',u.mem_read(at,4))[0]
def sg(v):return (v+2**31)%2**32-2**31
def hook(_u,at,size,_data):
 if at in calls:calls[at]+=1
 if at==0x47df40:
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,terrain&0xffffffff)
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,r(sp))
u.hook_add(unicorn.UC_HOOK_CODE,hook)
def parse(path):
 b=path.read_bytes();wc=lambda at:struct.unpack_from('<I',b,at)[0]
 at=128
 for _ in range(wc(124)):at+=36+wc(at+16)+1024
 hs=at;vc,nc,fc,mc=[wc(hs+i) for i in (128,136,144,152)]
 at+=184;verts=b[at:at+vc*8];at+=vc*8;normals=b[at:at+nc*8];at+=nc*8
 faces=b[at:at+fc*72];at+=fc*72;meshes=b[at:at+mc*112];at+=mc*112
 hardpoints=b[at:at+wc(hs+160)*12]
 at+=wc(hs+160)*12+wc(hs+176)*72
 materials=b[at:at+wc(hs+168)*76]
 return b,verts,normals,faces,meshes,materials,b[hs:hs+184],hardpoints

def load(path):
 b,verts,normals,faces,meshes,materials,header,hardpoints=parse(path)
 u.mem_write(MODEL,b[:124]);w(MODEL+0x4c,0x480ff0);w(MODEL+0x50,LOD)
 u.mem_write(LOD,header);w(LOD+0x98,len(meshes)//112);w(LOD+0x9c,MESH);w(LOD+0xa4,0x2310000)
 if hardpoints:u.mem_write(0x2310000,hardpoints)
 u.mem_write(MESH,meshes);u.mem_write(VERT,verts);u.mem_write(NORMAL,normals);u.mem_write(FACE,faces);u.mem_write(MATERIAL,materials)
 # Loader-localized vertices. Raw face constants and boxes keep stored values.
 if struct.unpack_from('<I',header,16)[0]&1:
  vertices=bytearray(verts);parts=bytearray(meshes)
  for index in range(len(verts)//8):
   owner=struct.unpack_from('<H',verts,index*8+6)[0]
   for axis in range(3):
    value=struct.unpack_from('<h',verts,index*8+axis*2)[0]
    origin=struct.unpack_from('<i',meshes,owner*112+52+axis*4)[0]
    struct.pack_into('<H',vertices,index*8+axis*2,(value-(origin>>8))&0xffff)
  for index in range(len(meshes)//112):
   parent=struct.unpack_from('<I',meshes,index*112+36)[0]
   for axis in range(3):
    origin=struct.unpack_from('<i',meshes,index*112+52+axis*4)[0]
    parent_origin=struct.unpack_from('<i',meshes,parent*112+52+axis*4)[0]
    struct.pack_into('<I',parts,index*112+40+axis*4,(origin-parent_origin)&0xffffffff)
  u.mem_write(VERT,bytes(vertices));u.mem_write(MESH,bytes(parts))
 vb=nb=fb=0
 for i in range(len(meshes)//112):
  at=MESH+i*112;vc=r(at+4);fc=r(at+12);nc=r(at+20)
  w(at+8,VERT+vb*8);w(at+16,FACE+fb*72);w(at+24,NORMAL+nb*8)
  for f in range(fc):
   p=FACE+(fb+f)*72;w(p+68,MATERIAL+r(p+68)*76)
  vb+=vc;nb+=nc;fb+=fc
 return b,len(meshes)//112

def make_rows(b,count):
 rng=random.Random(0x486fd0+count)
 rows=[];radius=struct.unpack_from('<i',b,0x64)[0]
 for i in range(150):
  translation=[rng.randrange(-20,20)*65536 for _ in range(3)]
  angles=[rng.choice([0,0,0,0x40000000,0x80000000,rng.randrange(2**32)]) for _ in range(3)]
  if i<80:angles[1:]=[0,0]
  q=[sg(translation[0]+rng.randrange(-max(radius,1),max(radius,1))),sg(translation[1]+rng.randrange(-max(radius,1),max(radius,1))),sg(translation[2]+rng.randrange(-3,10)*65536)]
  rows.append([*translation,*angles,*q,-0x10000000,0x2000000,rng.choice([0,8,0x1008]),rng.choice([0,1,1,1]),rng.choice([0,2])])
 # Deliberately hit the inside of nearly horizontal stored faces, plus nearby miss/depth cases.
 _,verts,normals,faces,meshes,_,_,_=parse(current)
 vb=nb=fb=0
 for mi in range(len(meshes)//112):
  vc,fc,nc=[struct.unpack_from('<I',meshes,mi*112+k)[0] for k in (4,12,20)]
  for fi in range(fc):
   f=faces[(fb+fi)*72:(fb+fi+1)*72];ni=struct.unpack_from('<H',f,2)[0]+nb
   normal=struct.unpack_from('<3h',normals,ni*8)
   if abs(normal[2])<64:continue
   ids=struct.unpack_from('<3h',f,28)
   positions=[struct.unpack_from('<3h',verts,(vb+j)*8) for j in ids]
   point=[sum(p[a] for p in positions)*256//3 for a in range(3)]
   for variant in range(3):
    t=[0,0,0] if variant==0 else [5*65536,-7*65536,11*65536]
    q=[point[a]+t[a] for a in range(3)]
    rows.append([*t,0,0,0,*q,-0x10000000,0x1000000 if variant<2 else -0x1000000,0,1,0])
  vb+=vc;nb+=nc;fb+=fc
 return rows
reports=[];checked=hits=0
for current in [args.models/n for n in ['CRATE1.3DI','CRATE2.3DI','BRIDGE.3DI','HOUSE7.3DI','BARAK.3DI','OILTANK.3DI','RUIN8.3DI']]:
 b,count=load(current)
 if struct.unpack_from('<I',b,40)[0]!=0x676e7263:raise ValueError('model is not gnrc')
 rows=make_rows(b,count)
 result=subprocess.run([str(args.probe.resolve()),str(current)],input=''.join(' '.join(map(str,row))+'\n' for row in rows),capture_output=True,text=True,check=True)
 native=[list(map(int,line.split())) for line in result.stdout.splitlines()]
 if len(native)!=len(rows):raise ValueError('native row count differs')
 local_hits=0
 for i,(row,got) in enumerate(zip(rows,native)):
  translation=row[:3];angles=row[3:6];query=row[6:9];terrain=row[9];depth=row[10];flags,primary,alternate=row[11:]
  u.mem_write(ACTOR,bytes(0x114));w(ACTOR+0x104,LIST);w(ACTOR+0x108,1);w(LIST,ENTITY)
  u.mem_write(ENTITY,bytes(0x114));w(ENTITY+0x10,MODEL if primary else 0);w(ENTITY+0x14,MODEL if alternate else 0);w(ENTITY+0x6c,flags)
  for a,v in enumerate(translation+angles):w(ENTITY+0x24+a*4,v)
  u.mem_write(POINT,struct.pack('<3i',*query))
  original.call(0x486fd0,[ACTOR,POINT,depth])
  height=sg(u.reg_read(UC_X86_REG_EAX));hit=int(r(ACTOR+0x100)==ENTITY)
  want=[struct.unpack_from('<i',b,0x64)[0],struct.unpack_from('<i',b,0x60)[0],height,hit]
  if got[:4]!=want:raise ValueError(f'{current.name} case{i}: native{got} original{want} row{row}')
  checked+=1;hits+=hit;local_hits+=hit
 reports.append(dict(model=current.name,sha256=hashlib.sha256(b).hexdigest(),cases=len(rows),hits=local_hits))

# Full original world-list query, using the same stored models and live transforms.
CONTEXT=0x2320000
world_cases=world_hits=0
for current in [args.models/n for n in ['CRATE1.3DI','CRATE2.3DI','BRIDGE.3DI','HOUSE7.3DI','BARAK.3DI','OILTANK.3DI','RUIN8.3DI']]:
 b,count=load(current)
 ground_rows=make_rows(b,count)
 rows=[]
 for index,row in enumerate(ground_rows):
  placement=row[:6];q=row[6:9];flags,primary,alternate=row[11:]
  origin=[q[0],q[1],sg(q[2]+0x20000)]
  end=[q[0],q[1],sg(q[2]-0x20000)]
  rows.append([*placement,*origin,*end,0,0,-65536,0x40000,flags,primary,alternate,
               0 if index%17==0 else 1,17,17 if index%19==0 else 0,0])
 output=subprocess.run([str(args.probe.resolve()),'--world',str(current)],input=''.join(' '.join(map(str,row))+'\n' for row in rows),capture_output=True,text=True,check=True)
 native=[list(map(int,line.split())) for line in output.stdout.splitlines()]
 if len(native)!=len(rows):raise ValueError('world query native row count differs')
 for index,(row,got) in enumerate(zip(rows,native)):
  placement=row[:6];origin=row[6:9];end=row[9:12];direction=row[12:15]
  length,flags,primary,alternate,active,identity,skip1,skip2=row[15:]
  u.mem_write(ENTITY,bytes(0x114));w(ENTITY,active)
  w(ENTITY+0x10,MODEL if primary else 0);w(ENTITY+0x14,MODEL if alternate else 0);w(ENTITY+0x6c,flags)
  for axis,value in enumerate(placement):w(ENTITY+0x24+axis*4,value)
  u.mem_write(CONTEXT,bytes(0x80))
  for axis,value in enumerate(origin):w(CONTEXT+axis*4,value)
  for axis,value in enumerate(end):w(CONTEXT+0x10+axis*4,value)
  for axis,value in enumerate(direction):w(CONTEXT+0x20+axis*4,value)
  w(CONTEXT+0x30,length)
  for axis in range(3):
   w(CONTEXT+0x34+axis*8,min(origin[axis],end[axis]));w(CONTEXT+0x38+axis*8,max(origin[axis],end[axis]))
  w(CONTEXT+0x4c,ENTITY if skip1==identity else 0);w(CONTEXT+0x50,ENTITY if skip2==identity else 0)
  before=calls[0x480ff0]
  original.call(0x42db80,[ENTITY,1,CONTEXT])
  callbacks=calls[0x480ff0]-before
  code=sg(u.reg_read(UC_X86_REG_EAX));hit=int(r(CONTEXT+0x58)!=0)
  want=[code,hit,sg(r(CONTEXT+0x5c)) if hit else 0,r(CONTEXT+0x60) if hit else 0,
        r(CONTEXT+0x64) if hit else 0,r(r(CONTEXT+0x54)+0x10) if hit else 0]
  if got[:6]!=want:raise ValueError(f'{current.name} world{index}: native{got} original{want} row{row}')
  if len(got)!=8 or got[7]!=callbacks:raise ValueError(f'{current.name} world{index}: deferred callback count differs')
  world_cases+=1;world_hits+=hit

# Resolve and execute every callback-table row, without a draw or transform hook.
callback_rows=[]
for index in range(16):
 at=0x500c1c+index*20
 callback_rows.append((r(at),r(at+8)))
stock={}
for path in sorted(args.models.glob('*')):
 raw=path.read_bytes()
 if raw[:4]==b'3DI\x05':stock.setdefault(struct.unpack_from('<I',raw,40)[0],path)
callback_cases=callback_slots=character_cases=0
callback_sources=[]
with tempfile.TemporaryDirectory(prefix='dfre-geometry-') as folder:
 for key,address in callback_rows:
  path=stock.get(key)
  if path is None:
   template='CRATE1.3DI' if key==0x66616365 else 'BLACKHWK.3DI'
   data=bytearray((args.models/template).read_bytes());struct.pack_into('<I',data,40,key)
   path=Path(folder)/(hex(key)+'.3di');path.write_bytes(data)
  b,count=load(path)
  callback_sources.append(dict(tag=hex(key),model=path.name,sha256=hashlib.sha256(b).hexdigest(),synthetic=key not in stock))
  cases=[]
  for case in range(12):
   generator=random.Random(key+case)
   image=[0]*69
   for axis in range(3):image[(0x24+axis*4)//4]=(generator.randrange(-20,20)*65536)&0xffffffff
   for axis in range(3):image[(0x30+axis*4)//4]=generator.randrange(2**32)
   for offset in (0x94,0x98,0x9c,0xa0,0xa4,0xc4,0xc8,0xe0,0xe4,0xe8,0xec):
    image[offset//4]=generator.randrange(0x1000000)
   image[0xf0//4]=case%3;image[0x7c//4]=65536
   if key==0x70616c6d:image[0xe0//4]=[0,1,2,0x10,0x11,0x20][case%6]
   if key==0x68656c6f:image[0x98//4]=0 if case%2 else 0x0c000000
   prior=[(case*17+mesh*37)&0xff for mesh in range(count)]
   cases.append((image,prior,[]))
  # Non-null CHR calls use real parent indices and current pose words.
  if key==0x6f726730 and args.chr:
   raw=args.chr.read_bytes();bone_count=struct.unpack_from('<I',raw,8)[0]
   map_at,map_count=struct.unpack_from('<2I',raw,12)
   if map_count!=count:raise ValueError('org0 stock model mesh count differs from CHR map')
   bone_offsets=[];bone_at=struct.unpack_from('<I',raw,4)[0]
   for _ in range(bone_count):
    bone_offsets.append(bone_at);bone_at+=92 if struct.unpack_from('<I',raw,bone_at+24)[0]&1 else 60
   parents=[struct.unpack_from('<i',raw,bone_offsets[struct.unpack_from('<I',raw,map_at+i*4)[0]]+56)[0] for i in range(count)]
   for case in range(20):
    image=list(cases[case%12][0]);image[0x18//4]=1
    prior=[(case*7+mesh*13)&0xff for mesh in range(count)]
    generator=random.Random(0x481130+case)
    pose=[[generator.randrange(-0x1000000,0x1000000) for _ in range(3)]+[65536]*3 for _ in range(count)]
    cases.append((image,prior,pose))
  payload=[]
  for case,(image,prior,pose) in enumerate(cases):payload.append(' '.join(map(str,[*image,*prior,(case*19+0x53)&255,*[v for bone in pose for v in bone]])))
  # Null-character cases omit pose words even when a CHR path is available.
  native=[]
  for has_pose in (False,True):
   selected=[(i,c) for i,c in enumerate(cases) if bool(c[2])==has_pose]
   if not selected:continue
   command=[str(args.probe.resolve()),'--callback',str(path)]+([str(args.chr)] if has_pose else [])
   output=subprocess.run(command,input=''.join(payload[i]+'\n' for i,_ in selected),capture_output=True,text=True)
   if output.returncode:raise ValueError(f'callback {key:#x}: {output.stderr}')
   values=[list(map(int,line.split())) for line in output.stdout.splitlines()]
   if len(values)!=len(selected):raise ValueError('callback native row count differs')
   native.extend((i,value) for (i,_),value in zip(selected,values))
  for index,got in sorted(native):
   image,prior,pose=cases[index]
   u.mem_write(ENTITY,struct.pack('<69I',*image))
   if pose:
    character=0x2330000;bone_table=0x2331000;pose_table=0x2332000;bone_base=0x2333000
    w(ENTITY+0x18,character);w(character+0x14,count);w(character+0x7c,bone_table);w(character+0x80,pose_table)
    for bone in range(count):
     w(bone_table+bone*4,bone_base+bone*64);w(bone_base+bone*64+56,parents[bone])
     u.mem_write(pose_table+bone*24,struct.pack('<6i',*pose[bone]))
   u.mem_write(0xc22b80,bytes([0xa7])*0x1000)
   for mesh,flag in enumerate(prior):w(0xc22b80+mesh*64+60,flag)
   # 44D2A0 local matrix +3C is copied from this unwritten stack word.
   w(base.STACK-16-44,(index*19+0x53)&255)
   original.call(address,[LOD,ENTITY,0])
   present=bool(u.reg_read(UC_X86_REG_EAX))
   if bool(got[0])!=present:raise ValueError(f'callback {key:#x} presence differs')
   if present:
    if got[1]<count:raise ValueError('callback native slot count is short')
    for mesh in range(count):
     flag=r(0xc22b80+mesh*64+60)&0xff;at=2+mesh*13
     if got[at]!=flag:raise ValueError(f'callback {key:#x} case{index} slot{mesh} flag original{flag}, native{got[at]}')
     # Localized slots are skipped; their old matrix words stay caller scratch state.
     if not flag&1:
      matrix=list(struct.unpack('<12i',u.mem_read(0xc22b80+mesh*64,48)))
      if got[at+1:at+13]!=matrix:raise ValueError(f'callback {key:#x} case{index} slot{mesh}: original{matrix}, native{got[at+1:at+13]}')
     callback_slots+=1
   callback_cases+=1;character_cases+=bool(pose)

# Execute the real 485E30 loader with source LOD bytes. Only file-read and
# allocation services are supplied; pointer slicing and geometry writes run.
u.mem_map(0x2500000,0x1000000)
loader={'reads':[],'heap':0x2500000}
w(0x500b0c,0x2005000);u.mem_write(0x2005000,bytes(range(256)))
w(0x500b10,0x2004000);u.mem_write(0x2004000,bytes(range(256)))
def loader_hook(_u,address,_size,_data):
 sp=u.reg_read(UC_X86_REG_ESP)
 if address==0x499280:
  destination,length=r(sp+4),r(sp+8)
  if not loader['reads']:raise ValueError('original loader read beyond supplied source')
  data=loader['reads'].pop(0)
  if length!=len(data):raise ValueError('original loader requested an unexpected source span')
  u.mem_write(destination,data)
 else:
  size,alignment=r(sp+4),r(sp+8)
  pointer=(loader['heap']+alignment-1)&~(alignment-1)
  loader['heap']=pointer+size
  if loader['heap']>0x3500000:raise ValueError('original loader test heap is full')
  u.reg_write(UC_X86_REG_EAX,pointer)
 u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,r(sp))
u.hook_add(unicorn.UC_HOOK_CODE,loader_hook,None,0x499280,0x499280)
u.hook_add(unicorn.UC_HOOK_CODE,loader_hook,None,0x497f60,0x497f60)
SECONDARY=MODEL+0x80
selected_model=[0]
def selected_read(_u,_access,address,_size,_value,_data):
 selected_model[0]=1 if address==MODEL+0x64 else 2
u.hook_add(unicorn.UC_HOOK_MEM_READ,selected_read,None,MODEL+0x64,MODEL+0x64)
u.hook_add(unicorn.UC_HOOK_MEM_READ,selected_read,None,SECONDARY+0x64,SECONDARY+0x64)

def original_load_lod(path):
 b,verts,normals,faces,meshes,materials,header,hardpoints=parse(path)
 wc=lambda at:struct.unpack_from('<I',b,at)[0]
 start=128
 for _ in range(wc(124)):start+=36+wc(start+16)+1024
 total=struct.unpack_from('<I',header,20)[0]
 loader['reads']=[header,b[start+184:start+184+total]];loader['heap']=0x2500000
 w(0xc65dcc,0);w(0xc80d10,0)
 original.call(0x485e30,[0,0,0])
 loaded=u.reg_read(UC_X86_REG_EAX)
 if not loaded or loader['reads']:raise ValueError('original LOD loader did not consume the stored first LOD')
 key=struct.unpack_from('<I',b,40)[0]
 address=next(address for tag,address in callback_rows if tag==key)
 for model in (MODEL,SECONDARY):
  u.mem_write(model,b[:124]);w(model+0x4c,address);w(model+0x50,loaded)
 return b,loaded,address

collision_names=['CRATE1.3DI','CRATE2.3DI','BRIDGE.3DI','HOUSE7.3DI','BARAK.3DI',
 'OILTANK.3DI','RUIN8.3DI','PICKUP.3DI','BMP.3DI','BRDM.3DI','BLACKHWK.3DI',
 'PALMX1.3DI','PINE.3DI','HUMVEE.3DI','RUSS04.3DI','TEMPLEA.3DI']
collision_cases=collision_hits=collision_callbacks=collision_volume_fields=0
collision_models=[]
for name in collision_names:
 path=args.models/name;b,loaded,address=original_load_lod(path)
 present=bool(r(loaded+0xb0));mesh_count=r(loaded+0x98);mesh_pointer=r(loaded+0x9c)
 # Compare typed native decoding with records produced by original485E30.
 want=[sg(r(MODEL+0x64)),int(present),mesh_count if present else 0]
 model_volumes=[]
 if present:
  for mesh in range(mesh_count):
   part=mesh_pointer+mesh*112;count=r(part+0x1c);volume_pointer=r(part+0x20)
   want.extend(struct.unpack('<6i',u.mem_read(part+0x40,24)));want.append(count)
   for volume in range(count):
    words=list(struct.unpack('<18I',u.mem_read(volume_pointer+volume*72,72)))
    values=[value if index<2 else sg(value) for index,value in enumerate(words)]
    want.extend(values[:11]+values[12:]);collision_volume_fields+=17
    model_volumes.append((mesh,values))
 native=subprocess.run([str(args.probe.resolve()),'--volumes',str(path)],capture_output=True,text=True,check=True)
 got=list(map(int,native.stdout.split()))
 if got!=want:raise ValueError(f'{name}: collision volume decode differs from original485E30')
 rng=random.Random(0x4889c0+len(model_volumes))
 rows=[]
 radius=struct.unpack_from('<i',b,0x64)[0]
 for index in range(120):
  transform=[rng.randrange(-8,9)*65536 for _ in range(3)]+[rng.choice([0,0x40000000,0x80000000,rng.randrange(2**32)]),0,0]
  point=[sg(transform[axis]+rng.randrange(-max(radius*2,1),max(radius*2,1))) for axis in range(3)]
  flags=rng.choice([0,8,0x1008]);primary=rng.choice([0,1,1]);alternate=rng.choice([0,2])
  rows.append([*transform,*point,flags,primary,alternate,0x10 if index%7==0 else 0])
 # Actor points are sampled from actual loaded volume centres and edges.
 for mesh,volume in model_volumes:
  for variant in range(12):
   transform=[3*65536,-4*65536,5*65536,variant*0x10000000,0,0]
   u.mem_write(ENTITY,bytes(0x114))
   for axis,value in enumerate(transform):w(ENTITY+0x24+axis*4,value)
   original.call(address,[loaded,ENTITY,0]);matrices=u.reg_read(UC_X86_REG_EAX)
   if not matrices:continue
   point=volume[2:5].copy()
   point[variant%3]+=(-1 if variant&1 else 1)*(0 if variant<3 else 0xbfff)
   u.mem_write(POINT,struct.pack('<3i',*point))
   original.call(0x45cda0,[matrices+mesh*64,POINT,POINT+0x20])
   world=list(struct.unpack('<3i',u.mem_read(POINT+0x20,12)))
   rows.append([*transform,*world,8 if variant&1 else 0,1,2 if variant&1 else 0,0x10 if variant%5==0 else 0])
 native=subprocess.run([str(args.probe.resolve()),'--collision',str(path)],input=''.join(' '.join(map(str,row))+'\n' for row in rows),capture_output=True,text=True)
 if native.returncode:raise ValueError(f'{name}: native actor collision failed: {native.stderr}')
 replies=[list(map(int,line.split())) for line in native.stdout.splitlines()]
 if len(replies)!=len(rows):raise ValueError('native actor collision row count differs')
 model_hits=0
 for index,(row,got) in enumerate(zip(rows,replies)):
  transform=row[:6];point=row[6:9];flags,primary,alternate,definition_flags=row[9:]
  u.mem_write(ACTOR,bytes(0x114));u.mem_write(ENTITY,bytes(0x114))
  for axis,value in enumerate(point):w(ACTOR+0x24+axis*4,value)
  for axis,value in enumerate(transform):w(ENTITY+0x24+axis*4,value)
  w(ENTITY+0x10,MODEL if primary else 0);w(ENTITY+0x14,SECONDARY if alternate else 0);w(ENTITY+0x6c,flags)
  w(ENTITY+4,0x2350000);w(0x2350024,definition_flags);w(0x67f528,0)
  selected_model[0]=0;callback_count=[0]
  def called(_u,_address,_size,_data):callback_count[0]+=1
  hook_id=u.hook_add(unicorn.UC_HOOK_CODE,called,None,address,address)
  original.call(0x4889c0,[ACTOR,ENTITY]);u.hook_del(hook_id)
  code=u.reg_read(UC_X86_REG_EAX)
  want=[code,*struct.unpack('<3i',u.mem_read(ACTOR+0x24,12)),callback_count[0],selected_model[0]]
  if got!=want:raise ValueError(f'{name} collision{index}: native{got} original{want} row{row}')
  collision_cases+=1;collision_hits+=bool(code);collision_callbacks+=callback_count[0];model_hits+=bool(code)
 collision_models.append(dict(name=name,sha256=hashlib.sha256(b).hexdigest(),meshes=mesh_count,
  volumes=len(model_volumes),cases=len(rows),hits=model_hits))
report=dict(source_sha256=base.EXPECTED_HASH,cases=checked,hits=hits,models=reports,
 world_cases=world_cases,world_hits=world_hits,callback_cases=callback_cases,
 callback_slots=callback_slots,character_pose_cases=character_cases,callback_sources=callback_sources,
 collision_cases=collision_cases,collision_hits=collision_hits,collision_callbacks=collision_callbacks,
 collision_volume_fields=collision_volume_fields,collision_models=collision_models,
 original_helper_calls={hex(a):n for a,n in calls.items()},
 hooks=['terrain47DF40 returns explicit terrain height; no geometry helper hooks',
        '485E30 file reads and allocation supplied from the unchanged source model'],mismatches=0)
if args.report:args.report.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
