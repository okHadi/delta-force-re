#!/usr/bin/env python3
"""Compare native triangle setup with original DF1 x86 instructions."""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess


def check(executable, probe, count):
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
    return dict(source_sha256=hashlib.sha256(source).hexdigest(),code_region='0x461000+0x2000',code_sha256=hashlib.sha256(code).hexdigest(),classification_cases=len(class_cases),gradient_span_cases=len(span_cases),spans=total,mismatches=0,functions=['0x462280','0x461690','0x461b10','0x461cd0','0x461e30'],textured32={mode:check_texture32(pe,probe,count,mode) for mode in ('uniform','varying_shade','cutout')},scope='generated 16.16 triangles; ties, tiny triangles, clipping and both winding orders; gradient flags 0-7; 640x480 inclusive clip; every span field; textured32 has a separate result')


def check_texture32(pe,probe,count,mode):
    setup,draw,option={'uniform':(0x468a6c,0x468cda,'texture32'),'varying_shade':(0x469258,0x4694df,'texture32-shade'),'cutout':(0x468e56,0x4690cc,'texture32-cutout')}[mode]
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
        if mode=='varying_shade':
            cases[-1]+=tuple(rng.randrange(-0x80000000,0x80000000) for i in range(2))
    text=''.join(' '.join(map(str,case))+'\n' for case in cases);output=subprocess.check_output([probe,option],input=text.encode()).decode().splitlines()
    if len(output)!=len(cases):
        raise ValueError('native textured32 case count differs from input')
    u.mem_write(0x3010000,b''.join(struct.pack('<I',(i*0x01010101^0xffa5c3e7)&0xffffffff) for i in range(64*256)));pixels=0
    for number,(case,line) in enumerate(zip(cases,output)):
        test,bx,by,shade,x,y,length,a,b,z,da,db,dz=case[:13]
        s,ds=case[13:] if mode=='varying_shade' else (0,0)
        u.mem_write(0x3000000,bytes((i*37+test*13)&255 for i in range(1<<(bx+by))));u.mem_write(0x3020000,b''.join(struct.pack('<H',(i*317+test*17)&65535) for i in range(512)));u.mem_write(0x3021000,b''.join(struct.pack('<I',(i*1234567^0xabcdef90)&0xffffffff) for i in range(512)))
        u.mem_write(0x2001000,struct.pack('<12I',0,shade,0,0,0,bx,by,0x3020000,0x3021000,0x3000000,0x3010000,0))
        u.mem_write(0x2002000,struct.pack('<10i',y*64+x,length,a,b,z,s,da,db,dz,ds));call(setup,0x2001000);call(draw,0x2002000)
        colours=struct.unpack('<512I',u.mem_read(0x3021000,2048));depths=struct.unpack('<512H',u.mem_read(0x3020000,1024));expected=[value for pair in zip(colours,depths) for value in pair]
        actual=list(map(int,line.split()))
        if actual!=expected:
            raise ValueError(f'textured32 target mismatch at case {number}')
        pixels+=length
    return dict(cases=len(cases),span_pixels=pixels,target_pixels=len(cases)*512,mismatches=0,functions=[hex(setup),hex(draw)],code_region='0x468000+0x2000',code_sha256=hashlib.sha256(pe.get_data(0x68000,0x2000)).hexdigest(),scope=f'32-bit {mode} texture; texture bits 0-8; signed UV, shade and step wrap; 64 shade rows; odd/even spans; strict paired depth test')

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('executable');parser.add_argument('--probe',required=True);parser.add_argument('--cases',type=int,default=1000)
    args=parser.parse_args()
    if not 1<=args.cases<=100000:
        parser.error('--cases must be between 1 and 100000')
    print(json.dumps(check(args.executable,str(Path(args.probe).resolve()),args.cases),indent=2))


if __name__=='__main__':
    main()
