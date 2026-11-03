#!/usr/bin/env python3
"""Recreate standalone diagnostic sketches from the reviewed final source."""
from pathlib import Path
import shutil

ROOT=Path(__file__).resolve().parents[1]
OFFICIAL=ROOT/'references/official_4.0.9/libraries/NeuralNetwork/examples/UVCDObjectDetectionLoop'

def main():
    for name,preset in [('A0_OfficialYOLOv7',False),('A1_OfficialUSBPreset',True)]:
        dest=ROOT/'stages'/name/'SmartTree'
        dest.mkdir(parents=True,exist_ok=True)
        source=(OFFICIAL/'UVCDObjectDetectionLoop.ino').read_text()
        source=source.replace('DEFAULT_YOLOV4TINY, NA_MODEL','DEFAULT_YOLOV7TINY, NA_MODEL')
        if preset:
            source=source.replace('VideoSetting stream_config(VIDEO_FHD, 24, VIDEO_H264, 0);',
                                  'VideoSetting stream_config(USB_UVCD_STREAM_PRESET);')
        (dest/'SmartTree.ino').write_text(source,encoding='utf-8')
        shutil.copyfile(OFFICIAL/'ObjectClassList.h',dest/'ObjectClassList.h')
    for name,stage in [('B_PlantOnly',2),('C_HoldAndPresence',3),('D_LedsEventsOSD',4),('E_IndependentUART',5),('F_Integrated',6)]:
        dest=ROOT/'stages'/name/'SmartTree'
        dest.mkdir(parents=True,exist_ok=True)
        for source in (ROOT/'SmartTree').iterdir():
            if source.suffix not in {'.ino','.h','.cpp'}: continue
            text=source.read_text()
            if source.name=='SmartTreeConfig.h': text=f'#define SMARTTREE_STAGE {stage}\n'+text
            (dest/source.name).write_text(text,encoding='utf-8')
    print('Created 7 complete, standalone stage sketches.')

if __name__=='__main__': main()
