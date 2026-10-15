#!/usr/bin/env python3
"""Compare native triangle setup with original DF1 x86 instructions."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess


def check(executable, probe, count,archive_path=None):
    import pefile
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX
    source=Path(executable).read_bytes();pe=pefile.PE(data=source)
    machine=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    for address,size in ((0x461000,0x2000),(0x8ee000,0x10000),(0x2000000,0x10000),(0x6000000,4096)):
        machine.mem_map(address,size)
    code=pe.get_data(0x461000-pe.OPTIONAL_HEADER.ImageBase,0x2000)
    machine.mem_write(0x461000,code);machine.mem_write(0x8ee1e0,struct.pack('<I',0x2001000))
    machine.mem_write(0x2001034,struct.pack('<4i',0,0,639,479));machine.mem_write(0x2001050,struct.pack('<I',8))
    machine.mem_write(0x2001068,b''.join(struct.pack('<I',0x3000000+i*640) for i in range(480)))
    def call(address,*args):
        machine.mem_write(0x2000800,struct.pack('<'+'I'*(len(args)+1),0x6000000,*args))
        machine.reg_write(UC_X86_REG_ESP,0x2000800)
        machine.emu_start(address,0x6000000,count=2000000)
        if machine.reg_read(UC_X86_REG_EIP)!=0x6000000:
            raise ValueError(f'original raster function {address:x} exceeded instruction limit')
    def input_triangle(points):
        machine.mem_write(0x2002000,b''.join(struct.pack('<8i',*p,p[0]//65536,p[1]//65536) for p in points))
    rng=random.Random(517);class_cases=[]
    for i in range(count*10):
        points=[(rng.randrange(-100,741)*65536+rng.randrange(65536),rng.randrange(-100,581)*65536+rng.randrange(65536)) for _ in range(3)]
        if i<20:
            points=[(i*1000,i*1000)]*3
        class_cases.append(points)
    text=''.join('0 0 639 479 '+' '.join(str(v) for p in case for v in p)+'\n' for case in class_cases)
    native=subprocess.check_output([probe],input=text.encode()).decode().splitlines()
    if len(native)!=len(class_cases):
        raise ValueError('native classification count differs from input')
    for number,(points,line) in enumerate(zip(class_cases,native)):
        input_triangle([(x,y,0,0,0,0) for x,y in points]);call(0x462280,0x2002000)
        extent=machine.reg_read(UC_X86_REG_EAX);indices=struct.unpack('<6I',machine.mem_read(0x8ee208,24))
        flags=struct.unpack('<I',machine.mem_read(0x8ee1f4,4))[0] if extent else 0
        if list(map(int,line.split()))!=[extent,flags,*indices]:
            raise ValueError(f'triangle classification mismatch at case {number}')
    rng=random.Random(776);span_cases=[]
    for i in range(count):
        points=[(rng.randrange(-40,681)*65536+rng.randrange(65536),rng.randrange(-40,521)*65536+rng.randrange(65536),rng.randrange(1,1000)*65536,rng.randrange(-0x1000000,0x1000000),rng.randrange(-0x1000000,0x1000000),rng.randrange(0x1000000)) for _ in range(3)]
        if i<10:
            points=[(j*65536,(j%2)*65536,65536,0,0,0) for j in range(3)]
        span_cases.append((points,i%8))
    text=''.join('0 0 639 479 '+' '.join(str(v) for p in case for v in p[:2])+' '+' '.join(str(v) for p in case for v in p[2:])+' '+str(flags)+'\n' for case,flags in span_cases)
    native=subprocess.check_output([probe,'spans'],input=text.encode()).decode().splitlines();total=0
    if len(native)!=len(span_cases):
        raise ValueError('native span case count differs from input')
    for number,((points,flags),line) in enumerate(zip(span_cases,native)):
        input_triangle(points);machine.mem_write(0x2004000,struct.pack('<I8i',flags,*([0]*8)))
        call(0x462280,0x2002000);call(0x461690,0x2002000,0x2004000)
        gradients=struct.unpack('<8i',machine.mem_read(0x2004004,32))
        machine.mem_write(0x8ee1f8,bytes(4));call(0x461e30,0x2002000,0x2004000)
        size=struct.unpack('<I',machine.mem_read(0x8ee1f8,4))[0]
        if size>480:
            raise ValueError('original span count exceeds screen height')
        raw=machine.mem_read(0x8ee220,size*40);expected=[*gradients,size]
        for index in range(size):
            span=struct.unpack_from('<10i',raw,index*40);offset=span[0]
            expected.extend([offset%640,offset//640,*span[1:]])
        if list(map(int,line.split()))!=expected:
            raise ValueError(f'triangle gradient/span mismatch at case {number}')
        total+=size
    return dict(source_sha256=hashlib.sha256(source).hexdigest(),code_region='0x461000+0x2000',code_sha256=hashlib.sha256(code).hexdigest(),classification_cases=len(class_cases),gradient_span_cases=len(span_cases),spans=total,mismatches=0,functions=['0x462280','0x461690','0x461b10','0x461cd0','0x461e30'],textured32={mode:check_texture32(pe,probe,count,mode) for mode in ('uniform','varying_shade','cutout','varying_cutout')},lighting=check_lighting(pe,probe,count,archive_path),clipping=check_clipping(pe,probe,count),tiny32=check_tiny32(pe,probe,count),local_lighting=check_local_lighting(pe,probe,count),camera=check_camera(pe,probe,count),scope='generated 16.16 triangles; ties, tiny triangles, clipping and both winding orders; gradient flags 0-7; 640x480 inclusive clip; every span field; textured32 has a separate result')


def check_texture32(pe,probe,count,mode):
    setup,draw,option={'uniform':(0x468a6c,0x468cda,'texture32'),'varying_shade':(0x469258,0x4694df,'texture32-shade'),'cutout':(0x468e56,0x4690cc,'texture32-cutout'),'varying_cutout':(0x4696b3,0x469945,'texture32-shade-cutout')}[mode]
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
    u=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    for a,n in [(0x468000,0x2000),(0x2000000,0x10000),(0x3000000,0x23000),(0x6000000,4096)]:u.mem_map(a,n)
    u.mem_write(0x468000,pe.get_data(0x68000,0x2000))
    def call(start,*args):
        u.mem_write(0x2000800,struct.pack('<'+'I'*(len(args)+1),0x6000000,*args));u.reg_write(UC_X86_REG_ESP,0x2000800);u.emu_start(start,0x6000000,count=1000000)
        if u.reg_read(UC_X86_REG_EIP)!=0x6000000:
            raise ValueError('original texture function exceeded instruction limit')
    rng=random.Random(424);cases=[]
    for test in range(count):
        x=rng.randrange(64);length=rng.randrange(65-x)
        cases.append((test,rng.randrange(0,9),rng.randrange(0,9),rng.randrange(0x100000000),x,rng.randrange(8),length,*[rng.randrange(-0x80000000,0x80000000) for i in range(6)]))
        if mode in ('varying_shade','varying_cutout'):
            cases[-1]+=tuple(rng.randrange(-0x80000000,0x80000000) for i in range(2))
    text=''.join(' '.join(map(str,case))+'\n' for case in cases);output=subprocess.check_output([probe,option],input=text.encode()).decode().splitlines()
    if len(output)!=len(cases):
        raise ValueError('native textured32 case count differs from input')
    u.mem_write(0x3010000,b''.join(struct.pack('<I',(i*0x01010101^0xffa5c3e7)&0xffffffff) for i in range(64*256)));pixels=0
    for number,(case,line) in enumerate(zip(cases,output)):
        test,bx,by,shade,x,y,length,a,b,z,da,db,dz=case[:13]
        s,ds=case[13:] if mode in ('varying_shade','varying_cutout') else (0,0)
        u.mem_write(0x3000000,bytes((i*37+test*13)&255 for i in range(1<<(bx+by))));u.mem_write(0x3020000,b''.join(struct.pack('<H',(i*317+test*17)&65535) for i in range(512)));u.mem_write(0x3021000,b''.join(struct.pack('<I',(i*1234567^0xabcdef90)&0xffffffff) for i in range(512)))
        u.mem_write(0x2001000,struct.pack('<12I',0,shade,0,0,0,bx,by,0x3020000,0x3021000,0x3000000,0x3010000,0))
        u.mem_write(0x2002000,struct.pack('<10i',y*64+x,length,a,b,z,s,da,db,dz,ds));call(setup,0x2001000);call(draw,0x2002000)
        colours=struct.unpack('<512I',u.mem_read(0x3021000,2048));depths=struct.unpack('<512H',u.mem_read(0x3020000,1024));expected=[value for pair in zip(colours,depths) for value in pair]
        actual=list(map(int,line.split()))
        if actual!=expected:
            raise ValueError(f'textured32 target mismatch at case {number}')
        pixels+=length
    return dict(cases=len(cases),span_pixels=pixels,target_pixels=len(cases)*512,mismatches=0,functions=[hex(setup),hex(draw)],code_region='0x468000+0x2000',code_sha256=hashlib.sha256(pe.get_data(0x68000,0x2000)).hexdigest(),scope=f'32-bit {mode} texture; texture bits 0-8; signed UV, shade and step wrap; 64 shade rows; odd/even spans; strict paired depth test')

def check_lighting(pe,probe,count,archive_path=None):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
    machine=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    for address,size in ((0x46a000,0x2000),(0xad3000,4096),(0x2000000,0x10000),(0x6000000,4096)):
        machine.mem_map(address,size)
    code=pe.get_data(0x6a000,0x2000);machine.mem_write(0x46a000,code)
    cases=models=normals=materials=0
    def batch(rows):
        text=''.join(' '.join(map(str,row))+'\n' for row in rows)
        values=subprocess.check_output([probe,'lighting'],input=text.encode()).decode().splitlines()
        if len(values)!=len(rows):
            raise ValueError('native normal-shade count differs from input')
        for index,(row,value) in enumerate(zip(rows,values)):
            x,y,z,flags,specular,constant,power,reverse,*context=row
            material=bytearray(76);struct.pack_into('<I',material,16,flags);struct.pack_into('<iiB',material,56,specular,constant,power)
            machine.mem_write(0x2001000,struct.pack('<4h',x,y,z,0));machine.mem_write(0x2002000,bytes(material))
            machine.mem_write(0xad3d80,struct.pack('<3i',*context[:3]));machine.mem_write(0xad3d90,struct.pack('<3i',*context[3:6]));machine.mem_write(0xad3da0,struct.pack('<3i',*context[6:]))
            machine.mem_write(0x2000800,struct.pack('<4I',0x6000000,0x2001000,0x2002000,reverse));machine.reg_write(UC_X86_REG_ESP,0x2000800)
            machine.emu_start(0x46aff0,0x6000000,count=100000)
            if machine.reg_read(UC_X86_REG_EIP)!=0x6000000:
                raise ValueError('original normal lighting exceeded instruction limit')
            if int(value)!=machine.reg_read(UC_X86_REG_EAX):
                raise ValueError(f'normal lighting mismatch in batch case {index}')
    rng=random.Random(681);rows=[]
    for index in range(count):
        rows.append(tuple(rng.randrange(-32768,32768) for _ in range(3))+(0x20 if index&1 else 0,rng.randrange(-0x80000000,0x80000000),rng.randrange(-0x80000000,0x80000000),255 if index<10 else rng.randrange(8),index&1)+tuple(rng.randrange(-0x400000,0x400001) for _ in range(6))+tuple(rng.randrange(-0x80000000,0x80000000) for _ in range(3)))
    batch(rows);cases+=len(rows)
    if archive_path:
        from pff import Archive
        from model_report import decode
        archive=Archive(archive_path)
        with archive.path.open('rb') as source:
            for entry in archive.entries:
                if entry.deleted or not entry.name.lower().endswith('.3di'):
                    continue
                source.seek(entry.offset);data=source.read(entry.size)
                if data[:4]!=b'3DI\x05':
                    continue
                model=decode(data,True);models+=1
                for lod in model['lods']:
                    if not lod['materials']:
                        continue
                    rows=[];materials+=len(lod['materials']);normals+=len(lod['normals'])
                    for index,normal in enumerate(lod['normals']):
                        material=lod['materials'][index%len(lod['materials'])];offset=material['offset']
                        specular,constant,power=struct.unpack_from('<iiB',data,offset+56)
                        for reverse in (0,1):
                            rows.append((*normal[:3],material['flags'],specular,constant,power,reverse,0x123456,-0x234567,0x345678,-0x345678,0x234567,0x123456,20000,40000,30000))
                    batch(rows);cases+=len(rows)
    return dict(cases=cases,models=models,normals=normals,material_records=materials,mismatches=0,function='0x46aff0',code_region='0x46a000+0x2000',code_sha256=hashlib.sha256(code).hexdigest(),archive_sha256=hashlib.sha256(Path(archive_path).read_bytes()).hexdigest() if archive_path else None,scope='generated wrap, clamp, constant and specular cases; all v5 stored normals paired cyclically with material records; both face directions; mesh-local light/view directions')


def check_clipping(pe,probe,count):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
    machine=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    for address,size in ((0x46a000,0x2000),(0xa1c000,0x10000),(0xaaa000,0x10000),(0xad3000,4096),(0x2000000,4096),(0x6000000,4096)):
        machine.mem_map(address,size)
    code=pe.get_data(0x6a000,0x2000);machine.mem_write(0x46a000,code)
    rng=random.Random(771);cases=[]
    for test in range(count):
        points=[tuple(rng.randrange(-0x100000,0x100001) for _ in range(3)) for _ in range(3)]
        if test<8:
            points=[((-65536 if test&(1<<j) else 65536),p[1],p[2]) for j,p in enumerate(points)]
        uv=[rng.randrange(-0x80000000,0x80000000) for _ in range(6)]
        planes=[(0x400000,0,0,0)]+[tuple(rng.randrange(-0x400000,0x400001) for _ in range(3))+(rng.randrange(-0x20000,0x20001),) for _ in range(test%5)]
        cases.append((points,uv,planes))
    text=''.join(str(len(planes))+' '+' '.join(str(v) for p in points for v in p)+' '+' '.join(map(str,uv))+' '+' '.join(str(v) for p in planes for v in p)+'\n' for points,uv,planes in cases)
    native=subprocess.check_output([probe,'clip'],input=text.encode()).decode().splitlines()
    if len(native)!=len(cases):
        raise ValueError('native clip case count differs from input')
    total_vertices=total_faces=rejected=plane_count=0
    for number,((points,uv,planes),line) in enumerate(zip(cases,native)):
        machine.mem_write(0xaaacb0,b''.join(struct.pack('<3i20x',*p) for p in points))
        face=bytearray(72);struct.pack_into('<6i',face,4,*uv);struct.pack_into('<3H',face,28,0,1,2)
        machine.mem_write(0xa1c370,bytes(face));machine.mem_write(0xad3db4,struct.pack('<II',3,1))
        for plane in planes:
            machine.mem_write(0x2000800,struct.pack('<I4i',0x6000000,*plane));machine.reg_write(UC_X86_REG_ESP,0x2000800)
            machine.emu_start(0x46a250,0x6000000,count=1000000)
            if machine.reg_read(UC_X86_REG_EIP)!=0x6000000:
                raise ValueError('original clipping exceeded instruction limit')
            plane_count+=1
        vertices,faces=struct.unpack('<II',machine.mem_read(0xad3db4,8))
        if vertices>512 or faces>512:
            raise ValueError('original clip check exceeds mapped buffers')
        expected=[vertices,faces];positions=machine.mem_read(0xaaacb0,vertices*32);face_bytes=machine.mem_read(0xa1c370,faces*72)
        for index in range(vertices):
            expected.extend(struct.unpack_from('<3i',positions,index*32))
        for index in range(faces):
            indexes=struct.unpack_from('<3H',face_bytes,index*72+28);uvs=struct.unpack_from('<6i',face_bytes,index*72+4)
            dead=int(indexes[0]==65535);expected.extend([dead,*indexes,*uvs]);rejected+=dead
        actual=list(map(int,line.split()));offset=2+vertices*3
        if len(actual)!=len(expected):
            raise ValueError(f'clipped geometry output count differs at case {number}')
        for index in range(faces):
            # Native rejection uses a bool; the original replaces the first index with -1.
            if actual[offset+index*10]:
                actual[offset+index*10+1]=65535
        if actual!=expected:
            raise ValueError(f'plane clipping mismatch at case {number}')
        total_vertices+=vertices;total_faces+=faces
    return dict(cases=len(cases),planes=plane_count,vertices=total_vertices,faces=total_faces,rejected=rejected,mismatches=0,functions=['0x46a250','0x46a110'],code_region='0x46a000+0x2000',code_sha256=hashlib.sha256(code).hexdigest(),scope='generated camera-space triangles; all eight initial outside masks; one to five planes; wrapped UV interpolation; exact vertex and split-face order; native rejected bool normalized to original -1 marker')


def check_tiny32(pe,probe,count):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
    machine=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    for address,size in ((0x461000,0x2000),(0x8ee000,4096),(0xbfc000,4096),(0x2000000,0x10000),(0x3000000,0x4000),(0x6000000,4096)):
        machine.mem_map(address,size)
    machine.mem_write(0x461000,pe.get_data(0x61000,0x2000));machine.mem_write(0x8ee1e0,struct.pack('<I',0x2001000));machine.mem_write(0x2001034,struct.pack('<4i',0,0,63,7));machine.mem_write(0x2001050,struct.pack('<I',32))
    machine.mem_write(0x2001068,b''.join(struct.pack('<I',0x3001000+i*256) for i in range(8)));machine.mem_write(0x2001c68,b''.join(struct.pack('<I',0x3002000+i*128) for i in range(8)));machine.mem_write(0xbfc108,struct.pack('<I',0x3000000))
    rng=random.Random(274);cases=[]
    for test in range(count):
        x=rng.randrange(-1,65);y=rng.randrange(-1,9)
        points=[((x+rng.randrange(2))*65536+rng.randrange(65536),(y+rng.randrange(2))*65536+rng.randrange(65536),rng.randrange(-0x80000000,0x80000000)) for _ in range(3)]
        cases.append((test,rng.randrange(0x800),rng.randrange(0x100000000),points))
    text=''.join(' '.join(map(str,(test,flags,flat,*[v for point in points for v in point])))+'\n' for test,flags,flat,points in cases)
    native=subprocess.check_output([probe,'tiny32'],input=text.encode()).decode().splitlines();extents=[0,0,0]
    if len(native)!=len(cases):
        raise ValueError('native tiny triangle case count differs from input')
    def call(address,*args):
        machine.mem_write(0x2000800,struct.pack('<'+'I'*(len(args)+1),0x6000000,*args));machine.reg_write(UC_X86_REG_ESP,0x2000800);machine.emu_start(address,0x6000000,count=100000)
        if machine.reg_read(UC_X86_REG_EIP)!=0x6000000:
            raise ValueError('original tiny triangle function exceeded instruction limit')
    for number,((test,flags,flat,points),line) in enumerate(zip(cases,native)):
        machine.mem_write(0x3000000,struct.pack('<I',flat));machine.mem_write(0x3001000,b''.join(struct.pack('<I',(i*1234567^0xabcdef90)&0xffffffff) for i in range(512)));machine.mem_write(0x3002000,b''.join(struct.pack('<H',(i*317+test*17)&65535) for i in range(512)))
        machine.mem_write(0x2002000,b''.join(struct.pack('<8i',x,y,z,0,0,0,x//65536,y//65536) for x,y,z in points));call(0x462280,0x2002000)
        extent=machine.reg_read(UC_X86_REG_EAX)
        if extent>2:
            raise ValueError('generated tiny triangle exceeds two-pixel extent')
        extents[extent]+=1
        if extent:
            call(0x4613c0 if extent==1 else 0x4614c0,flags,0x2002000,0x2003000)
        colours=struct.unpack('<512I',machine.mem_read(0x3001000,2048));depths=struct.unpack('<512H',machine.mem_read(0x3002000,1024))
        expected=[value for pair in zip(colours,depths) for value in pair]
        if list(map(int,line.split()))!=expected:
            raise ValueError(f'tiny triangle target mismatch at case {number}')
    return dict(cases=count,extents=extents,target_pixels=count*512,mismatches=0,functions=['0x4613c0','0x4614c0'],scope='cached flat colour; per-vertex unsigned depth; inclusive clip; one/two-pixel extents; flags0x600 skip; all colour/depth target pairs')


def check_local_lighting(pe,probe,count):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
    machine=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    for address,size in ((0x45c000,0x2000),(0x46b000,4096),(0xad3000,4096),(0x2000000,0x10000),(0x6000000,4096)):
        machine.mem_map(address,size)
    first=pe.get_data(0x5c000,0x2000);second=pe.get_data(0x6b000,4096);machine.mem_write(0x45c000,first);machine.mem_write(0x46b000,second)
    def signed(value):
        return ((value+0x80000000)&0xffffffff)-0x80000000
    def product(a,b):
        return signed((a*b)>>22)
    rng=random.Random(194);cases=[]
    while len(cases)<count:
        matrix=[rng.randrange(-0x400000,0x400001) if i%4!=3 else rng.randrange(-0x80000000,0x80000000) for i in range(12)]
        determinant=signed(product(matrix[0],product(matrix[5],matrix[10]))+product(matrix[1],product(matrix[6],matrix[8]))+product(matrix[2],product(matrix[4],matrix[9]))-product(matrix[0],product(matrix[6],matrix[9]))-product(matrix[1],product(matrix[4],matrix[10]))-product(matrix[2],product(matrix[5],matrix[8])))
        if abs(determinant)<65536:
            continue
        context=[rng.randrange(-0x400000,0x400001) for _ in range(6)]+[rng.randrange(-0x80000000,0x80000000) for _ in range(3)]
        cases.append((*matrix,*context,rng.randrange(-0x80000000,0x80000000)))
    text=''.join(' '.join(map(str,row))+'\n' for row in cases);native=subprocess.check_output([probe,'local-lighting'],input=text.encode()).decode().splitlines()
    if len(native)!=len(cases):
        raise ValueError('native local lighting case count differs from input')
    for number,(case,line) in enumerate(zip(cases,native)):
        machine.mem_write(0x2001000,struct.pack('<12i16x',*case[:12]));machine.mem_write(0xad3d40,struct.pack('<3i',*case[12:15]));machine.mem_write(0xad3d50,struct.pack('<3i',*case[15:18]));machine.mem_write(0xad3d60,struct.pack('<3i',*case[18:21]));machine.mem_write(0x2000800,struct.pack('<IIi',0x6000000,0x2001000,case[21]));machine.reg_write(UC_X86_REG_ESP,0x2000800)
        machine.emu_start(0x46b140,0x6000000,count=100000)
        if machine.reg_read(UC_X86_REG_EIP)!=0x6000000:
            raise ValueError('original local lighting conversion exceeded instruction limit')
        expected=[*struct.unpack('<3i',machine.mem_read(0xad3d80,12)),*struct.unpack('<3i',machine.mem_read(0xad3d90,12)),*struct.unpack('<3i',machine.mem_read(0xad3da0,12))]
        if list(map(int,line.split()))!=expected:
            raise ValueError(f'local lighting mismatch at case {number}')
    return dict(cases=count,mismatches=0,functions=['0x46b140','0x45d090','0x45cfe0','0x45cf30'],code_sha256=hashlib.sha256(first+second).hexdigest(),scope='non-singular generated camera matrices; original nested fixed-point determinant and transpose reciprocal; light/view directions; wrapped signed strength scale; translation excluded from directions')


def check_camera(pe,probe,count):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
    machine=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    ranges=((0x44a000,4096),(0x45c000,0x2000),(0x477000,4096),(0x49b000,4096),(0x4cc000,0x2000),(0x4fb000,4096))
    for address,size in (*ranges,(0xbd0000,0x4000),(0x2000000,0x10000),(0x6000000,4096)):
        machine.mem_map(address,size)
    for address,size in ranges:
        machine.mem_write(address,pe.get_data(address-pe.OPTIONAL_HEADER.ImageBase,size))
    machine.reg_write(UC_X86_REG_FPCW,0x037f)
    def call(address,*args):
        machine.mem_write(0x2000800,struct.pack('<'+'I'*(len(args)+1),0x6000000,*args));machine.reg_write(UC_X86_REG_ESP,0x2000800)
        machine.emu_start(address,0x6000000,count=1000000)
        if machine.reg_read(UC_X86_REG_EIP)!=0x6000000:
            raise ValueError('original camera setup exceeded instruction limit')
    call(0x477510);rng=random.Random(420);cases=[]
    for test in range(count):
        width=rng.choice([1,2,3,160,320,640,800,1024]);height=rng.choice([1,2,3,100,200,480,600,768]);left=rng.randrange(10);top=rng.randrange(10)
        cases.append((left,top,left+width-1,top+height-1,test+1 if test<179 else rng.randrange(1,180),*[rng.randrange(-0x80000000,0x80000000) for _ in range(3)],*[rng.randrange(0x100000000) for _ in range(3)]))
    text=''.join(' '.join(map(str,row))+'\n' for row in cases);native=subprocess.check_output([probe,'camera'],input=text.encode()).decode().splitlines()
    if len(native)!=len(cases):
        raise ValueError('native camera case count differs from input')
    for number,(case,line) in enumerate(zip(cases,native)):
        left,top,right,bottom,fov,*pose=case;raw=bytearray(80);struct.pack_into('<3i3I',raw,4,*pose)
        struct.pack_into('<6I',raw,0x24,right+1,bottom+1,(right+1)*4,left,top,right);struct.pack_into('<I',raw,0x3c,bottom);struct.pack_into('<2I',raw,0x48,fov,32)
        machine.mem_write(0x2001000,bytes(0x2000));machine.mem_write(0x2004000,bytes(raw));call(0x44a820,0x2001000,0x2004000)
        projection=struct.unpack('<3i',machine.mem_read(0x2001054,12));matrix=struct.unpack('<12i',machine.mem_read(0x2002920,48));planes=struct.unpack('<20i',machine.mem_read(0x2002880,80))
        expected=[*projection,planes[3],*matrix,*planes[:12],*planes[16:],*planes[12:16]]
        if list(map(int,line.split()))!=expected:
            raise ValueError(f'model camera mismatch at case {number}')
    return dict(cases=count,fov_degrees=[min(row[4] for row in cases),max(row[4] for row in cases)],mismatches=0,function='0x44a820',code_region='0x44a000+0x1000',code_sha256=hashlib.sha256(pe.get_data(0x4a000,4096)).hexdigest(),scope='generated positions/full-turn angles; inclusive viewport and offsets; focal/centre/near; twelve camera matrix components; five planes in original dispatch order; native long-double camera math; original x87 control037f')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('executable');parser.add_argument('--probe',required=True);parser.add_argument('--cases',type=int,default=1000);parser.add_argument('--models-archive')
    args=parser.parse_args()
    if not 1<=args.cases<=100000:
        parser.error('--cases must be between 1 and 100000')
    print(json.dumps(check(args.executable,str(Path(args.probe).resolve()),args.cases,args.models_archive),indent=2))


if __name__=='__main__':
    main()
