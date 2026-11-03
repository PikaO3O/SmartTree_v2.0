#!/usr/bin/env python3
"""Compile sequentially: the vendor's build hooks share mutable packaging files."""
from pathlib import Path
import argparse,json,subprocess,time

ROOT=Path(__file__).resolve().parents[1]
TARGETS={'A0':'stages/A0_OfficialYOLOv7/SmartTree','A1':'stages/A1_OfficialUSBPreset/SmartTree',
         'B':'stages/B_PlantOnly/SmartTree','C':'stages/C_HoldAndPresence/SmartTree',
         'D':'stages/D_LedsEventsOSD/SmartTree','E':'stages/E_IndependentUART/SmartTree','F':'SmartTree'}

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--cli',default='arduino-cli')
    p.add_argument('--config',type=Path)
    p.add_argument('--build-root',type=Path,required=True)
    p.add_argument('--targets',default='A0,A1,B,C,D,E,F')
    p.add_argument('--jobs',type=int,default=4)
    p.add_argument('--camera',default='f37',help='Match your actual sensor, e.g. f37/f53/gc2053')
    a=p.parse_args()
    validation=ROOT/'validation';validation.mkdir(exist_ok=True)
    outcomes=[]
    for name in a.targets.split(','):
        if name not in TARGETS: p.error('Unknown target '+name)
        sketch=ROOT/TARGETS[name];build=a.build_root.resolve()/name
        build.mkdir(parents=True,exist_ok=True)
        cmd=[a.cli]
        if a.config: cmd+=['--config-file',str(a.config.resolve())]
        cmd+=['compile','--fqbn',f'realtek:AmebaPro2:Ameba_AMB82-MINI:04_CameraOption={a.camera},06_LoadNNModelSource=flash,07_SHOWNNLOGS=Disable',
              '--build-path',str(build),'--jobs',str(a.jobs),str(sketch)]
        log=validation/f'compile_{name}.log'
        start=time.monotonic()
        with log.open('w',encoding='utf-8') as stream:
            code=subprocess.run(cmd,stdout=stream,stderr=subprocess.STDOUT,check=False).returncode
        outcome={'target':name,'returncode':code,'elapsed_seconds':round(time.monotonic()-start,2),
                 'command':cmd,'log':log.name}
        outcomes.append(outcome)
        print(f'{name}: exit={code} ({outcome["elapsed_seconds"]} s)',flush=True)
        if code: print(log.read_text(errors='replace')[-5000:],flush=True)
    (validation/'compile_results.json').write_text(json.dumps(outcomes,indent=2)+'\n',encoding='utf-8')
    return int(any(r['returncode'] for r in outcomes))

if __name__=='__main__': raise SystemExit(main())
