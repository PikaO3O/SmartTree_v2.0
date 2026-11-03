#!/usr/bin/env python3
"""Run host behavior tests and check packaged stage/source consistency."""
from pathlib import Path
import argparse,subprocess,tempfile,json,hashlib

ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
    results=[]
    with tempfile.TemporaryDirectory(prefix='smarttree-check-') as tmp:
        for name in ['legacy_monitor','v2']:
            exe=Path(tmp)/name
            cmd=[a.cxx,'-std=c++11','-Wall','-Wextra','-Werror',str(ROOT/'tests'/f'test_{name}.cpp'),'-o',str(exe)]
            subprocess.run(cmd,check=True)
            r=subprocess.run([str(exe)],check=True,text=True,capture_output=True)
            results.append(r.stdout.strip());print(r.stdout.strip())
    subprocess.run(['python3','-m','unittest','discover','-s',str(ROOT/'tests'),'-p','test_*.py','-v'],check=True)
    stages={'B_PlantOnly':2,'C_HoldAndPresence':3,'D_LedsEventsOSD':4,'E_IndependentUART':5,'F_Integrated':6}
    for name,value in stages.items():
        dest=ROOT/'stages'/name/'SmartTree'
        for source in (ROOT/'SmartTree').iterdir():
            if source.suffix not in {'.h','.cpp','.ino'}:continue
            expected=source.read_text()
            if source.name=='SmartTreeConfig.h':expected=f'#define SMARTTREE_STAGE {value}\n'+expected
            assert (dest/source.name).read_text()==expected,(name,source.name,'stage out of date')
    app=(ROOT/'SmartTree/SmartTreeApp.cpp').read_text()
    assert 'camera.videostream_status(' not in app
    assert 'usb.begin(' not in app
    assert 'xQueueSend(samples,&s,0)' in app
    for source in (ROOT/'SmartTree').iterdir():
        if source.suffix in {'.h','.cpp','.ino'}:
            assert 'TODO' not in source.read_text(),source.name
    ref=ROOT/'references/official_4.0.9'
    for entry in json.loads((ref/'SHA256.json').read_text()):
        assert hashlib.sha256((ref/entry['path']).read_bytes()).hexdigest()==entry['sha256'],entry['path']
    print('PASS: 5 stage copies, reference checksums, no legacy polling/begin replacement, bounded callback queue.')

if __name__=='__main__':main()
