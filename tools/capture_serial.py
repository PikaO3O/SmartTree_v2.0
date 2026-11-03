#!/usr/bin/env python3
"""Optional PC collector. Dependency: python -m pip install pyserial."""
from pathlib import Path
import argparse,csv,json,time

def parse_sample(text):
    fields=next(csv.reader([text]))
    if len(fields)!=8 or fields[0]!='SAMPLE': return None
    try:
        return [int(fields[1]),int(fields[2]),int(fields[3]),int(fields[4]),fields[5],fields[6],int(fields[7])]
    except ValueError:
        return None

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--port',required=True,help='e.g. COM5')
    p.add_argument('--seconds',type=float,default=120)
    p.add_argument('--label',choices=['present','absent'],required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    if a.seconds<=0: p.error('--seconds must be positive')
    try: import serial
    except ImportError: p.error('Install pyserial: python -m pip install pyserial')
    a.output.parent.mkdir(parents=True,exist_ok=True)
    rows=[];raw=[];started=time.monotonic()
    with serial.Serial(a.port,115200,timeout=0.2) as port:
        port.write(b'LOG OFF\nSAMPLES OFF\nSTATUS\n')
        # Allow status output; this collector does not open or configure UVC.
        until=time.monotonic()+1
        while time.monotonic()<until:
            raw.append(port.readline().decode('utf-8',errors='replace').strip())
        port.write(b'STATS RESET\nSAMPLES ON\n')
        started=time.monotonic()
        try:
            while time.monotonic()-started<a.seconds:
                text=port.readline().decode('utf-8',errors='replace').strip()
                if not text: continue
                raw.append(text)
                row=parse_sample(text)
                if row is not None: rows.append(row)
        finally:
            port.write(b'SAMPLES OFF\nSTATS\nSTATUS\n')
            until=time.monotonic()+2
            while time.monotonic()<until:
                text=port.readline().decode('utf-8',errors='replace').strip()
                if text: raw.append(text)
    with a.output.open('w',newline='',encoding='utf-8-sig') as f:
        writer=csv.writer(f);writer.writerow(['sequence','time_ms','plant_count','score','box','state','epoch','ground_truth'])
        writer.writerows(r+[a.label] for r in rows)
    a.output.with_suffix('.log').write_text('\n'.join(raw)+'\n',encoding='utf-8')
    hits=sum(r[2]>0 for r in rows)
    gaps=sum(((rows[i][0]-rows[i-1][0])&0xffffffff)!=1 for i in range(1,len(rows)))
    result={'logged_samples':len(rows),'logged_hits':hits,'sequence_gaps':gaps,
            'ground_truth':a.label,'logged_raw_rate':hits/len(rows) if rows else None,
            'warning':'Check STATS queue_drops and raw_samples in .log. No samples means no valid measurement.'}
    a.output.with_suffix('.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,ensure_ascii=False,indent=2))

if __name__=='__main__': main()
