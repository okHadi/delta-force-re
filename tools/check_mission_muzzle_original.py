#!/usr/bin/env python3
"""Compare mission muzzle bindings with the hash-bound original instructions."""
import argparse,hashlib,json,random,re,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import *
import check_model_instances_original as base
import check_animation_original as animation
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('executable',type=Path);p.add_argument('probe',type=Path)
p.add_argument('--items',required=True,type=Path);p.add_argument('--models',required=True,type=Path)
p.add_argument('--avatar',default='PLAYER01.3DI');p.add_argument('--chr',required=True,type=Path)
p.add_argument('--report',type=Path);args=p.parse_args()
o=base.Original(args.executable);u=o.unicorn
sg=lambda n:(n+2**31)%2**32-2**31
pack=lambda vs:struct.pack('<%dI'%len(vs),*[v&0xffffffff for v in vs])
def w(at,*vs):u.mem_write(at,pack(vs))
def r(at,n=1):return list(struct.unpack('<%dI'%n,u.mem_read(at,n*4)))
def run(lines):
 out=subprocess.run([str(args.probe.resolve()),str(args.items),str(args.models),args.avatar,str(args.chr)],input='\n'.join(lines)+'\n',capture_output=True,text=True)
 if out.returncode:raise ValueError(f"{out.stderr} after {len(out.stdout.splitlines())} rows; next {lines[len(out.stdout.splitlines()):len(out.stdout.splitlines())+1]}")
 return [line.split() for line in out.stdout.splitlines()]
meta=run(['models','bindings','attachment']);models={int(row[1]):row[2] for row in meta if row[0]=='M'}
bindings=[int(v) for row in meta if row[0]=='B' for v in row[1:]]
attachment=[int(v) for row in meta if row[0]=='T' for v in row[1:]]
# Read source ids independently of the native table. Original4828C0 performs each lookup.
blocks=re.findall(r'\bbegin\s+"[^"\r\n]*"(.*?)\bend\b',re.sub(r'//[^\n]*','',args.items.read_text()),re.S|re.I)
ids=[int(re.search(r'\bid\s+(-?\d+)',b,re.I)[1])-100000 for b in blocks]
ITEM=0x2280000;ACTOR=0x2270000;OUTPUT=0x2271000;CHARACTER=0x2272000;BONES=0x2273000;POSE=0x2274000;BONEBASE=0x2275000
w(0xc23810,ITEM);w(0xc23814,len(ids));w(0xad3ed0,0x4f9934)
for i,id in enumerate(ids):w(ITEM+i*0x13c+0x20,id)
# Name services are supplied. All item lookup and weapon +94/+98 stores execute.
name_calls={0x493a80:0,0x404cf0:0};u.mem_write(OUTPUT,b'\0')
def names(_u,at,size,data):
 name_calls[at]+=1;sp=u.reg_read(UC_X86_REG_ESP)
 if at==0x493a80:u.mem_write(r(sp+4)[0],b'\0');u.reg_write(UC_X86_REG_EAX,0)
 else:u.reg_write(UC_X86_REG_EAX,OUTPUT)
 u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,r(sp)[0])
name_hooks=[u.hook_add(unicorn.UC_HOOK_CODE,names,None,at,at) for at in name_calls]
o.call(0x46bfb0,[])
for handle in name_hooks:u.hook_del(handle)
if list(name_calls.values())!=[78,78]:raise ValueError('weapon initializer name-service count differs')
original_bindings=[]
for weapon in range(39):
 for field in (0x64,0x68):
  o.call(0x4828c0,[r(0x4f9934+weapon*0xa8+field)[0]])
  index=u.reg_read(UC_X86_REG_EAX);original_bindings.append(index)
  stored=r(0x4f9934+weapon*0xa8+(0x94 if field==0x64 else 0x98))[0]
  if stored!=index:raise ValueError('46BFB0 stored item index differs from lookup')
if bindings!=original_bindings:raise ValueError(f'runtime weapon item indices differ: native{bindings} original{original_bindings} ids{len(ids)}')
# Graphic names match source ITEMS.DEF, with the original .3DI resource suffix.
name_handles={name.lower():handle for handle,name in models.items()}
for i,b in enumerate(blocks):
 graphic=re.search(r'\bgraphic\s+(\S+)',b,re.I)
 if graphic:w(ITEM+i*0x13c+0x6c,name_handles.get(graphic[1].lower()+'.3di',0))
o.call(0x481af0,[])
if list(map(sg,r(0xc22b40,12)))!=attachment:raise ValueError('C22B40 attachment differs')
# Load actual first-LOD bytes and perform the loader's mesh-origin step.
heap=0x2100000;source_hashes={};hardpoint_counts={}
for handle,name in models.items():
 raw=(args.models/name).read_bytes();source_hashes[name]=hashlib.sha256(raw).hexdigest()
 wc=lambda at:struct.unpack_from('<I',raw,at)[0]
 at=128
 for _ in range(wc(124)):at+=36+wc(at+16)+1024
 header=raw[at:at+184];vc,nc,fc,mc,hc=[wc(at+k) for k in (128,136,144,152,160)];at+=184
 at+=vc*8+nc*8+fc*72;meshes=bytearray(raw[at:at+mc*112]);at+=mc*112
 hardpoint_counts[handle]=hc
 points=raw[at:at+hc*12];lod=heap;mesh=heap+0x100;hp=mesh+mc*112+0x100;heap=(hp+len(points)+0xfff)&~0xfff
 u.mem_write(handle,raw[:124]);u.mem_write(lod,header);w(handle+0x50,lod);w(lod+0x9c,mesh);w(lod+0xa4,hp)
 if struct.unpack_from('<I',header,16)[0]&1:
  for i in range(mc):
   parent=struct.unpack_from('<I',meshes,i*112+36)[0]
   for axis in range(3):
    own=struct.unpack_from('<i',meshes,i*112+52+axis*4)[0];ancestor=struct.unpack_from('<i',meshes,parent*112+52+axis*4)[0]
    struct.pack_into('<I',meshes,i*112+40+axis*4,(own-ancestor)&0xffffffff)
 u.mem_write(mesh,bytes(meshes))
 if points:u.mem_write(hp,points)
 tag=wc(40);callback={0x6f726730:0x481130,0x676e7263:0x480ff0}.get(tag)
 if not callback:raise ValueError(f'unsupported test avatar/model tag{name}:{tag:x}')
 w(handle+0x4c,callback)
avatar=name_handles[args.avatar.lower()]
raw=args.chr.read_bytes();bone_offsets=[];bone_at=struct.unpack_from('<I',raw,4)[0]
for _ in range(struct.unpack_from('<I',raw,8)[0]):
 bone_offsets.append(bone_at);bone_at+=92 if struct.unpack_from('<I',raw,bone_at+24)[0]&1 else 60
map_at,count=struct.unpack_from('<2I',raw,12)
parents=[struct.unpack_from('<i',raw,bone_offsets[struct.unpack_from('<I',raw,map_at+i*4)[0]]+56)[0] for i in range(count)]
a=animation.Original(args.executable);a.load_character(raw)
rng=random.Random(0x45e7a0);rows=[]
for i in range(156):
 weapon=i%39;placement=[rng.randrange(-30,30)*65536 for _ in range(3)]+[rng.getrandbits(32) for _ in range(3)]
 if i%2==0:
  rows.append(['normal',weapon,0,*placement,*[rng.randrange(-65536,65536) for _ in range(3)],rng.randrange(0,65536)])
 else:rows.append(['animated',weapon,4 if i%4==1 else 0,*placement,(i//39)*5,0,i%8])
excluded=[];accepted=[]
for row in rows:
 mode,weapon=row[:2];field=0x94 if mode=='normal' else 0x98
 definition=r(0x4f9934+weapon*0xa8+field)[0];handle=r(ITEM+definition*0x13c+0x6c)[0]
 point=r(0x4f9934+weapon*0xa8+0x90)[0]
 if handle and point>=hardpoint_counts[handle]:
  excluded.append({'path':mode,'weapon':weapon,'model':models[handle],'hardpoint':point,'stored_hardpoints':hardpoint_counts[handle]})
 else:accepted.append(row)
rows=accepted
outputs=run([' '.join(map(str,row)) for row in rows])
if len(outputs)!=len(rows):raise ValueError('native muzzle row count differs')
counts={'normal':0,'animated':0,'shots':0,'current_pose_words':0}
captured=[];events=[]
def hook(_u,at,size,data):
 if at==0x481130:events.append('matrix')
 if at==0x45ea00:
  sp=u.reg_read(UC_X86_REG_ESP);actor,shot=r(sp+4,2)
  if actor!=ACTOR:raise ValueError('shot actor differs')
  captured.extend(r(shot,6));events.append('shot')
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,r(sp)[0])
u.hook_add(unicorn.UC_HOOK_CODE,hook,None,0x481130,0x481130)
u.hook_add(unicorn.UC_HOOK_CODE,hook,None,0x45ea00,0x45ea00)
for i,(row,out) in enumerate(zip(rows,outputs)):
 mode,weapon,flags,*values=row;placement=values[:6];u.mem_write(ACTOR,bytes(0x114));w(ACTOR+0x24,*placement);w(ACTOR+0x94,weapon);w(ACTOR+0x6c,flags);w(ACTOR+0x10,avatar);w(ACTOR+0x18,CHARACTER)
 if mode=='normal':
  w(0xc23820,ACTOR);w(0xc239b8,0);w(0xc239c0,weapon);w(0xc239e8,*values[6:9]);w(0xc23a18,values[9])
  o.call(0x482d90,[]);o.call(0x4847d0,[OUTPUT,OUTPUT+4,OUTPUT+8])
  want=[*map(sg,r(OUTPUT,3)),*r(0xc23a80,3)]
  if list(map(int,out[1:]))!=want:raise ValueError(f'normal{i}: native{out} original{want}')
 else:
  slot,frame,ticks=values[6:];a.reset_instance();a.run_operation(f'select {slot} {frame}')
  for _ in range(ticks):a.run_operation(animation.tick_line(0,0,0,0,write_random=0))
  pose=a.state()['pose'];actual=list(map(int,out[1:]));fired,shots=actual[:2];pose_at=20 if fired else 8
  native_pose=actual[pose_at+1:]
  if actual[pose_at]!=count or native_pose!=[v for bone in pose for v in bone]:raise ValueError(f'current controller pose{i} differs')
  w(CHARACTER+0x14,count);w(CHARACTER+0x7c,BONES);w(CHARACTER+0x80,POSE)
  for b in range(count):w(BONES+b*4,BONEBASE+b*64);w(BONEBASE+b*64+56,parents[b]);w(POSE+b*24,*pose[b])
  captured.clear();events.clear();o.call(0x45e7a0,[ACTOR])
  expected=bool(captured)
  if fired!=expected or shots!=int(expected):raise ValueError('animated guard/call count differs')
  if expected:
   want=[*map(sg,captured[:3]),*captured[3:]]
   if actual[2:8]!=want:raise ValueError(f'animated{i}: native{actual[2:8]} original{want}')
   if actual[8:20]!=list(map(sg,r(0xc22d40,12))):raise ValueError('mutated slot-seven differs')
   if events!=['matrix','shot']:raise ValueError('callback order differs')
  if r(ACTOR+0x34)[0]!=placement[4]:raise ValueError('actor pitch not restored')
  counts['shots']+=int(expected);counts['current_pose_words']+=count*6
 counts[mode]+=1
report={'executable_sha256':o.digest,'cases':counts,'weapon_bindings':39,'initializer_name_service_calls':list(name_calls.values()),'attachment_words':12,'excluded_missing_hardpoints':excluded,'models':source_hashes,'character_sha256':hashlib.sha256(raw).hexdigest(),'differences':0,'scope':'Original muzzle and callback instructions, stock hardpoints, independently replayed current controller poses. Shot dispatch is captured.'}
if args.report:args.report.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
