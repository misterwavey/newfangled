#!/usr/bin/env python3
"""Inspect DFU, or explicitly install a built image. Does not alter option bytes."""
import argparse
from pathlib import Path
import shutil
import struct
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--image',type=Path)
p.add_argument('--write',action='store_true',help='Explicitly permit flash replacement; may clear saved settings')
p.add_argument('--serial',help='Select one DFU device by serial number')
a=p.parse_args()
exe=shutil.which('dfu-util')
if not exe:p.error('dfu-util is not installed')
selection=['-d','0483:df11']
if a.serial:selection+=['-S',a.serial]
if not a.write:
    raise SystemExit(subprocess.call([exe,*selection,'-l']))
if not a.image:p.error('--write requires --image')
data=a.image.read_bytes()
if len(data)<192 or len(data)>32768:p.error('image size outside target range')
sp,reset=struct.unpack_from('<II',data)
if not (0x20000000<sp<=0x20001800 and sp%8==0 and reset&1 and 0x08000000<=(reset&~1)<0x08000000+len(data)):
    p.error('image vectors do not match SDK target')
print('Writing custom firmware. Eurorack power and expanders must be disconnected; USB only.',flush=True)
raise SystemExit(subprocess.call([exe,*selection,'-a','0','--dfuse-address','0x08000000','-D',str(a.image)]))
