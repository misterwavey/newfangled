#!/usr/bin/env python3
"""Export/import Newfangled preset JSON over USB DFU. Never accesses EEPROM."""
import argparse
import datetime
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
from preset_format import BASE,BANK_BYTES,STORE_BYTES,scan,library,validate,bank_image

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    commands=parser.add_mutually_exclusive_group(required=True)
    commands.add_argument('--export',dest='export_file',type=Path)
    commands.add_argument('--import',dest='import_file',type=Path)
    parser.add_argument('--serial',required=True)
    parser.add_argument('--write',action='store_true',help='Permit importing presets; default is read-only preview')
    a=parser.parse_args();exe=shutil.which('dfu-util')
    if not exe:parser.error('Install dfu-util first')
    if a.write and not a.import_file:parser.error('--write is only for import')
    selection=[exe,'-d','0483:df11','-S',a.serial,'-a','0']
    # Parse the whole JSON before touching USB; no silently truncated values.
    incoming=None
    if a.import_file:
        if a.import_file.stat().st_size>100000:parser.error('Preset JSON exceeds 100 KB')
        incoming=json.loads(a.import_file.read_text());validate(incoming)
    with tempfile.TemporaryDirectory(prefix='newfangled-presets-') as directory:
        root=Path(directory)
        def upload(address,length,name):
            path=root/name
            subprocess.run([*selection,'-s',f'0x{address:08x}:{length}','-U',str(path)],check=True)
            b=path.read_bytes()
            if len(b)!=length:raise ValueError('Incomplete DFU read')
            return b
        identity=upload(0x080000c0,16,'identity.bin')
        if identity!=b'NFPRES01'+struct.pack('<II',BASE,STORE_BYTES):
            raise ValueError('Device is not storage-enabled Newfangled. Refusing preset access.')
        before=upload(BASE,STORE_BYTES,'before.bin');state=scan(before)
        if state['future']:raise ValueError('Newer storage schema present; use matching firmware/tools')
        if a.export_file:
            # Exclusive create avoids overwriting a previous backup by mistake.
            with a.export_file.open('x') as f:json.dump(library(state),f,indent=2);f.write('\n')
            print(f'Exported {len(state["slots"])} slots to {a.export_file}');return
        merged={**state['slots'],**validate(incoming)};boot=incoming['boot_slot'] if incoming['boot_slot'] is not None else state['boot_slot']
        target=0 if state['bank'] is None else state['bank']^1
        print(f'Import merges {len(incoming["slots"])} slots; writes bank {target} only, 0x{BASE+target*BANK_BYTES:08x}..0x{BASE+(target+1)*BANK_BYTES-1:08x}.')
        if not a.write:print('Read-only preview. Add --write to import.');return
        backups=Path(__file__).resolve().parents[1]/'build/backups';backups.mkdir(parents=True,exist_ok=True)
        backup=backups/('presets-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')+'.json')
        backup.write_text(json.dumps(library(state),indent=2)+'\n');print(f'Previous library: {backup}')
        image=bank_image(merged,boot,state['generation']+1,state['sequence']);path=root/'replacement.bin';path.write_bytes(image)
        subprocess.run([*selection,'-s',f'0x{BASE+target*BANK_BYTES:08x}','-D',str(path)],check=True)
        after=upload(BASE,STORE_BYTES,'after.bin');expected=bytearray(before);expected[target*BANK_BYTES:(target+1)*BANK_BYTES]=image
        if after!=expected:raise ValueError('Readback differs, or old bank changed. Keep the JSON backup and retry.')
        result=scan(after)
        if result['slots']!=merged or result['boot_slot']!=boot:raise ValueError('Readback library validation failed')
        print('Imported and verified. Disconnect USB before reconnecting Eurorack power; boot will be stopped.')

if __name__=='__main__':
    try:main()
    except (ValueError,KeyError,IndexError,TypeError,OSError,subprocess.CalledProcessError) as e:raise SystemExit(str(e))
