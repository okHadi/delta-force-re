#!/usr/bin/env python3
"""Compare all stock loadout-id triples with original 484970 and 4848C0."""
import argparse,hashlib,itertools,json,struct,subprocess
from pathlib import Path
import pefile,unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
SOURCE_SHA256='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable',type=Path);parser.add_argument('probe',type=Path)
    parser.add_argument('--report',type=Path);args=parser.parse_args()
    raw=args.executable.read_bytes()
    if hashlib.sha256(raw).hexdigest()!=SOURCE_SHA256:raise ValueError('Unsupported executable hash')
    pe=pefile.PE(data=raw);u=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    u.mem_map(0x400000,0x1000000);u.mem_write(0x400000,pe.get_memory_mapped_image())
    u.mem_map(0x2000000,0x10000)
    def put(at,value):u.mem_write(at,struct.pack('<I',value))
    put(0xad3ed4,0x4f9704);put(0xad3ed0,0x4f9934);put(0xad3ee4,0x4f9154)
    u.mem_write(0xc23820,bytes(0x648))
    cases=list(itertools.product(range(28),repeat=3))
    done=subprocess.run([str(args.probe.resolve())],input=''.join('loadout %d %d %d\n'%ids for ids in cases),text=True,capture_output=True,check=True)
    lines=done.stdout.splitlines()
    if len(lines)!=len(cases):raise ValueError('Probe result count differs')
    failures=[]
    for index,(ids,line) in enumerate(zip(cases,lines)):
        u.mem_write(0x2008000,struct.pack('<4I',0x2000000,*ids))
        u.reg_write(UC_X86_REG_ESP,0x2008000);u.reg_write(UC_X86_REG_FPCW,0x037f)
        u.emu_start(0x484970,0x2000000,count=20000)
        if u.reg_read(UC_X86_REG_EIP)!=0x2000000:raise ValueError('Original loadout did not return')
        def words(at,count):return list(struct.unpack('<'+'I'*count,u.mem_read(at,4*count)))
        if words(0xc23a44,3)!=list(ids):raise ValueError('Original loadout-id fields differ')
        expected=dict(map=words(0xc239c0,10),magazine=words(0xc238b8,28),reserve=words(0xc23938,28),pending=words(0xc23838,28))
        got=json.loads(line)
        for key,values in expected.items():
            # Unused classes retain their state across loadout changes.
            if [v&0xffffffff for v in got[key]]!=values:
                failures.append(dict(case=index,ids=ids,field=key));break
    report=dict(source_sha256=SOURCE_SHA256,cases=len(cases),mismatches=len(failures),samples=failures[:5],
        scope='all 28^3 recipe-id triples; full 484970 and 4848C0 instructions; 10 slots and 84 ammo-state words after each call')
    text=json.dumps(report,indent=2);print(text)
    if args.report:args.report.write_text(text+'\n')
    raise SystemExit(bool(failures))
if __name__=='__main__':main()
