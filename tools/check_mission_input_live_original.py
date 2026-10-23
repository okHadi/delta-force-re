#!/usr/bin/env python3
"""Compare the live flag/effect adapter with original 4790A0."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
from check_input_state_original import Original,TYPE_FLAG,DIGIT_FLAG,ENTER_FLAG
from check_projectiles_original import SOURCE_SHA256

def main():
    p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
    rng=random.Random(4790);rows=[]
    for branch in range(7):
        for key in [ord('Y'),ord('N'),*range(48,58),0x21,0x22,0x20,13,ord('Q')]:
            for value in [1,0x100,0x80000000,0xffffffff]:
                flags=[0]*6
                if branch<4:flags[branch]=value
                elif branch==4:flags[3:]=[value,value,value]
                elif branch==5:flags[4]=value
                rows.append([key,*flags,4 if branch==5 else 0])
    for _ in range(200):rows.append([rng.randrange(256),*[rng.choice([0,0,1,256,0x80000000])for _ in range(6)],rng.randrange(256)])
    raw=(a.game/'Df.exe').read_bytes()
    if hashlib.sha256(raw).hexdigest()!=SOURCE_SHA256:raise SystemExit('Unsupported original executable')
    original=Original(raw);expected=[]
    for key,t,d,pag,m,h,h2,actor in rows:
        result=original.filter_key(key,[t,d,pag,m,h,h2,ord('Y'),ord('N'),actor])
        result=(*result,original._read_u32(TYPE_FLAG),original._read_u32(DIGIT_FLAG),original._read_u32(ENTER_FLAG))
        expected.append(':'.join(map(str,result)))
    result=subprocess.run([str(a.probe.resolve())],input=''.join(' '.join(map(str,row))+'\n'for row in rows),text=True,capture_output=True,check=True)
    actual=result.stdout.splitlines();bad=[{'row':rows[i],'original':x,'native':y}for i,(x,y)in enumerate(zip(expected,actual))if x!=y]
    report={'source_sha256':SOURCE_SHA256,'cases':len(rows),'differences':len(bad)+abs(len(expected)-len(actual)),'mismatches':bad[:10]}
    a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k]for k in ['cases','differences']}));return bool(report['differences'])
if __name__=='__main__':raise SystemExit(main())
