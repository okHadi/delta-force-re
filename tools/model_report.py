#!/usr/bin/env python3
"""Inspect DF1 model geometry. Original data stays in the input archive."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import struct
from pff import Archive


class ModelError(ValueError):
    pass


def decode(data, geometry=False):
    def region(offset, size):
        if offset < 0 or size < 0 or offset > len(data) or size > len(data)-offset:
            raise ModelError(f'range outside model at 0x{offset:x}, size {size}')
        return data[offset:offset+size]
    def u32(offset):
        return struct.unpack('<I', region(offset, 4))[0]
    def name(offset):
        return region(offset, 12).split(b'\0', 1)[0].decode('ascii', errors='backslashreplace')
    region(0, 128)
    if data[:3] != b'3DI':
        raise ModelError('unsupported magic '+data[:4].hex())
    version = data[3]
    if version not in (2, 3, 4, 5):
        raise ModelError(f'unsupported version {version}')
    header_size, mesh_size = (184, 112) if version == 5 else (176, 88 if version == 2 else 104)
    result = dict(name=name(4), version=version, sha256=hashlib.sha256(data).hexdigest(), size=len(data), textures=[], lods=[])
    lod_count = u32(20)
    if lod_count > len(data)//header_size:
        raise ModelError('LOD count outside model')
    texture_count = u32(124)
    if texture_count > (len(data)-128)//1060:
        raise ModelError('texture count outside model')
    pos = 128
    for _ in range(texture_count):
        region(pos, 36)
        size = u32(pos+16)
        width, height = struct.unpack('<HH', region(pos+24, 4))
        region(pos+36, size+1024)
        result['textures'].append(dict(name=name(pos), offset=pos, width=width, height=height, sequence=struct.unpack('<H', region(pos+20, 2))[0], flags=data[pos+22], pixels_offset=pos+36, pixels_size=size, palette_offset=pos+36+size))
        pos += 36+size+1024
    for lod in range(lod_count):
        start = pos
        region(start, header_size)
        counts = [u32(start+o) for o in (128, 136, 144, 152, 160, 168)]
        if version == 5:
            counts.append(u32(start+176))
        else:
            counts.append(0)
        vc,nc,fc,mc,hc,ac,cc = counts
        sizes = (8,8,72,mesh_size,12,72,76)
        # Volumes precede materials in the stored block.
        ordered_counts = (vc,nc,fc,mc,hc,cc,ac)
        total = sum(a*b for a,b in zip(ordered_counts,sizes))
        declared = u32(start+20)
        if declared != total:
            raise ModelError(f'LOD {lod}: declared size {declared}, calculated {total}')
        pos += header_size
        region(pos,total)
        offsets=[]
        for count,stride in zip(ordered_counts,sizes):
            offsets.append(pos);pos+=count*stride
        vo,no,fo,mo,ho,co,ao=offsets
        vertices=[struct.unpack_from('<hhhH',data,vo+i*8) for i in range(vc)]
        normals=[struct.unpack_from('<hhhh',data,no+i*8) for i in range(nc)]
        meshes=[];faces=[];vb=nb=fb=0
        for i in range(mc):
            p=mo+i*mesh_size
            v,f,n=(u32(p+o) for o in (4,12,20))
            if v>vc-vb or f>fc-fb or n>nc-nb:
                raise ModelError(f'LOD {lod}: mesh {i} count outside geometry')
            mesh=dict(offset=p, vertex_start=vb,vertex_count=v,normal_start=nb,normal_count=n,face_start=fb,face_count=f)
            if version==5:
                mesh.update(parent_index=u32(p+36),delta_origin=list(struct.unpack_from('<iii',data,p+40)),origin=list(struct.unpack_from('<iii',data,p+52)))
            meshes.append(mesh)
            for j in range(f):
                p=fo+(fb+j)*72
                vi=struct.unpack_from('<hhh',data,p+28)
                ni=struct.unpack_from('<hhh',data,p+34)
                if any(x<0 or x>=v for x in vi):
                    raise ModelError(f'LOD {lod}: face {fb+j} vertex outside mesh')
                if any(x<0 or x>=n for x in ni):
                    raise ModelError(f'LOD {lod}: face {fb+j} normal outside mesh')
                material=u32(p+68)
                if material>=ac:
                    raise ModelError(f'LOD {lod}: face {fb+j} material outside table')
                faces.append(dict(vertices=[x+vb for x in vi], normals=[x+nb for x in ni], material=material, flags=struct.unpack_from('<H',data,p)[0], u=list(struct.unpack_from('<iii',data,p+4)),v=list(struct.unpack_from('<iii',data,p+16))))
            vb+=v;nb+=n;fb+=f
        if (vb,nb,fb)!=(vc,nc,fc):
            raise ModelError(f'LOD {lod}: mesh totals differ from geometry')
        materials=[]
        for i in range(ac):
            p=ao+i*76
            flags=u32(p+16);key=data[p+(55 if flags&0x8000 and not flags&1 else 52)]
            candidates=[j for j,t in enumerate(result['textures']) if t['sequence']&255==key] if flags&0x8001 and key!=255 else []
            if flags&0x4000 and data[p+30]>ac-i:
                raise ModelError(f'LOD {lod}: material animation outside table')
            materials.append(dict(offset=p,name=name(p),flags=flags,texture_key=key,texture_candidates=candidates,frame_count=data[p+30],animation_parameter=data[p+31],flat_rgb=list(data[p+40:p+43])))
        item=dict(flags=u32(start+16),offset=start,data_offset=start+header_size,data_size=total,vertices_count=vc,normals_count=nc,faces_count=fc,meshes=meshes,hardpoints=[list(struct.unpack_from('<iii',data,ho+i*12)) for i in range(hc)],collision_count=cc,collision_offset=co,materials=materials,bounds_raw=[[min(v[k] for v in vertices),max(v[k] for v in vertices)] for k in range(3)] if vertices else None)
        if geometry:
            item.update(vertices=vertices,normals=normals,faces=faces)
        result['lods'].append(item)
    if pos!=len(data):
        raise ModelError(f'trailing data: {len(data)-pos} bytes')
    result.update(decoded_end=pos,trailing_bytes=0)
    return result


def report_archive(path):
    archive=Archive(path);models=[]
    with archive.path.open('rb') as source:
        for entry in archive.entries:
            if entry.deleted or not entry.name.lower().endswith('.3di'):
                continue
            source.seek(entry.offset);data=source.read(entry.size)
            item=dict(member=entry.name,archive_offset=entry.offset,size=entry.size,sha256=hashlib.sha256(data).hexdigest())
            try:
                item.update(status='decoded',model=decode(data))
            except ModelError as error:
                item.update(status='unsupported',error=str(error))
            models.append(item)
    return dict(archive=str(path),counts=dict(collections.Counter(m['status'] for m in models)),models=models)


def check_origins(archive_path, executable, probe):
    # Run only the bounded original origin block. This does not launch the game.
    import subprocess
    import tempfile
    import pefile
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
    from unicorn.x86_const import UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EFLAGS, UC_X86_REG_EIP
    exe=Path(executable).read_bytes()
    source_hash='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
    if hashlib.sha256(exe).hexdigest()!=source_hash:
        raise ModelError('expected the documented original executable SHA-256')
    pe=pefile.PE(data=exe)
    begin,end=0x486128,0x486203
    code=pe.get_data(begin-pe.OPTIONAL_HEADER.ImageBase,end-begin)
    machine=Uc(UC_ARCH_X86,UC_MODE_32)
    machine.mem_map(0x486000,4096);machine.mem_write(begin,code)
    machine.mem_map(0x2000000,4096);machine.mem_map(0x3000000,0x200000)
    archive=Archive(archive_path);models=lods=vertices=meshes=0
    with archive.path.open('rb') as source, tempfile.TemporaryDirectory(prefix='df-model-',dir=Path('.local')) as temporary:
        for entry in archive.entries:
            if entry.deleted or not entry.name.lower().endswith('.3di'):
                continue
            source.seek(entry.offset);data=source.read(entry.size)
            try:
                model=decode(data)
            except ModelError:
                continue
            if model['version']!=5 or not any(lod['flags']&1 for lod in model['lods']):
                continue
            path=Path(temporary)/entry.name;path.write_bytes(data)
            native=json.loads(subprocess.check_output([str(Path(probe).resolve()),'--origins-json',str(path)]))
            models+=1
            for index,lod in enumerate(model['lods']):
                if not lod['flags']&1:
                    continue
                header=bytearray(data[lod['offset']:lod['data_offset']])
                vc=lod['vertices_count'];mc=len(lod['meshes'])
                vertex_data=data[lod['data_offset']:lod['data_offset']+vc*8]
                mesh_data=b''.join(data[m['offset']:m['offset']+112] for m in lod['meshes'])
                if len(vertex_data)>0xff000 or len(mesh_data)>0x100000:
                    raise ModelError('origin test exceeds its mapped memory')
                struct.pack_into('<I',header,0x84,0x3001000)
                struct.pack_into('<I',header,0x9c,0x3100000)
                machine.mem_write(0x3000000,bytes(header));machine.mem_write(0x3001000,vertex_data);machine.mem_write(0x3100000,mesh_data)
                machine.reg_write(UC_X86_REG_EBX,0x3000000);machine.reg_write(UC_X86_REG_ESI,0);machine.reg_write(UC_X86_REG_ESP,0x2000800);machine.reg_write(UC_X86_REG_EFLAGS,2)
                machine.emu_start(begin,end,timeout=1000000,count=1000000)
                if machine.reg_read(UC_X86_REG_EIP)!=end:
                    raise ModelError('original origin block exceeded instruction limit')
                actual_v=bytes(machine.mem_read(0x3001000,len(vertex_data)))
                actual_m=bytes(machine.mem_read(0x3100000,len(mesh_data)))
                expected=dict(vertices=[list(struct.unpack_from('<hhh',actual_v,i*8)) for i in range(vc)],delta_origin=[list(struct.unpack_from('<iii',actual_m,i*112+40)) for i in range(mc)])
                if native[index]!=expected:
                    raise ModelError(f'origin mismatch: {entry.name} LOD {index}')
                lods+=1;vertices+=vc;meshes+=mc
    return dict(source_sha256=source_hash,block_start=hex(begin),block_end_exclusive=hex(end),block_sha256=hashlib.sha256(code).hexdigest(),models=models,lods=lods,vertices=vertices,meshes=meshes,mismatches=0,scope='v5 LODs with flags bit 0; original loader global skip flag is zero')


def check_character(path, executable, probe):
    import subprocess
    import pefile
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EFLAGS, UC_X86_REG_EIP
    exe=Path(executable).read_bytes()
    source_hash='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
    if hashlib.sha256(exe).hexdigest()!=source_hash:
        raise ModelError('expected the documented original executable SHA-256')
    data=Path(path).read_bytes()
    native=json.loads(subprocess.check_output([str(Path(probe).resolve()),'--character-poses-json',str(path)]))
    pe=pefile.PE(data=exe);start,end=0x43a0e0,0x43a145
    code=pe.get_data(start-pe.OPTIONAL_HEADER.ImageBase,end-start)
    machine=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    machine.mem_map(start&~4095,4096);machine.mem_write(start,code)
    base=0x3000000
    if len(data)>0x800000:
        raise ModelError('character check exceeds its mapped range')
    machine.mem_map(base,(len(data)+4095)&~4095);machine.mem_write(base,data)
    for address in (0x2000000,0x4000000,0x5000000,0x6000000):
        machine.mem_map(address,4096)
    table=struct.unpack_from('<I',data,20)[0]
    machine.mem_write(base+20,struct.pack('<I',base+table))
    for i in range(255):
        pointer=struct.unpack_from('<I',data,table+i*4)[0]
        if pointer:
            machine.mem_write(base+table+i*4,struct.pack('<I',base+pointer))
    machine.mem_write(0x4000000,struct.pack('<I',base))
    machine.mem_write(0x4000058,struct.pack('<I',52))
    machine.mem_write(0x4000080,struct.pack('<I',0x5000000))
    bones=0
    for item in native:
        if len(item['pose'])>4096//24:
            raise ModelError('character bone count exceeds test output buffer')
        machine.reg_write(UC_X86_REG_EAX,item['slot']);machine.reg_write(UC_X86_REG_EBX,item['frame'])
        machine.reg_write(UC_X86_REG_ESI,0x4000000);machine.reg_write(UC_X86_REG_ESP,0x2000800);machine.reg_write(UC_X86_REG_EFLAGS,2)
        machine.mem_write(0x2000800,struct.pack('<I',0x6000000))
        machine.emu_start(start,0x6000000,count=100000,timeout=1000000)
        if machine.reg_read(UC_X86_REG_EIP)!=0x6000000:
            raise ModelError('original character pose read exceeded instruction limit')
        raw=bytes(machine.mem_read(0x5000000,len(item['pose'])*24))
        expected=[list(struct.unpack_from('<6i',raw,i*24)) for i in range(len(item['pose']))]
        if item['pose']!=expected:
            raise ModelError(f"character pose mismatch: slot {item['slot']} frame {item['frame']}")
        bones+=len(item['pose'])
    return dict(source_sha256=source_hash,character_sha256=hashlib.sha256(data).hexdigest(),function=hex(start),function_bytes=len(code),function_sha256=hashlib.sha256(code).hexdigest(),frames=len(native),bone_poses=bones,mismatches=0,scope='original frame angle and unit-scale read; interpolation and motion are separate')


def check_character_deltas(path, executable, probe):
    import subprocess
    import pefile
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBX, UC_X86_REG_ESP, UC_X86_REG_EFLAGS, UC_X86_REG_EIP
    checked=check_character(path,executable,probe)
    exe=Path(executable).read_bytes();data=Path(path).read_bytes()
    probe=str(Path(probe).resolve())
    native=json.loads(subprocess.check_output([probe,'--character-deltas-json',str(path)]))
    poses=json.loads(subprocess.check_output([probe,'--character-poses-json',str(path)]))
    by_key={(row['slot'],row['frame']):row['pose'] for row in poses}
    pe=pefile.PE(data=exe);machine=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    machine.mem_map(0x43a000,4096);machine.mem_write(0x43a000,pe.get_data(0x43a000-pe.OPTIONAL_HEADER.ImageBase,4096))
    machine.mem_map(0x439000,4096)
    base=0x3000000
    machine.mem_map(base,(len(data)+4095)&~4095);machine.mem_write(base,data)
    for address in (0x2000000,0x4000000,0x5000000):
        machine.mem_map(address,4096)
    def word(offset):
        return struct.unpack_from('<I',data,offset)[0]
    slots={};table=word(20)
    for slot in range(255):
        pos=word(table+slot*4)
        if not pos:
            continue
        count=word(pos);pos+=20;slots[slot]=[]
        for _ in range(count):
            slots[slot].append(pos);pos+=word(pos)
    total=0
    for item in native:
        slot,frame=item['slot'],item['frame'];offset=slots[slot][frame]
        if word(offset)&0x80000000 or word(offset+4)&0x80 or word(offset+40):
            raise ModelError('delta check covers frames without head overrides or scale blocks')
        current=by_key[(slot,frame-1 if frame else len(slots[slot])-1)]
        if len(current)>1024//24:
            raise ModelError('delta check bone count exceeds output buffer')
        state=bytearray(256)
        for field,value in ((0x14,len(current)),(0x58,52),(0x80,0x5000000),(0x84,0x5000400)):
            struct.pack_into('<I',state,field,value)
        machine.mem_write(0x4000000,bytes(state))
        machine.mem_write(0x5000000,b''.join(struct.pack('<6i',*pose) for pose in current))
        machine.reg_write(UC_X86_REG_ESI,0x4000000);machine.reg_write(UC_X86_REG_EDI,base+offset)
        machine.reg_write(UC_X86_REG_EBX,item['duration']);machine.reg_write(UC_X86_REG_ESP,0x2000800);machine.reg_write(UC_X86_REG_EFLAGS,2)
        machine.emu_start(0x43a2c7,0x43a45b,count=100000,timeout=1000000)
        if machine.reg_read(UC_X86_REG_EIP)!=0x43a45b:
            raise ModelError('original character delta block exceeded instruction limit')
        raw=bytes(machine.mem_read(0x5000400,len(current)*24))
        expected=[list(struct.unpack_from('<6i',raw,i*24)) for i in range(len(current))]
        if item['delta']!=expected:
            raise ModelError(f'character delta mismatch: slot {slot} frame {frame}')
        machine.reg_write(UC_X86_REG_ESI,0x4000000)
        machine.emu_start(0x43a7ea,0x43a848,count=100000,timeout=1000000)
        if machine.reg_read(UC_X86_REG_EIP)!=0x43a848:
            raise ModelError('original character advance block exceeded instruction limit')
        raw=bytes(machine.mem_read(0x5000000,len(current)*24))
        expected=[list(struct.unpack_from('<6i',raw,i*24)) for i in range(len(current))]
        if item['advanced']!=expected:
            raise ModelError(f'character advance mismatch: slot {slot} frame {frame}')
        total+=len(current)
    return dict(source_sha256=checked['source_sha256'],character_sha256=checked['character_sha256'],frames=len(native),bone_deltas=total,bone_advances=total,mismatches=0,delta_block_start='0x43a2c7',delta_block_end_exclusive='0x43a45b',delta_block_sha256=hashlib.sha256(pe.get_data(0x43a2c7-pe.OPTIONAL_HEADER.ImageBase,0x43a45b-0x43a2c7)).hexdigest(),advance_block_start='0x43a7ea',advance_block_end_exclusive='0x43a848',advance_block_sha256=hashlib.sha256(pe.get_data(0x43a7ea-pe.OPTIONAL_HEADER.ImageBase,0x43a848-0x43a7ea)).hexdigest(),scope='core per-bone deltas and one tick; unit scale; prior stored pose; frame head overrides absent')


def check_character_prepare(path, executable, probe):
    import subprocess
    import pefile
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EFLAGS, UC_X86_REG_EIP
    exe=Path(executable).read_bytes();data=Path(path).read_bytes()
    source_hash='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
    if hashlib.sha256(exe).hexdigest()!=source_hash:
        raise ModelError('expected the documented original executable SHA-256')
    if len(data)>0x800000:
        raise ModelError('character preparation exceeds its mapped range')
    native=subprocess.check_output([str(Path(probe).resolve()),'--character-prepared',str(path)])
    pe=pefile.PE(data=exe);start,end=0x42c9a0,0x42cbad
    code=pe.get_data(start-pe.OPTIONAL_HEADER.ImageBase,end-start)
    machine=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    machine.mem_map(0x42c000,4096);machine.mem_write(start,code)
    base=0x3000000
    machine.mem_map(base,(len(data)+4095)&~4095);machine.mem_write(base,data)
    for address in (0x2000000,0x6ba000,0x6000000):
        machine.mem_map(address,4096)
    def word(offset):
        return struct.unpack_from('<I',data,offset)[0]
    bones=[];pos=word(4);keys=0
    machine.mem_write(base+4,struct.pack('<I',base+pos))
    for _ in range(word(8)):
        if not word(pos+24)&1:
            raise ModelError('original preparation check requires 92-byte bones')
        a,b,count=word(pos),word(pos+4),word(pos+16)
        keys+=word(pos+12);bones.append(pos)
        for field in (0,4):
            machine.mem_write(base+pos+field,struct.pack('<I',base+word(pos+field)))
        for i in range(count):
            for field in (0,4,8):
                offset=b+i*88+field
                machine.mem_write(base+offset,struct.pack('<I',base+word(offset)))
        pos+=92
    machine.mem_write(0x6ba4a0,struct.pack('<I',base))
    machine.mem_write(0x2000800,struct.pack('<II',0x6000000,0))
    machine.reg_write(UC_X86_REG_ESP,0x2000800);machine.reg_write(UC_X86_REG_EFLAGS,2)
    machine.emu_start(start,0x6000000,count=2000000,timeout=1000000)
    if machine.reg_read(UC_X86_REG_EIP)!=0x6000000:
        raise ModelError('original character preparation exceeded instruction limit')
    expected=bytearray(machine.mem_read(base,len(data)))
    struct.pack_into('<I',expected,4,word(4))
    for pos in bones:
        for field in (0,4):
            struct.pack_into('<I',expected,pos+field,word(pos+field))
    if native!=expected:
        raise ModelError('character preparation differs from original bytes')
    return dict(source_sha256=source_hash,character_sha256=hashlib.sha256(data).hexdigest(),function=hex(start),function_bytes=len(code),function_sha256=hashlib.sha256(code).hexdigest(),bones=len(bones),track_a_keys=keys,compared_bytes=len(data),mismatches=0,scope='original 92-byte-bone preparation; process addresses normalized to stored offsets')


def check_character_matrices(path, executable, probe):
    import subprocess
    import pefile
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EFLAGS, UC_X86_REG_EIP, UC_X86_REG_FPCW
    pose_check=check_character(path,executable,probe)
    check_character_prepare(path,executable,probe)
    probe=str(Path(probe).resolve());exe=Path(executable).read_bytes()
    raw=subprocess.check_output([probe,'--character-prepared',str(path)])
    stored=json.loads(subprocess.check_output([probe,'--character-matrices-json',str(path)]))
    cases=json.loads(subprocess.check_output([probe,'--character-matrix-cases-json',str(path)]))
    poses=json.loads(subprocess.check_output([probe,'--character-poses-json',str(path)]))
    by_key={(row['slot'],row['frame']):row['pose'] for row in poses}
    pe=pefile.PE(data=exe);machine=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    for address,size in ((0x45c000,0x2000),(0x46b000,4096),(0x477000,4096),(0x49b000,4096),(0x4cd000,4096),(0x4fb000,4096),(0xbd0000,0x4000),(0x2000000,4096),(0x3000000,(len(raw)+4095)&~4095),(0x4000000,0x3000),(0x5000000,4096),(0x6000000,4096)):
        machine.mem_map(address,size)
    for address,size in ((0x45c000,0x2000),(0x46bc00,0x205),(0x477510,0x48),(0x4776c0,0xa4),(0x49bae6,29),(0x4cd518,16),(0x4fb2f8,4)):
        machine.mem_write(address,pe.get_data(address-pe.OPTIONAL_HEADER.ImageBase,size))
    machine.mem_write(0x2000800,struct.pack('<I',0x6000000));machine.reg_write(UC_X86_REG_ESP,0x2000800);machine.reg_write(UC_X86_REG_FPCW,0x037f)
    machine.emu_start(0x477510,0x6000000,count=1000000,timeout=1000000)
    if machine.reg_read(UC_X86_REG_EIP)!=0x6000000:
        raise ModelError('original sine initialization exceeded instruction limit')
    machine.mem_write(0x3000000,raw)
    count,bones,mapping=(struct.unpack_from('<I',raw,offset)[0] for offset in (16,4,12))
    if count>32:
        raise ModelError('matrix check bone count exceeds its output buffer')
    for i in range(count):
        index=struct.unpack_from('<I',raw,mapping+i*4)[0]
        machine.mem_write(0x4001000+i*4,struct.pack('<I',0x3000000+bones+index*92))
    state=bytearray(256)
    for offset,value in ((0x14,count),(0x7c,0x4001000),(0x80,0x4002000)):
        struct.pack_into('<I',state,offset,value)
    machine.mem_write(0x4000000,bytes(state));total=0
    for item in stored+cases:
        pose=item.get('pose',by_key[(item['slot'],item['frame'])])
        machine.mem_write(0x4002000,b''.join(struct.pack('<6i',*bone) for bone in pose))
        machine.mem_write(0x5000000,b'\xa5'*(count*64))
        world=item.get('world',[]);world_pointer=0x5000800 if world else 0
        if world:
            machine.mem_write(world_pointer,struct.pack('<12i',*world))
        machine.mem_write(0x2000800,struct.pack('<5I',0x6000000,world_pointer,0x5000000,0x4000000,item.get('pitch',0)))
        machine.reg_write(UC_X86_REG_ESP,0x2000800);machine.reg_write(UC_X86_REG_EFLAGS,2)
        machine.emu_start(0x46bc00,0x6000000,count=1000000,timeout=1000000)
        if machine.reg_read(UC_X86_REG_EIP)!=0x6000000:
            raise ModelError('original bone matrices exceeded instruction limit')
        output=bytes(machine.mem_read(0x5000000,count*64))
        expected=[list(struct.unpack_from('<12i',output,i*64)) for i in range(count)]
        if item['matrices']!=expected:
            raise ModelError(f"bone matrix mismatch: slot {item['slot']} frame {item['frame']}")
        total+=count
    return dict(source_sha256=pose_check['source_sha256'],character_sha256=pose_check['character_sha256'],stored_frames=len(stored),extra_cases=len(cases),matrices=total,components=total*12,mismatches=0,function='0x46bc00',function_bytes=0x205,function_sha256=hashlib.sha256(pe.get_data(0x46bc00-pe.OPTIONAL_HEADER.ImageBase,0x205)).hexdigest(),sine_table_sha256=hashlib.sha256(bytes(machine.mem_read(0xbd0068,2561*4))).hexdigest(),scope='all stored poses; extra fractional angles, six pitch values and world transforms; prepared fields; matrix padding excluded')


def check_character_mesh_matrices(path, executable, probe, archive_path):
    import subprocess
    import tempfile
    import pefile
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EFLAGS, UC_X86_REG_EIP, UC_X86_REG_FPCW, UC_X86_REG_EBX, UC_X86_REG_ESI
    pose_check=check_character(path,executable,probe);check_character_prepare(path,executable,probe)
    probe=str(Path(probe).resolve());exe=Path(executable).read_bytes()
    raw=subprocess.check_output([probe,'--character-prepared',str(path)])
    poses=json.loads(subprocess.check_output([probe,'--character-poses-json',str(path)]))
    by_key={(row['slot'],row['frame']):row['pose'] for row in poses}
    pe=pefile.PE(data=exe);machine=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    for address,size in ((0x45c000,0x2000),(0x481000,4096),(0x486000,4096),(0x477000,4096),(0x49b000,4096),(0x4cd000,4096),(0x4fb000,4096),(0xbd0000,0x4000),(0xc22000,4096),(0x2000000,4096),(0x3000000,(len(raw)+4095)&~4095),(0x4000000,0x3000),(0x6000000,4096),(0x7000000,0x20000)):
        machine.mem_map(address,size)
    for address,size in ((0x45c000,0x2000),(0x481130,0x1f7),(0x486128,0xdb),(0x477510,0x48),(0x4776c0,0xa4),(0x49bae6,29),(0x4cd518,16),(0x4fb2f8,4)):
        machine.mem_write(address,pe.get_data(address-pe.OPTIONAL_HEADER.ImageBase,size))
    machine.mem_write(0x2000800,struct.pack('<I',0x6000000));machine.reg_write(UC_X86_REG_ESP,0x2000800);machine.reg_write(UC_X86_REG_FPCW,0x037f)
    machine.emu_start(0x477510,0x6000000,count=1000000,timeout=1000000)
    if machine.reg_read(UC_X86_REG_EIP)!=0x6000000:
        raise ModelError('original sine initialization exceeded instruction limit')
    machine.mem_write(0x3000000,raw)
    count,bones,mapping=(struct.unpack_from('<I',raw,offset)[0] for offset in (16,4,12))
    if count>32:
        raise ModelError('mesh check bone count exceeds output buffer')
    for i in range(count):
        index=struct.unpack_from('<I',raw,mapping+i*4)[0]
        machine.mem_write(0x4001000+i*4,struct.pack('<I',0x3000000+bones+index*92))
    state=bytearray(256)
    for offset,value in ((0x14,count),(0x7c,0x4001000),(0x80,0x4002000)):
        struct.pack_into('<I',state,offset,value)
    machine.mem_write(0x4000000,bytes(state));machine.mem_write(0x4002818,struct.pack('<I',0x4000000))
    model_count=lod_count=case_count=matrix_count=0;archive=Archive(archive_path)
    local=Path(__file__).resolve().parent.parent/'.local';local.mkdir(exist_ok=True)
    with archive.path.open('rb') as source,tempfile.TemporaryDirectory(prefix='df-mesh-',dir=local) as temporary:
        for entry in archive.entries:
            if entry.deleted or not entry.name.lower().endswith('.3di'):
                continue
            source.seek(entry.offset);data=source.read(entry.size)
            if data[:4]!=b'3DI\x05' or data[40:44]!=b'0gro':
                continue
            model=decode(data);model_count+=1
            model_path=Path(temporary)/entry.name;model_path.write_bytes(data)
            native=json.loads(subprocess.check_output([probe,'--character-mesh-matrices-json',str(path),str(model_path)]))
            by_lod={}
            for row in native:
                by_lod.setdefault(row['lod'],[]).append(row)
            for index,lod in enumerate(model['lods']):
                header=bytearray(data[lod['offset']:lod['data_offset']])
                vertices=data[lod['data_offset']:lod['data_offset']+lod['vertices_count']*8]
                meshes=b''.join(data[m['offset']:m['offset']+112] for m in lod['meshes'])
                if len(vertices)>0xf000 or len(meshes)>0x10000:
                    raise ModelError('mesh matrix check exceeds mapped geometry range')
                struct.pack_into('<I',header,0x84,0x7001000);struct.pack_into('<I',header,0x9c,0x7010000)
                machine.mem_write(0x7000000,bytes(header));machine.mem_write(0x7001000,vertices);machine.mem_write(0x7010000,meshes)
                if lod['flags']&1:
                    machine.reg_write(UC_X86_REG_EBX,0x7000000);machine.reg_write(UC_X86_REG_ESI,0);machine.reg_write(UC_X86_REG_ESP,0x2000800)
                    machine.emu_start(0x486128,0x486203,count=1000000,timeout=1000000)
                    if machine.reg_read(UC_X86_REG_EIP)!=0x486203:
                        raise ModelError('original mesh origin block exceeded instruction limit')
                lod_count+=1
                for row in by_lod[index]:
                    pose=by_key[(row['slot'],row['frame'])]
                    machine.mem_write(0x4002000,b''.join(struct.pack('<6i',*p) for p in pose))
                    machine.mem_write(0x2000800,struct.pack('<4I',0x6000000,0x7000000,0x4002800,0))
                    machine.reg_write(UC_X86_REG_ESP,0x2000800);machine.reg_write(UC_X86_REG_EFLAGS,2)
                    machine.emu_start(0x481130,0x6000000,count=1000000,timeout=1000000)
                    if machine.reg_read(UC_X86_REG_EIP)!=0x6000000:
                        raise ModelError('original mesh matrices exceeded instruction limit')
                    output=bytes(machine.mem_read(0xc22b80,count*64))
                    expected=[list(struct.unpack_from('<12i',output,i*64)) for i in range(count)]
                    if row['matrices']!=expected:
                        raise ModelError(f'mesh matrix mismatch: {entry.name} LOD {index}')
                    case_count+=1;matrix_count+=count
    return dict(source_sha256=pose_check['source_sha256'],character_sha256=pose_check['character_sha256'],archive_sha256=hashlib.sha256(Path(archive_path).read_bytes()).hexdigest(),models=model_count,lods=lod_count,cases=case_count,matrices=matrix_count,components=matrix_count*12,mismatches=0,function='0x481130',function_bytes=0x1f7,function_sha256=hashlib.sha256(pe.get_data(0x481130-pe.OPTIONAL_HEADER.ImageBase,0x1f7)).hexdigest(),scope='all v5 primary org0 models; three sampled poses per LOD; original loader origin step; pitch zero; vertex projection separate')


def check_model_vertices(archive_path, executable, probe):
    import subprocess
    import tempfile
    import pefile
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EFLAGS, UC_X86_REG_EBX, UC_X86_REG_ESI
    exe=Path(executable).read_bytes();pe=pefile.PE(data=exe)
    machine=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    for address,size in ((0x45c000,0x2000),(0x469000,0x2000),(0x486000,4096),(0x8cf000,4096),(0x8d1000,4096),(0xaaa000,0x20000),(0xad3000,4096),(0x2000000,0x40000),(0x6000000,4096),(0x7000000,0x20000)):
        machine.mem_map(address,size)
    for address,size in ((0x45c000,0x2000),(0x469dc0,0x34a),(0x486128,0xdb)):
        machine.mem_write(address,pe.get_data(address-pe.OPTIONAL_HEADER.ImageBase,size))
    machine.mem_write(0x8cfa74,struct.pack('<3i',320,320,240));machine.mem_write(0x8d12ac,struct.pack('<i',65536))
    def call(address,args):
        machine.mem_write(0x2000800,struct.pack('<'+'I'*(len(args)+1),0x6000000,*args))
        machine.reg_write(UC_X86_REG_ESP,0x2000800);machine.reg_write(UC_X86_REG_EFLAGS,2)
        machine.emu_start(address,0x6000000,count=1000000,timeout=1000000)
        if machine.reg_read(UC_X86_REG_EIP)!=0x6000000:
            raise ModelError(f'original vertex function {address:x} exceeded instruction limit')
    archive=Archive(archive_path);probe=str(Path(probe).resolve());excluded=[]
    model_count=lod_count=mesh_cases=vertex_cases=projected=0
    local=Path(__file__).resolve().parent.parent/'.local';local.mkdir(exist_ok=True)
    with archive.path.open('rb') as source,tempfile.TemporaryDirectory(prefix='df-vertices-',dir=local) as temporary:
        for entry in archive.entries:
            if entry.deleted or not entry.name.lower().endswith('.3di'):
                continue
            source.seek(entry.offset);data=source.read(entry.size)
            if data[:4]!=b'3DI\x05':
                excluded.append(dict(name=entry.name,reason='not v5'));continue
            try:
                model=decode(data)
            except ModelError as error:
                excluded.append(dict(name=entry.name,reason=str(error)));continue
            path=Path(temporary)/entry.name;path.write_bytes(data)
            native=json.loads(subprocess.check_output([probe,'--mesh-vertices-json',str(path)]));model_count+=1
            if len(native)!=len(model['lods']):
                raise ModelError('native vertex LOD count differs from source')
            for row,lod in zip(native,model['lods']):
                header=bytearray(data[lod['offset']:lod['data_offset']]);vertices=data[lod['data_offset']:lod['data_offset']+lod['vertices_count']*8]
                meshes=b''.join(data[m['offset']:m['offset']+112] for m in lod['meshes'])
                if len(vertices)>0xf000 or len(meshes)>0x10000 or len(row['matrices'])>200:
                    raise ModelError('vertex check exceeds mapped buffers')
                struct.pack_into('<I',header,0x84,0x7001000);struct.pack_into('<I',header,0x9c,0x7010000)
                machine.mem_write(0x7000000,bytes(header));machine.mem_write(0x7001000,vertices);machine.mem_write(0x7010000,meshes)
                if lod['flags']&1:
                    machine.reg_write(UC_X86_REG_EBX,0x7000000);machine.reg_write(UC_X86_REG_ESI,0);machine.reg_write(UC_X86_REG_ESP,0x2000800)
                    machine.emu_start(0x486128,0x486203,count=1000000,timeout=1000000)
                    if machine.reg_read(UC_X86_REG_EIP)!=0x486203:
                        raise ModelError('original vertex origin block exceeded instruction limit')
                machine.mem_write(0x2001000,b''.join(struct.pack('<12i16x',*m) for m in row['matrices']))
                if len(row['meshes'])!=len(lod['meshes'])*2:
                    raise ModelError('native vertex mesh case count differs from source')
                lod_count+=1
                for item in row['meshes']:
                    mesh=lod['meshes'][item['index']];count=mesh['vertex_count']
                    # Mesh offsets in the report are absolute; vertex ranges are cumulative.
                    vertex_start=mesh['vertex_start']
                    flags=struct.unpack_from('<I',data,mesh['offset'])[0]
                    machine.mem_write(0xad3d2c,struct.pack('<I',item['mode']))
                    machine.mem_write(0xaaacb0,bytes(count*32))
                    matrix=0x2001000 if flags&1 else 0x2001000+item['index']*64
                    call(0x469fc0 if flags&1 else 0x469dc0,[matrix,count,0x7001000+vertex_start*8])
                    call(0x46a040,[])
                    output=bytes(machine.mem_read(0xaaacb0,count*32));expected=[]
                    for index in range(count):
                        point=struct.unpack_from('<3i',output,index*32);screen=struct.unpack_from('<2i',output,index*32+24);visible=int(point[0]>=65536)
                        expected.append([*point,*screen,visible]);projected+=visible
                    if item['vertices']!=expected:
                        raise ModelError(f'vertex/projection mismatch: {entry.name} LOD {row["lod"]} mesh {item["index"]} mode {item["mode"]}')
                    mesh_cases+=1;vertex_cases+=count
    return dict(source_sha256=hashlib.sha256(exe).hexdigest(),archive_sha256=hashlib.sha256(Path(archive_path).read_bytes()).hexdigest(),models=model_count,lods=lod_count,mesh_cases=mesh_cases,vertex_cases=vertex_cases,projected=projected,mismatches=0,functions=[dict(address=hex(a),bytes=n,sha256=hashlib.sha256(pe.get_data(a-pe.OPTIONAL_HEADER.ImageBase,n)).hexdigest()) for a,n in ((0x469dc0,0x1fc),(0x469fc0,0x73),(0x46a040,0xca))],excluded=excluded,scope='all bounded v5 models; generated matrices; scalar and MMX; owner path; focal 320 centre 320,240 near 65536; original origin step')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input');parser.add_argument('--archive',action='store_true');parser.add_argument('--geometry',action='store_true')
    checks=parser.add_mutually_exclusive_group()
    for name in ('origins','character','deltas','prepare','matrices','mesh-matrices','vertices'):
        checks.add_argument('--check-'+name,metavar='EXE')
    parser.add_argument('--models-archive');parser.add_argument('--probe')
    args=parser.parse_args()
    selected=next(((name,getattr(args,'check_'+name)) for name in
        ('origins','character','deltas','prepare','matrices','mesh_matrices','vertices') if getattr(args,'check_'+name)),None)
    if selected:
        name,executable=selected
        if not args.probe:
            parser.error('instruction checks require --probe')
        if args.archive!=(name in ('origins','vertices')):
            parser.error('--archive is required for origin and vertex checks')
        if name=='mesh_matrices' and not args.models_archive:
            parser.error('--check-mesh-matrices requires --models-archive')
    try:
        if not selected:
            result=report_archive(args.input) if args.archive else decode(Path(args.input).read_bytes(),args.geometry)
        elif name=='mesh_matrices':
            result=check_character_mesh_matrices(args.input,executable,args.probe,args.models_archive)
        else:
            check={'origins':check_origins,'vertices':check_model_vertices,'character':check_character,'deltas':check_character_deltas,
                'prepare':check_character_prepare,'matrices':check_character_matrices}[name]
            result=check(args.input,executable,args.probe)
        print(json.dumps(result,indent=2))
    except (OSError,ValueError) as error:
        parser.exit(1,f'model: {error}\n')


if __name__=='__main__':
    main()
