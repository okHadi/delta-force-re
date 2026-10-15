"""Check ported movement leaves against the original DF1 instructions in Df.exe."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
import pefile,unicorn
from unicorn.x86_const import *

p=argparse.ArgumentParser(description=__doc__);p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path);args=p.parse_args()
raw=(args.game/'Df.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
if sha!='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff':raise SystemExit('Unsupported original executable hash')
pe=pefile.PE(data=raw);u=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
for at,size in [(0x400000,0xa00000),(0x1000000,0x200000),(0x2000000,0x10000),(0x3000000,0x100000),(0x4000000,0x1000)]:u.mem_map(at,size)
for s in pe.sections:
 if s.PointerToRawData and not s.Characteristics&0x80:u.mem_write(0x400000+s.VirtualAddress,raw[s.PointerToRawData:s.PointerToRawData+s.SizeOfRawData])
def put(at,v):u.mem_write(at,struct.pack('<I',v&0xffffffff))
def word(at):return struct.unpack('<I',u.mem_read(at,4))[0]
def signed(v):return v if v<0x80000000 else v-0x100000000
PROBE=str(args.probe.resolve())
def probe(mode,rows):
 result=[line.split() for line in subprocess.check_output([PROBE,mode],input=''.join(' '.join(map(str,row))+'\n' for row in rows).encode()).decode().splitlines()]
 if len(result)!=len(rows):raise ValueError(f'{mode}: probe output count differs')
 return result
ACTOR=0x1000000;STRUCT=0xc23820
queue=[];steady=[0]
STOPS=(0x4883fd,0x48895a,0x488969,0x47964a,0x478ef1,0x478f25,0x478f44,0x47824b,0x487674)
def run(start,end,count):
 u.emu_start(start,end,count=count)
 if u.reg_read(UC_X86_REG_EIP) not in (*STOPS,end):raise ValueError(f'Original block did not finish: {start:x}')
def hook(machine,at,size,user):
 if at in STOPS:u.emu_stop();return
 if at==0x497d20: # mouse delta reader: axes are preset
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp));return
 if at==0x484740: # steady predicate
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,steady[0]);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,word(sp));return
 if at==0x479fa0: # action queue push
  sp=u.reg_read(UC_X86_REG_ESP);queue.append((word(sp+4),word(sp+8),word(sp+12),word(sp+16)))
  u.reg_write(UC_X86_REG_ESP,sp+20);u.reg_write(UC_X86_REG_EIP,word(sp));return
u.hook_add(unicorn.UC_HOOK_CODE,hook)
report={'source_sha256':sha,'probe':str(args.probe.resolve()),'checks':{}}
# 0x479630-0x4796CE: mouse axes scale and push the abs look actions.
cases=[]
for ax,ay in ((0,0),(0,7),(3,0),(-3,0),(0,-4),(1,1),(-1,-1),(255,-255),(32767,-32767),(0x40000000,0x40000000),(0x12345678,-0x12345678)):
 for sens in (0,1,128,255,1024):
  for flip in (0,1):
   for st in (0,1):cases.append((ax,ay,sens,flip,st))
native=probe('mouse_look',cases);mismatch=0
for (ax,ay,sens,flip,st),line in zip(cases,native):
 queue.clear();steady[0]=st
 u.mem_write(0xcc0884,struct.pack('<I',1));put(0x503130,0);put(0xbd4848,0)
 put(0x67f4fc,flip);put(0x67f500,sens);put(0xd0934c,ax);put(0xd09350,ay)
 u.reg_write(UC_X86_REG_ESP,0x2008000);u.mem_write(0x2008000,struct.pack('<2I',0x4000000,0))
 run(0x47964f,0x4000000,20000)
 turn=look=0
 for action,tick,value,extra in queue:
  if action==0x9e:turn=value
  elif action==0x9d:turn=signed(-value)
  elif action==0x9b:look=value
  elif action==0x9c:look=signed(-value)
 if (int(line[0]),int(line[1]))!=(turn,look):mismatch+=1
report['checks']['mouse_look_0x479630']={'cases':len(cases),'mismatches':mismatch,'actions':['9b look_up_abs','9c look_dn_abs','9d turn_left_abs','9e turn_right_abs']}
# 0x488379-0x4883FD: keyboard turn and look step with the pitch clamp.
cases=[]
for f0 in (0,1,0x20,0x40,0x21,0x60,0x41,0xff):
 for f1 in (0,1,2,3,4,6):
  for yaw,pitch in ((0,0),(0x12345678,0x40000000),(0xffffffff,0xd0000000),(0x80000000,0x80000001),(0,0xc0000000),(0x7fffffff,0x10000000),(0xabcdef01,0x3fffffff)):cases.append((f0,f1,yaw,pitch))
native=probe('look_step',cases);mismatch=0
for (f0,f1,yaw,pitch),line in zip(cases,native):
 put(STRUCT,ACTOR);u.mem_write(ACTOR,bytes(0x120))
 u.mem_write(STRUCT+0x1e0,struct.pack('<2B',f0,f1));put(ACTOR+0x30,yaw);put(ACTOR+0x34,pitch)
 sp=0x2008000;u.mem_write(sp,struct.pack('<40I',*([0]*20+[0xffffff]+[0]*19)))
 u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ESI,STRUCT)
 run(0x488379,0x4000000,2000)
 got=(word(ACTOR+0x30),word(ACTOR+0x34))
 if (int(line[0]),int(line[1]))!=got:mismatch+=1
report['checks']['look_step_0x488379']={'cases':len(cases),'mismatches':mismatch}
# 0x478EE2/0x478F35/0x478F16/0x47823C: absolute look actions.
cases=[]
for action in (0x9b,0x9c,0x9d,0x9e,0x9f,0x00):
 for value in (0,1,0x1000,0xffffff,0x7fffffff,-1,-0x100000):
  for yaw,pitch in ((0,0),(0x11111111,0x22222222),(0x80000000,0x40000001)):cases.append((action,value,yaw,pitch))
native=probe('look_action',cases);mismatch=0
for (action,value,yaw,pitch),line in zip(cases,native):
 put(STRUCT,ACTOR);u.mem_write(ACTOR,bytes(0x120));put(ACTOR+0x30,yaw);put(ACTOR+0x34,pitch)
 u.reg_write(UC_X86_REG_ESP,0x2008000);u.reg_write(UC_X86_REG_ECX,value&0xffffffff)
 blocks={0x9b:(0x478ee2,0x478ef1),0x9c:(0x47823c,0x47824b),0x9d:(0x478f16,0x478f25),0x9e:(0x478f35,0x478f44)}
 if action in blocks:run(blocks[action][0],blocks[action][1],100)
 got=(word(ACTOR+0x30),word(ACTOR+0x34))
 if (int(line[0]),int(line[1]))!=got:mismatch+=1
report['checks']['look_actions_0x478']={'cases':len(cases),'mismatches':mismatch}
# 0x487658-0x487671: position step with the 0x44 vertical value.
cases=[]
for x,y,z,vertical in ((0,0,0,0),(1,2,3,4),(0x7fffffff,0xffffff00,0x80000000,0xffffffff),(0x40000000,0x40000000,0x1000,0x1000)):
 for dx,dy,dz in ((0,0,0),(1,0,0),(0,-1,0),(0x100,0x200,-0x100),(0x7fffffff,0x80000000,0xffffffff)):cases.append((x,y,z,vertical,dx,dy,dz))
cases=[tuple(signed(v&0xffffffff) for v in row) for row in cases]
native=probe('step',cases);mismatch=0
for (x,y,z,vertical,dx,dy,dz),line in zip(cases,native):
 u.mem_write(ACTOR,bytes(0x120));put(ACTOR+0x24,x);put(ACTOR+0x28,y);put(ACTOR+0x2c,z);put(ACTOR+0x44,vertical)
 u.mem_write(0x2008000,bytes(0x100))
 u.reg_write(UC_X86_REG_ESP,0x2008000);u.reg_write(UC_X86_REG_EBX,ACTOR)
 u.reg_write(UC_X86_REG_EBP,dx&0xffffffff);u.reg_write(UC_X86_REG_EDI,dy&0xffffffff);u.reg_write(UC_X86_REG_ESI,dz&0xffffffff)
 run(0x487658,0x487674,100)
 got=tuple(signed(word(ACTOR+o)) for o in (0x24,0x28,0x2c,0x44))
 if tuple(int(v) for v in line)!=got:mismatch+=1
report['checks']['step_0x487658']={'cases':len(cases),'mismatches':mismatch}
# 0x4884FA-0x488553: movement bits through the 0x4881BC jump table.
cases=[]
for bits in range(16):
 for no_run in (0,1):
  for actor_bit in (0,1):cases.append((bits<<1|no_run,actor_bit))
native=probe('move_dir',cases);mismatch=0
for (f0,actor_bit),line in zip(cases,native):
 put(STRUCT,ACTOR);u.mem_write(ACTOR,bytes(0x120))
 u.mem_write(ACTOR+0x6e,struct.pack('<B',8 if actor_bit else 0))
 u.mem_write(STRUCT+0x1e0,struct.pack('<2B',f0&0xff,0))
 u.reg_write(UC_X86_REG_ESP,0x2008000);u.reg_write(UC_X86_REG_ESI,STRUCT)
 run(0x4884fa,0x48855e,500)
 got=(u.reg_read(UC_X86_REG_EAX),u.reg_read(UC_X86_REG_EDI))
 if (int(line[0]),int(line[1]))!=got:mismatch+=1
report['checks']['move_direction_0x4884fa']={'cases':len(cases),'mismatches':mismatch}
report['original_functions']=['479630-4796CE','488379-4883FD','4883CB-48894B pitch clamp','478EE2','478F35','478F16','47823C','487658-487671','4884FA-488553','4881BC jump table']
report['hooks']=['497D20 returns 0 (axes preset)','484740 returns the asked steady value','479FA0 records the pushed action']
report['total_mismatches']=sum(c['mismatches'] for c in report['checks'].values())
text=json.dumps(report,indent=2);print(text)
if args.report:args.report.write_text(text+'\n')
if report['total_mismatches']:raise SystemExit(1)
