"""Convert the authored albedo to an embedded 512px intensity resource.

This step only resamples and encodes pixels; it does not generate or paint art.
Runtime expands the monochrome data to RGBA and applies the species palette.
"""
from pathlib import Path
from PIL import Image
import hashlib
import json

root=Path(__file__).resolve().parent
source=root/'fur-albedo.png'
pixels=Image.open(source).convert('L').resize((512,512),Image.Resampling.LANCZOS).tobytes()
dest=root.parents[2]/'soh/soh/Enhancements/arpg/WolfosFurData.h'
lines=['#pragma once','#include <cstdint>','namespace ARPG::WolfosFurData {',
       'inline constexpr int Width=512,Height=512;', 'inline constexpr uint8_t Pixels[]={']
lines.extend(','.join(map(str,pixels[i:i+32]))+',' for i in range(0,len(pixels),32))
lines.extend(['};','}'])
dest.write_text('\n'.join(lines)+'\n')
(root/'texture-report.json').write_text(json.dumps({'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
    'pixels_sha256':hashlib.sha256(pixels).hexdigest(),'width':512,'height':512,'encoding':'I8 expanded to RGBA8 at runtime',
    'logical_tile':[32,32],'uv':'authored per-corner UV; 5 fractional bits'},indent=2)+'\n')
