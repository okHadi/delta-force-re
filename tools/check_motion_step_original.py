"""Check the ported motion-step rules against the original DF1 instructions.

Replays the original ranges in Unicorn and compares them with motion_step_probe.
Exits non-zero when any probe line is missing or any case mismatches.
"""
import argparse,hashlib,json,struct,subprocess,sys
from pathlib import Path
import pefile,unicorn
from unicorn.x86_const import *

p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path);args=p.parse_args()
raw=(args.game/'Df.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
if sha!='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff':raise SystemExit('Unsupported original executable hash')
pe=pefile.PE(data=raw);u=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
for at,size in [(0x400000,0xa00000),(0x1000000,0x900000),(0x2000000,0x10000),(0x4000000,0x1000)]:u.mem_map(at,size)
for s in pe.sections:
 if s.PointerToRawData and not s.Characteristics&0x80:u.mem_write(0x400000+s.VirtualAddress,raw[s.PointerToRawData:s.PointerToRawData+s.SizeOfRawData])
def put(at,v):u.mem_write(at,struct.pack('<I',v&0xffffffff))
def word(at):return struct.unpack('<I',u.mem_read(at,4))[0]
def signed(v):return v if v<0x80000000 else v-0x100000000
sg=signed
PROBE=str(args.probe.resolve())
def probe(mode,rows):
 rows=list(rows)
 out=subprocess.check_output([PROBE,mode],input=''.join(' '.join(map(str,row))+'\n' for row in rows).encode()).decode().splitlines()
 if len(out)!=len(rows):raise SystemExit(f'{mode}: probe returned {len(out)} lines for {len(rows)} cases')
 return [line.split() for line in out]
ACTOR=0x1000000;ANIMDEF=0x1100000;FRAMES=0x1200000;DEF=0x1300000;STRUCT=0xc23820
sincos_cache={}
def sincos_of(angle):
 if angle not in sincos_cache:
  row=probe('sincos',[(angle,)])[0]
  sincos_cache[angle]=(int(row[0]),int(row[1]))
 return sincos_cache[angle]
state={'steady':0,'slope':(0,0),'height':0,'stops':set()}
def hook(machine,at,size,user):
 if at in state['stops']:u.emu_stop();return
 if at==0x4776c0:
  sp=u.reg_read(UC_X86_REG_ESP);sine,cosine=sincos_of(word(sp+4))
  u.mem_write(word(sp+8),struct.pack('<i',sine));u.mem_write(word(sp+12),struct.pack('<i',cosine))
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp));return
 if at in (0x42ce40,0x45dc70,0x43e7d0):
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp));return
 if at==0x47df40:
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,state['height']);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp));return
 if at==0x47e5d0:
  sp=u.reg_read(UC_X86_REG_ESP)
  u.mem_write(word(sp+12),struct.pack('<i',state['slope'][0]));u.mem_write(word(sp+16),struct.pack('<i',state['slope'][1]))
  u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp));return
 if at==0x484740:
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,state['steady']);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp));return
u.hook_add(unicorn.UC_HOOK_CODE,hook)
def setup_actor(x,y,z,yaw,pitch,vertical,flags,clearance_value=0):
 u.mem_write(ACTOR,bytes(0x200))
 put(ACTOR+0x24,x);put(ACTOR+0x28,y);put(ACTOR+0x2c,z);put(ACTOR+0x30,yaw);put(ACTOR+0x34,pitch)
 put(ACTOR+0x44,vertical);put(ACTOR+0x6c,flags);put(ACTOR+0x108,0);put(ACTOR+0x90,0)
 u.mem_write(ANIMDEF,bytes(0x100));put(ANIMDEF+0x78,FRAMES);u.mem_write(FRAMES,bytes(0x200))
 put(ACTOR+0x04,ANIMDEF);put(ACTOR+0x7c,0);put(ACTOR+0x80,0);put(ACTOR+0x18,DEF)
 u.mem_write(DEF,bytes(0x100));put(DEF+0x1c,7);put(DEF+0x20,11);put(DEF+0x24,13)
 u.mem_write(0xc91e80,bytes(0x60));put(0xc91e90,0x111);put(0xc91e94,0x222);put(0xc91e98,0x333)
 put(DEF+0x10,0x1420000);u.mem_write(0x1420000,bytes(0x10))
 if clearance_value:
  put(DEF+0xc,0x1400000);u.mem_write(0x1400000,bytes(0x20));put(0x1400008,1)
  put(DEF+0x8c,0x1410000);u.mem_write(0x1410000,bytes(0x10));put(0x1410004,clearance_value)
 else:
  put(DEF+0xc,0x1400000);u.mem_write(0x1400000,bytes(0x20));put(0x1400008,0)
def run_frame_step(slot=0,param_3=1,stop=0x487565):
 sp=0x2008000;u.mem_write(sp,struct.pack('<4I',0x4000000,slot,ACTOR,param_3))
 u.reg_write(UC_X86_REG_ESP,sp)
 state['stops']={stop}
 u.emu_start(0x487370,0x4000000,count=40000)
 if u.reg_read(UC_X86_REG_EIP) not in state['stops']:raise RuntimeError('Original motion block did not reach its stop: %x' % u.reg_read(UC_X86_REG_EIP))
report={'source_sha256':sha,'probe':PROBE,'checks':{}}
def record(name,cases,checked,mismatch,note=''):
 if cases<=0:raise SystemExit(f'{name}: no cases')
 if checked!=cases:raise SystemExit(f'{name}: checked {checked} of {cases} cases')
 entry={'cases':cases,'checked':checked,'mismatches':mismatch}
 if note:entry['note']=note
 report['checks'][name]=entry
# 0x487370-0x487565: frame step, slope response and the step cache skip.
cases=[]
for vertical,three_d in ((0,0),(0,1),(1,0)):
 for speed in (0,1,100,0x100000,0x7fffffff,-100,-0x100000,0x80000000):
  for yaw,angle,pitch in ((0,0,0),(0x12345678,0x1000,0x20000000),(0xffffffff,1,0xd0000000),(0x80000000,0x7fffffff,0x40000000)):
   for z,water in ((0,0x10000),(0x10000,0x10000),(0x10000,0),(0x7fffffff,0x80000000),(0x80000000,0x7fffffff)):
    for slope_mode in (0,1,2,3,4,5):
     cases.append((vertical,three_d,yaw,angle,pitch,speed,slope_mode,z,water))
native_step=probe('frame_step',[(c[0],c[1],c[2],c[3],c[4],sg(c[5]),7,11,13,sg(c[7]),sg(c[8])) for c in cases])
slope_rows=[]
for c,step_line in zip(cases,native_step):
 slope_x,slope_y,blocked={1:(0,0,0),2:(0x20000,0,0),3:(0xb400,0xc000,0),4:(0,0,1),5:(0x20000,0x20000,1)}.get(c[6],(0,0,0))
 if c[7]==0x7fffffff:slope_x,slope_y=-0x80000000,0x7fffffff
 pair=sincos_of((c[2]+c[3])&0xffffffff)
 slope_flags=(0 if c[6]==0 else 1)|(2*0x10000 if c[1] else 0)|(0x40000 if blocked else 0)
 slope_rows.append((int(step_line[0]),int(step_line[1]),int(step_line[2]),slope_flags,pair[0],pair[1],slope_x,slope_y))
native_slope=probe('slope',slope_rows)
mismatch=0;checked=0
for case_index,(case,step_line,slope_line) in enumerate(zip(cases,native_step,native_slope)):
 vertical,three_d,yaw,angle,pitch,speed,slope_mode,z,water=case
 slope_x,slope_y,blocked={1:(0,0,0),2:(0x20000,0,0),3:(0xb400,0xc000,0),4:(0,0,1),5:(0x20000,0x20000,1)}.get(slope_mode,(0,0,0))
 if z==0x7fffffff:slope_x,slope_y=-0x80000000,0x7fffffff
 actor_flags=(0 if slope_mode==0 else 1)|(0x20000 if three_d else 0)|(0x40000 if blocked else 0)
 state['slope']=(slope_x,slope_y)
 setup_actor(0,0,z,yaw,pitch,0,actor_flags)
 put(FRAMES+0x00,angle);put(FRAMES+0x08,speed);put(FRAMES+0x0c,1 if vertical else 0);put(FRAMES+0x10,0)
 put(0xbd5248,water)
 try:
  run_frame_step()
 except Exception as error:
  raise SystemExit(f'frame_step case {case_index} {case}: {error} eip={hex(u.reg_read(UC_X86_REG_EIP))}') from error
 step=[int(step_line[0]),int(step_line[1]),int(step_line[2])]
 if int(slope_line[5]):
  expected_step=[int(slope_line[0]),int(slope_line[1]),int(slope_line[2])]
  expected_flags=(4 if int(slope_line[3]) else 0)|(8 if int(slope_line[4]) else 0)
  native=(expected_step[0],expected_step[1],expected_step[2],expected_flags)
 else:
  native=(step[0],step[1],step[2],4 if blocked else 0)
 got=(signed(u.reg_read(UC_X86_REG_EBP)),signed(u.reg_read(UC_X86_REG_EDI)),signed(u.reg_read(UC_X86_REG_ESI)),
      u.mem_read(ACTOR+0x6e,1)[0]&0xc)
 checked+=1
 if native!=got:
  mismatch+=1
  if mismatch<=6:print('frame mismatch',case,'native',native,'orig',got)
record('frame_step_0x487370',len(cases),checked,mismatch,'slope response composed from the probe slope mode; sine/cosine from the 4776C0 hook')
# 0x487991-0x4879CC step cache.
cases=[]
for airborne,object_bit in ((0,0),(1,0),(0,1),(1,1)):
 for dx,dy,dz in ((0,0,0),(1,2,3),(-5,0x10000,0x7fffffff)):
  for cx,cy,cz in ((0x111,0x222,0x333),(-1,-2,-3)):
   cases.append((airborne,object_bit,dx,dy,dz,cx,cy,cz))
native_cache=probe('step_cache',[(c[2],c[3],c[4],c[0],c[5],c[6],c[7]) for c in cases])
mismatch=0;checked=0
for case,line in zip(cases,native_cache):
 airborne,object_bit,dx,dy,dz,cx,cy,cz=case
 setup_actor(0,0,0,0,0,0,object_bit|(0x2000 if airborne else 0))
 put(0xc91e90,cx);put(0xc91e94,cy);put(0xc91e98,cz)
 sp=0x2008000;u.mem_write(sp,bytes(0x100))
 u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_EBP,dx&0xffffffff);u.reg_write(UC_X86_REG_EDI,dy&0xffffffff)
 u.reg_write(UC_X86_REG_ESI,dz&0xffffffff)
 state['stops']={0x487565}
 u.emu_start(0x487991,0x4000000,count=2000)
 if u.reg_read(UC_X86_REG_EIP) not in state['stops']:raise RuntimeError('Original motion block did not reach its stop: %x' % u.reg_read(UC_X86_REG_EIP))
 got=(signed(u.reg_read(UC_X86_REG_EBP)),signed(u.reg_read(UC_X86_REG_EDI)),signed(u.reg_read(UC_X86_REG_ESI)),
      signed(word(0xc91e90)),signed(word(0xc91e94)),signed(word(0xc91e98)))
 checked+=1
 if tuple(int(v) for v in line)!=got:mismatch+=1
record('step_cache_0x487991',len(cases),checked,mismatch)
# 0x487370-0x4877DE ground, gravity and water update (step cache skip path).
cases=[]
for ground_bits,state_bits in ((0,0),(0x2000,0),(0,0x20000),(0,0x40000),(0,0x60000),(0x2000,0x20000),(0x2000,0x40000),(0,0x140000)):
 for height,clearance,z in ((0,0,0),(0x10000,0,0x10000),(0x10000,0,0x8000),(0xfff0000,0,0x10000),(0x10000,0x10000,0x10000),(0x10000,0x20000,0x8000),
                            (0x7fffffff,0,0x80000000),(0x80000000,0,0x7fffffff),(0,0xffffffff,0x80000000),(0x7fffffff,0xffffffff,0x7fffffff)):
  for vertical,water in ((0,0),(100,0x10000),(-100,0x20000)):
   for is_local,full in ((1,0),(1,1),(0,1)):
    cases.append((ground_bits,state_bits,height,clearance,z,vertical,water,is_local,full))
def effective_clearance(value):
 # The original imul reads the list value as a signed dword, then shrd 16.
 return signed((signed(value)*0x1d2b0)>>16&0xffffffff) if value else 0xfffffff
def ground_case_state(case):
 ground_bits,state_bits,height,clearance,z,vertical,water,is_local,full=case
 flags=ground_bits|state_bits
 setup_actor(0x4000,0x8000,z,0x1234,0,vertical,flags,clearance)
 put(0xc23820,ACTOR if is_local else 0)
 put(0xbd5248,water);state['height']=height;state['slope']=(0,0)
 put(FRAMES+0x00,0);put(FRAMES+0x08,0x1000);put(FRAMES+0x0c,0)
 return 3 if full else 1
def ground_row(case,dx,dy,dz):
 ground_bits,state_bits,height,clearance,z,vertical,water,is_local,full=case
 return (0x4000,0x8000,sg(z),0x1234,vertical,ground_bits|state_bits,0,is_local,full,sg(height),sg(water),
         effective_clearance(clearance),dx,dy,dz,0,0,0,0,0,0)
mismatch=0;checked=0
for case in cases:
 param=ground_case_state(case)
 run_frame_step(param_3=param,stop=0x487565)
 step=(signed(u.reg_read(UC_X86_REG_EBP)),signed(u.reg_read(UC_X86_REG_EDI)),signed(u.reg_read(UC_X86_REG_ESI)))
 param=ground_case_state(case)
 run_frame_step(param_3=param,stop=0x4877de)
 got=(signed(word(ACTOR+0x24)),signed(word(ACTOR+0x28)),signed(word(ACTOR+0x2c)),word(ACTOR+0x30),
      signed(word(ACTOR+0x44)),word(ACTOR+0x6c))
 ground_bits,state_bits,height,clearance,z,vertical,water,is_local,full=case
 line=probe('ground',[ground_row(case,step[0],step[1],step[2])])[0]
 checked+=1
 expected=(int(line[0]),int(line[1]),int(line[2]),int(line[3]),int(line[4]),int(line[5]))
 if expected!=got:
  mismatch+=1
  if mismatch<=4:print('ground mismatch',case,'native',line,'orig',got)
record('ground_update_0x487565',len(cases),checked,mismatch,'step read from the original 0x487565 stop, then fed to the port; clearance scan 0x4876E0 seeded')
# 0x487370-0x4877DE ground object transform (rotation path).
cases=[]
for rotation in (0,0x40000000,0x12345678,0xffffffff,0x80000000,0xc0000000):
 for gx,gy,gz,ox,oy in ((0,0,0,0,0),(0x1000,0x2000,0x3000,0x10,0x20),(0,0,0x1000,-0x40,0x80),
                        (0,0,0,0x7fffffff,0x7fffffff),(0x7fffffff,0x80000000,0,0x80000000,0x7fffffff)):
  for px,py in ((0,0),(0x10000,0x20000),(0x80000000,0x7fffffff),(0x7fffffff,0x80000000)):
   cases.append((rotation,gx,gy,gz,ox,oy,px,py))
def object_row(c,dx,dy,dz):
 rotation,gx,gy,gz,ox,oy,px,py=c
 return (sg(px),sg(py),0x5000,0,7,0,1,1,0,0x4000,0,0xfffffff,dx,dy,dz,sg(gx),sg(gy),gz,signed(rotation),sg(ox),sg(oy))
mismatch=0;checked=0
for case in cases:
 rotation,gx,gy,gz,ox,oy,px,py=case
 setup_actor(px,py,0x5000,0,0,7,7,0xfffffff);put(0xc23820,ACTOR)
 put(ACTOR+0x100,0x1500000);u.mem_write(0x1500000,bytes(0x100))
 put(0x1500000+0x24,gx);put(0x1500000+0x28,gy);put(0x1500000+0x2c,gz)
 put(0x1500000+0x90,rotation);put(0x1500000+0x3c,ox);put(0x1500000+0x40,oy)
 put(0xbd5248,0);state['height']=0x4000;state['slope']=(0,0)
 put(FRAMES+0x00,0);put(FRAMES+0x08,0);put(FRAMES+0x0c,0)
 run_frame_step(stop=0x487565)
 step=(signed(u.reg_read(UC_X86_REG_EBP)),signed(u.reg_read(UC_X86_REG_EDI)),signed(u.reg_read(UC_X86_REG_ESI)))
 setup_actor(px,py,0x5000,0,0,7,7,0xfffffff);put(0xc23820,ACTOR)
 put(ACTOR+0x100,0x1500000);u.mem_write(0x1500000,bytes(0x100))
 put(0x1500000+0x24,gx);put(0x1500000+0x28,gy);put(0x1500000+0x2c,gz)
 put(0x1500000+0x90,rotation);put(0x1500000+0x3c,ox);put(0x1500000+0x40,oy)
 put(0xbd5248,0);state['height']=0x4000;state['slope']=(0,0)
 put(FRAMES+0x00,0);put(FRAMES+0x08,0);put(FRAMES+0x0c,0)
 run_frame_step(stop=0x4877de)
 line=probe('ground',[object_row(case,step[0],step[1],step[2])])[0]
 checked+=1
 if (int(line[0]),int(line[1]),int(line[2]),int(line[3]))!=(signed(word(ACTOR+0x24)),signed(word(ACTOR+0x28)),signed(word(ACTOR+0x2c)),word(ACTOR+0x30)):
  mismatch+=1
  if mismatch<=3:print('object mismatch',case,'native',line,'orig',(signed(word(ACTOR+0x24)),signed(word(ACTOR+0x28)),signed(word(ACTOR+0x2c)),word(ACTOR+0x30)))
record('ground_object_0x487573',len(cases),checked,mismatch)
# 0x48834E-0x488969 movement animation choice and stance pulses.
cases=[]
for control in (0,1,2,4,8,0x10,0x12,0x1e,0x1f,0x20,0x40,0x60,0x400,0x41e,0x500,0x600,0x1000,0x1e|0x400):
 for ground,water,slope,step_bit in ((0,0,0,0),(0,1,0,0),(0,0,1,0),(0,0,0,1),(1,0,0,0),(1,1,1,1)):
  for prone,stance,slot in ((0,0,0),(0,0,0x40),(0,0x40,0x44),(1,0,8),(0,1,0x0c),(1,1,0x0a),(0,0,0xff)):
   for pulses in ((0,0,0,0),(0,0,0,1),(0,0,1,0),(0,1,0,0),(1,0,0,0),(1,1,1,1)):
    cases.append((control,ground,water,slope,step_bit,prone,stance,slot,pulses))
def animation_flags(ground,water,slope,step_bit):
 return (0x2000 if ground else 0)|(0x20000 if water else 0)|(0x40000 if slope else 0)|(0x80000 if step_bit else 0)
def animation_row(c,steady_value=0):
 control,ground,water,slope,step_bit,prone,stance,slot,pulses=c
 return (control,0x40000000,5,animation_flags(ground,water,slope,step_bit),0,0,prone,stance,
         pulses[0],pulses[1],pulses[2],pulses[3],slot,steady_value,0x1111)
native_animation=probe('animation',[animation_row(c) for c in cases])
mismatch=0;checked=0
for case,line in zip(cases,native_animation):
 control,ground,water,slope,step_bit,prone,stance,slot,pulses=case
 flags=animation_flags(ground,water,slope,step_bit)
 put(STRUCT,ACTOR);u.mem_write(ACTOR,bytes(0x200))
 put(0xc23a00,control);put(0xc23824,0);put(0xc23828,prone);put(0xc23830,stance)
 put(0xc23a04,pulses[2]);put(0xc23a08,pulses[1]);put(0xc23a0c,pulses[3]);put(0xc23a10,pulses[0])
 put(0xc2382c,0x1111);put(ACTOR+0x100,0)
 put(ACTOR+0x2c,0x40000000);put(ACTOR+0x44,5);put(ACTOR+0x30,0);put(ACTOR+0x34,0)
 put(ACTOR+0x6c,flags)
 sp=0x2008000;u.mem_write(sp,struct.pack('<40I',*([0]*20+[0xffffff,0,slot,0,0]+[0]*15)))
 u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ESI,STRUCT)
 state['steady']=0;state['stops']={0x4883fd,0x488969}
 u.emu_start(0x48834e,0x4000000,count=20000)
 if u.reg_read(UC_X86_REG_EIP) not in state['stops']:raise RuntimeError('Original motion block did not reach its stop: %x' % u.reg_read(UC_X86_REG_EIP))
 got=(word(ACTOR+0x7c),word(ACTOR+0x2c),word(ACTOR+0x44),word(0xc23824),word(0xc23828),word(0xc23830),
      word(0xc23a10),word(0xc23a08),word(0xc23a04),word(0xc23a0c))
 checked+=1
 if tuple(int(v) for v in line)!=got:
  mismatch+=1
  if mismatch<=5:print('anim mismatch',case,'native',line,'orig',got)
record('movement_animation_0x48834e',len(cases),checked,mismatch)
report['hooks']=['4776C0 writes the probe sine/cosine from models fixed_sine_cosine','42CE40/45DC70/43E7D0 return 0',
 '47DF40 returns the case ground height','47E5D0 returns the case slope samples','484740 returns the case steady value']
report['original_functions']=['487370-487565 frame step and slope response','487991-4879CC step cache','487565-4877DE ground, gravity, water','48834E-488969 movement animation']
expected_sections={'frame_step_0x487370','step_cache_0x487991','ground_update_0x487565','ground_object_0x487573','movement_animation_0x48834e'}
if set(report['checks'])!=expected_sections:raise SystemExit('section set changed: '+repr(sorted(report['checks'])))
report['total_mismatches']=sum(c['mismatches'] for c in report['checks'].values())
report['total_cases']=sum(c['cases'] for c in report['checks'].values())
text=json.dumps(report,indent=2);print(text)
if args.report:args.report.write_text(text+'\n')
sys.exit(1 if report['total_mismatches'] else 0)
