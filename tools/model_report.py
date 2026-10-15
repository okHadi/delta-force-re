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


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input');parser.add_argument('--archive',action='store_true');parser.add_argument('--geometry',action='store_true')
    parser.add_argument('--check-origins',metavar='EXE');parser.add_argument('--probe')
    args=parser.parse_args()
    if args.check_origins and (not args.archive or not args.probe):
        parser.error('--check-origins requires --archive and --probe')
    try:
        result=check_origins(args.input,args.check_origins,args.probe) if args.check_origins else (report_archive(args.input) if args.archive else decode(Path(args.input).read_bytes(),args.geometry))
        print(json.dumps(result,indent=2))
    except (OSError,ValueError) as error:
        parser.exit(1,f'model: {error}\n')


if __name__=='__main__':
    main()
