#!/usr/bin/env python3
"""Replay the complete projectile pass against local stock model geometry.

Original model callbacks, model hit selection, damage and impact branches run
unchanged. Terrain returns clear. Foreign effects are recorded without audio.
"""
from pathlib import Path
import argparse, hashlib, json, random, struct, subprocess
import check_model_instances_original as base
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EIP
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('executable',type=Path)
parser.add_argument('probe',type=Path)
parser.add_argument('--models',type=Path,required=True)
parser.add_argument('--report',type=Path)
parser.add_argument('--game',type=Path,help='Also replay stock terrain queries from this game folder')
args=parser.parse_args()
original=base.Original(args.executable);u=original.unicorn
ACTOR=0x7dde80;ENTITY=0x9f3098;MODEL=0x2102000;LOD=0x2103000
MESH=0x2110000;VERT=0x2120000;NORMAL=0x2140000;FACE=0x2180000;MATERIAL=0x2300000
DEFINITION=0x2320000;CALLBACK=0x2330000
records=[];counts={}
def w(at,v):u.mem_write(at,struct.pack('<I',v&0xffffffff))
def r(at):return struct.unpack('<I',u.mem_read(at,4))[0]
def sg(v):return (v+2**31)%2**32-2**31
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

def hook(_u,at,size,_data):
 if at not in {0x47e390,0x463350,0x4657d0,0x45dc70,0x43e7d0,0x460d20,0x45fcf0,0x45dab0,CALLBACK}:return
 counts[hex(at)]=counts.get(hex(at),0)+1
 sp=u.reg_read(UC_X86_REG_ESP)
 if at==0x47e390:value=1
 else:
  value=0
  arg=0 if at in (0x463350,0x45dab0,0x460d20) else r(sp+4)
  if at==0x45fcf0:arg=r(sp+16)
  if at==CALLBACK:arg=r(sp+8)
  records.extend((0x6000000 if at==CALLBACK else at,arg))
 u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,r(sp))
foreign_hook=u.hook_add(unicorn.UC_HOOK_CODE,hook)
rng=random.Random(0x464eb0);report={'source_sha256':original.digest,'models':[],'cases':0,'hits':0,'mismatches':[]}
for name in ['CRATE1.3DI','CRATE2.3DI','BRIDGE.3DI','HOUSE7.3DI','BARAK.3DI','OILTANK.3DI','RUIN8.3DI']:
 path=args.models/name;b,count=load(path)
 raw,verts,normals,faces,meshes,materials,header,hardpoints=parse(path)
 rows=[]
 radius=max(65536,struct.unpack_from('<i',raw,0x64)[0])
 for index in range(80):
  group=index%2;pos=[rng.randrange(-8,8)*65536 for _ in range(3)]
  angles=[rng.choice([0,0x40000000,0x80000000,rng.randrange(2**32)]),0,0]
  origin=[pos[0]+rng.randrange(-radius,radius),pos[1]+rng.randrange(-radius,radius),pos[2]+radius+65536]
  velocity=[0,0,-2*radius-131072]
  rows.append([group,*pos,*angles,*origin,*velocity,rng.choice([0,0,0x10,0x20,0x40,0x100]),10,rng.choice([0,1,3,5]),rng.choice([0,0,100]),MATERIAL])
 # Aim through face centroids with the stored mesh origin restored.
 vb=fb=0
 for mi in range(len(meshes)//112):
  vc,fc=struct.unpack_from('<I',meshes,mi*112+4)[0],struct.unpack_from('<I',meshes,mi*112+12)[0]
  for fi in range(min(fc,30)):
   f=faces[(fb+fi)*72:(fb+fi+1)*72];ids=struct.unpack_from('<3H',f,28)
   if any(i>=vc for i in ids):continue
   point=[sum(struct.unpack_from('<3h',verts,(vb+i)*8)[axis] for i in ids)*256//3 for axis in range(3)]
   for group in (0,1):rows.append([group,0,0,0,0,0,0,point[0],point[1],point[2]+65536,0,0,-131072,0,10,3,0,MATERIAL])
  vb+=vc;fb+=fc
 for index,row in enumerate(rows):row.append(0 if index%2 else -1)
 result=subprocess.run([str(args.probe.resolve()),str(path)],input=''.join(' '.join(map(str,row))+'\n' for row in rows),text=True,capture_output=True,check=True)
 native=[list(map(int,line.split())) for line in result.stdout.splitlines()]
 assert len(native)==len(rows)
 hits=0
 for index,(row,got) in enumerate(zip(rows,native)):
  group=row[0];actor=0x867e80 if group==1 else 0x7dde80
  u.mem_write(0x7dde80,bytes(0x114));u.mem_write(0x867e80,bytes(0x114));u.mem_write(0x7cca80,bytes(0x114))
  w(0x8cf904,int(group==1));w(0x8cfa1c,int(group==0));w(0x8cfa20,int(row[18]==0))
  if row[18]==0:w(0x7cca80,DEFINITION);w(0x7cca80+0x24,0x7000000)
  u.mem_write(actor,bytes(0x114));w(actor,DEFINITION);w(actor+4,DEFINITION);w(actor+0x10,MODEL);w(actor+0x110,CALLBACK);w(actor+0x74,100)
  for at,v in zip((0x24,0x28,0x2c),row[1:4]):w(actor+at,v)
  for at,v in zip((0x30,0x34,0x38),row[4:7]):w(actor+at,v)
  u.mem_write(DEFINITION,bytes(0x200));w(DEFINITION+0x84,CALLBACK);w(DEFINITION+0x98,row[16])
  u.mem_write(ENTITY,bytes(0x22800));w(ENTITY,1)
  for at,v in zip((0x24,0x28,0x2c),row[7:10]):w(ENTITY+at,v)
  for at,v in zip((0x3c,0x40,0x44),row[10:13]):w(ENTITY+at,v)
  for at,v in ((0x6c,row[13]),(0xfc,row[14]),(0x84,row[14]),(0x80,row[15]),(0xf8,0x7cca80 if row[18]==0 else 0)):w(ENTITY+at,v)
  for at,v in ((0x67f528,1),(0x67f520,0),(0x67f548,0),(0x67f549,0),(0xbd5248,0),(0xc23820,0),(0xa158c0,0),(0xa158bc,0),(0x6e4708,0x4000000),(0x6e4704,0x4000000),(0x6e4700,0)):w(at,v)
  records.clear();original.call(0x464eb0,[])
  want=[sg(r(ENTITY+at)) for at in (0x24,0x28,0x2c,0x3c,0x40,0x44,0xfc)]+[r(ENTITY),r(actor+0x74)&0xffff,r(0xa158c0),r(0xa158bc),0x76030000 if r(actor+0xf8)==0x7cca80 else r(actor+0xf8),len(records)//2,*records]
  if r(actor+0x74)!=100:hits+=1
  if got!=want:
   report['mismatches'].append({'model':name,'row':index,'input':row,'native':got,'original':want})
   if len(report['mismatches'])<=3:print(report['mismatches'][-1])
 report['models'].append({'name':name,'sha256':hashlib.sha256(raw).hexdigest(),'cases':len(rows),'hits':hits})
 report['cases']+=len(rows);report['hits']+=hits
 print(name,len(rows),'cases',hits,'damage hits')
if args.game:
 from mission import Resolver,terrain_name
 from pcx import decode
 resolver=Resolver(args.game);u.hook_del(foreign_hook)
 terrain_report={'cases':0,'hits':0,'clear':0,'positive_hits':0,'zero_hits':0,'missions':[]}
 seen=set()
 for archive in resolver.archives:
  for entry in archive.entries:
   if entry.deleted or not entry.name.lower().endswith('.bms') or entry.name.lower() in seen:continue
   seen.add(entry.name.lower());mission,_=resolver.read(archive,entry)
   trn,_=resolver.read(*resolver.find(terrain_name(mission)));fields={}
   for line in trn.decode('ascii').splitlines():
    tokens=line.split()
    if len(tokens)>=2:fields[tokens[0]]=tokens[1]
   image_bytes,_=resolver.read(*resolver.find(fields['elev_map']+'.PCX'));image=decode(image_bytes)
   assert image.width==image.height==1024
   heights=b''.join(image.pixels[y*1024:(y+1)*1024] for y in reversed(range(1024)))
   u.mem_write(0x2200000,heights);w(0xbd5284,0x2200000)
   rows=[]
   for index in range(80):
    x,y=rng.randrange(1024)<<16,rng.randrange(1024)<<16
    z=heights[((y>>16)&1023)*1024+((x>>16)&1023)]*16384+rng.choice([-65536,-1,0,65536,0x400000])
    limit=rng.choice([0,4096,65536,131072,0x80000])
    delta=rng.choice([(0,0,-65536),(65536,0,0),(-46340,46340,-32768),(0,-65536,32768)])
    rows.append([limit,x,y,z,*delta])
   result=subprocess.run([str(args.probe.resolve()),'--terrain',str(args.game),entry.name],input=''.join(' '.join(map(str,row))+'\n' for row in rows),text=True,capture_output=True,check=True)
   native=list(map(int,result.stdout.split()));assert len(native)==len(rows)
   hit_count=0
   for index,(row,got) in enumerate(zip(rows,native)):
    for axis in range(3):w(0x2105000+axis*4,row[axis+1]);w(0x2105010+axis*4,row[axis+4])
    original.call(0x47e390,[0x2105000,0x2105010,row[0]])
    want=sg(u.reg_read(UC_X86_REG_EAX))
    if want==1:terrain_report['clear']+=1
    else:
     hit_count+=1;terrain_report['hits']+=1
     if want>0:terrain_report['positive_hits']+=1
     if want==0:terrain_report['zero_hits']+=1
    if got!=want:report['mismatches'].append({'mission':entry.name,'row':index,'input':row,'native':got,'original':want})
   terrain_report['cases']+=len(rows);terrain_report['missions'].append({'name':entry.name,'height_sha256':hashlib.sha256(heights).hexdigest(),'cases':len(rows),'hits':hit_count})
   print(entry.name,len(rows),'terrain cases',hit_count,'hits')
 report['terrain']=terrain_report
report['owner_slots']=[-1,0]
report['foreign_calls']=counts
if args.report:args.report.write_text(json.dumps(report,indent=2)+'\n')
print(report['cases'],'cases',len(report['mismatches']),'mismatches')
raise SystemExit(bool(report['mismatches']))
