"""Compare item callback binding with the original 0x4011C0 instructions."""
import argparse,hashlib,json,random,struct,subprocess
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('executable',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path)
a=p.parse_args();raw=a.executable.read_bytes();sha=hashlib.sha256(raw).hexdigest()
if sha!='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff':raise SystemExit('Unsupported executable')
pe=pefile.PE(data=raw);u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,0x2000000)
u.mem_write(pe.OPTIONAL_HEADER.ImageBase,pe.get_memory_mapped_image());u.mem_map(0x3000000,0x300000)
ACTOR=0x3100000;STOP=0x3200000;calls=[]
def word(at):return struct.unpack('<I',bytes(u.mem_read(at,4)))[0]
def put(at,v):u.mem_write(at,struct.pack('<I',v))
def sound(machine,at,size,user):
    sp=u.reg_read(UC_X86_REG_ESP);ptr=word(sp+4);calls.append(ptr-ACTOR)
    u.reg_write(UC_X86_REG_EAX,0x80000000+len(calls)-1)
    u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp))
u.hook_add(UC_HOOK_CODE,sound,begin=0x43f490,end=0x43f490)
ai=['null','org0','org1','plyr','brrl','clay','cly2','schg','dec0','sgen','snd0','snd1','snd2','slgt','tree','veh0','helo','spar','sgul','btre','prnh','vult','mine','gnrc','cact','lamp','palm','oakt','wdsm','wdmd','wdlg','bldg','peng','vtrg']
move=['null','smok','slgt','veh0','helo','spar','sgul','prnh','vult','gdeb','lamp','psec','peng']
rng=random.Random(4011);cases=[(x,'null') for x in ai]+[('null',x) for x in move]
cases +=[(x.upper(),'NULL') for x in ai]+[('NULL',x.upper()) for x in move]
cases +=[('-','-'),('unknown','missing')]+[(rng.choice(ai+['bad','-']),rng.choice(move+['bad','-'])) for _ in range(200)]
result=subprocess.check_output([str(a.probe.resolve())],input=''.join(x+' '+y+'\n' for x,y in cases),text=True).splitlines()
if len(result)!=len(cases):raise SystemExit('Probe line count differs')
mismatch=[]
for n,((x,y),native) in enumerate(zip(cases,result)):
    record=bytearray(0x13c)
    for off,name in [(0x7c,x),(0x88,y)]:
        if name!='-':record[off:off+len(name)]=name.encode()
    u.mem_write(ACTOR,bytes(record));put(0xc23810,ACTOR);put(0xc23814,1);calls.clear()
    sp=0x3008000;put(sp,STOP);u.reg_write(UC_X86_REG_ESP,sp)
    u.emu_start(0x4011c0,STOP,count=1000000)
    if u.reg_read(UC_X86_REG_EIP)!=STOP:raise RuntimeError('Original binding did not return')
    expected=[word(ACTOR+off) for off in [0x84,0x90,0x118,0x11c,0x120,0x124]]
    if calls!=[0xe8,0xf4,0x100,0x10c]:raise RuntimeError('Original sound lookup order changed')
    if list(map(int,native.split()))!=expected:mismatch.append({'case':n,'names':[x,y],'native':native,'original':expected})
report={'source_sha256':sha,'entry':'0x4011c0','cases':len(cases),'mismatches':len(mismatch),'samples':mismatch[:5],'hooks':['0x43f490 sound resource lookup; records call order and supplies deterministic ids'],'scope':'Original binder and both callback resolvers execute from original bytes; callback ids are not called.'}
if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
raise SystemExit(bool(mismatch))
