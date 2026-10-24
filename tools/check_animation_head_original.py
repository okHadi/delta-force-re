#!/usr/bin/env python3
"""Compare typed head overrides and partial state with actual frame decoders."""
import argparse,json,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP
from check_projectiles_original import Rig,SOURCE_SHA256
class DivisionFault(Exception):pass
def run(a):
 r=Rig(a.game);u=r.u
 def get(at):return int.from_bytes(u.mem_read(at,4),'little')
 def put(at,n,w=4):u.mem_write(at,(n&((1<<(8*w))-1)).to_bytes(w,'little'))
 def instruction(m,at,size,_):
  if bytes(u.mem_read(at,2))==b'\xf7\xfb' and u.reg_read(UC_X86_REG_EBX)==0:raise DivisionFault()
 u.hook_add(unicorn.UC_HOOK_CODE,instruction);rows=[];expected=[]
 for duration in [1,2,3,65535]:
  for pitch in [0,1,127,128,129,252,255]:
   for y in [0,1,127,128,255]:
    rows.append((duration,pitch,y));u.mem_write(0x1300000,b'\0'*0x4000)
    for at,n in {0x130000c:0x1300600,0x1300010:0x1300200,0x1300014:3,0x1300018:duration,0x1300058:0x34,0x1300080:0x1301000,0x1300084:0x1301200,0x130008c:0x1301400,0x1300090:0x1301500,0x1300204:0x80,0x1300220:0x1301800,0x1300608:1,0x439a08:0,0x439a0c:0,0x439a10:0}.items():put(at,n)
    put(0x1300065,pitch,1)
    for i in range(3):
     for k in range(3):put(0x1301000+i*24+12+k*4,65536)
     for k,n in enumerate([i+1,y if i==2 else i+2,i+3,0]):put(0x1300234+i*4+k,n,1)
    for reg,n in zip([UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP],[0,duration,0,0,0x1300000,0x1300200,0]):u.reg_write(reg,n)
    failed=False
    try:r.call(0x43a4dc if duration==1 else 0x43a2c7)
    except DivisionFault:failed=True
    result=[int(failed),get(0x1300018),*[get(0x1301200+i*4) for i in range(18)],*[get(0x1301500+i*4)for i in range(3)],*[get(0x130001c+i*4)for i in range(3)]];expected.append(' '.join(map(str,result)))
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,row))for row in rows)+'\n',text=True,capture_output=True);assert not p.returncode,p.stderr;actual=p.stdout.splitlines();assert len(actual)==len(expected);diff=[{'duration_pitch_lastY':rows[i],'actual':x,'original':y}for i,(x,y)in enumerate(zip(actual,expected))if x!=y];report={'source_sha256':SOURCE_SHA256,'cases':len(rows),'differences':len(diff),'mismatches':diff[:10],'scope':'Typed current frame duration, pose-delta/root-delta/motion state and head division failure prefix; declared stable valid three-bone model; immediate and timed original instructions','audio_output':'none'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(diff)}));assert bool(diff)==a.expect_differences
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--expect-differences',action='store_true');run(p.parse_args())
