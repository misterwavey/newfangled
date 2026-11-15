#!/usr/bin/env python3
"""Build Pam’s Newfangled Workout from standalone source using Clang and LLD."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import struct
from elf_symbols import function_symbols
def linker():
    configured=os.environ.get('PNW_LD')
    if configured: return configured
    found=shutil.which('ld.lld')
    if found: return found
    for candidate in (Path.home()/'.rustup/toolchains').glob('*/lib/rustlib/*/bin/rust-lld'):
        return str(candidate)
    raise SystemExit('Need LLVM ld.lld or rust-lld; set PNW_LD to its path.')

def binary_from_elf(data):
    if data[:7]!=b'\x7fELF\x01\x01\x01': raise ValueError('expected ELF32 little endian')
    phoff=struct.unpack_from('<I',data,28)[0]
    phsize,phnum=struct.unpack_from('<HH',data,42)
    segments=[]
    for i in range(phnum):
        typ,off,va,pa,size,mem,flags,align=struct.unpack_from('<IIIIIIII',data,phoff+i*phsize)
        if typ==1 and size and 0x08000000<=pa<0x08008000:
            if off+size>len(data) or pa+size>0x08008000: raise ValueError('invalid load segment')
            segments.append((pa,data[off:off+size]))
    if not segments: raise ValueError('no flash load segments')
    result=bytearray(b'\xff'*(max(a+len(b) for a,b in segments)-0x08000000))
    for a,b in segments: result[a-0x08000000:a-0x08000000+len(b)]=b
    return bytes(result)

ROOT=Path(__file__).resolve().parents[1]
SOURCES=['firmware/startup.c','firmware/board.c','firmware/eeprom_store.c','firmware/gate_bus.c','firmware/runtime.c','firmware/divmod.S',
         'firmware/protocol.c','firmware/font.c','firmware/ui.c','firmware/clock.c','firmware/app.c','firmware/settings.c','firmware/store.c','firmware/cv.c','firmware/sync.c','firmware/voice.c','firmware/main.c']
INPUTS=SOURCES+['firmware/stm32f042.ld','include/pnw_board.h','firmware/controls.h','firmware/font.h','firmware/ui.h','firmware/clock.h','firmware/app.h','firmware/settings.h','firmware/store.h','firmware/preset_format.h','firmware/cv.h','firmware/sync.h','firmware/voice.h','firmware/gate_bus.h','docs/validation.json']
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--inputs',action='store_true',help='Build zero-output physical input monitor')
    parser.add_argument('--bia',action='store_true',help='Build the separate BIA scene controller')
    args=parser.parse_args()
    if args.bia and args.inputs:parser.error('Choose --bia or --inputs')
    sources=SOURCES;inputs=INPUTS
    if args.bia:
        sources=['firmware/startup.c','firmware/board.c','firmware/eeprom_store.c','firmware/gate_bus.c','firmware/runtime.c','firmware/divmod.S','firmware/protocol.c','firmware/font.c','firmware/sync.c','firmware/cv.c','firmware/store.c','firmware/bia/engine.c','firmware/bia/ui.c','firmware/bia/settings.c','firmware/bia/main.c']
        inputs=sources+['firmware/bia/bia.h','firmware/bia/settings.h','firmware/store.h','firmware/preset_format.h','firmware/sync.h','firmware/cv.h','firmware/gate_bus.h','firmware/font.h','include/pnw_board.h','firmware/controls.h','firmware/stm32f042.ld']
    out=ROOT/('build/bia' if args.bia else 'build/newfangled-inputs' if args.inputs else 'build/newfangled');out.mkdir(parents=True,exist_ok=True)
    cc=os.environ.get('PNW_CC',shutil.which('clang') or 'clang');objects=[]
    flags=['--target=arm-none-eabi','-mcpu=cortex-m0','-mthumb','-Oz','-g','-ffreestanding','-fno-builtin','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-ffunction-sections','-fdata-sections','-Wall','-Wextra','-Werror','-Iinclude']
    if args.inputs:flags.append('-DPNW_INPUT_DIAGNOSTICS')
    if args.bia:flags.append('-DBIA_BUILD')
    for i,name in enumerate(sources):
        obj=out/f'{i}-{Path(name).stem}.o'
        subprocess.run([cc,*flags,'-c',name,'-o',str(obj)],cwd=ROOT,check=True);objects.append(str(obj))
    ld=linker();prefix=[ld]+(['-flavor','gnu'] if Path(ld).name=='rust-lld' else [])
    subprocess.run([*prefix,'-T','firmware/stm32f042.ld','--gc-sections','-Map='+str(out/'pnw.map'),'-o',str(out/'pnw.elf'),*objects],cwd=ROOT,check=True)
    elf=(out/'pnw.elf').read_bytes();data=binary_from_elf(elf);symbols=function_symbols(elf)
    if any(name.startswith(('recovered_','pnw_internal_flash_')) or name in ('pnw_oled_number','flash_operation') for name in symbols):
        raise ValueError('unexpected recovered code, internal flash writes or legacy numeral renderer linked')
    if not args.inputs and not all(name in symbols for name in ('pnw_eeprom_read','pnw_eeprom_write')):raise ValueError('EEPROM backend not linked')
    (out/'pnw.bin').write_bytes(data)
    validation=json.loads((ROOT/'docs/validation.json').read_text())
    if len(data)>32768:raise ValueError('application exceeds 32 KiB MCU flash')
    tested=not args.bia and not args.inputs and hashlib.sha256(data).hexdigest()==validation['image_sha256']
    report=dict(application=('Newfangled BIA scene controller v0.2' if args.bia else 'Newfangled physical input monitor; all outputs zero' if args.inputs else 'Pam’s Newfangled Workout v0.11: waveforms, logic, expanded CV and beat loops'),flash_bytes=len(data),application_limit_bytes=32768,preset_slots=8,preset_storage=('SPI EEPROM 0x2000..0x3fff' if args.bia else 'SPI EEPROM 0x0000..0x1fff'),
                sha256=hashlib.sha256(data).hexdigest(),hardware_tested=tested,hardware_validation=validation if tested else None,stock_binary_required=False,
                inputs={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in inputs},
                font_origin='Manually authored 5x7 row glyphs in firmware/font.c',
                reused_code='Existing independent SDK startup, board driver and runtime; not a clean-room provenance claim',
                clock_tick_hz=1000,outputs_active=[] if args.inputs else list(range(1,9)),gate_codes=[0,1023],eeprom_functions_linked=not args.inputs,legacy_numeral_renderer_linked=False)
    (out/'build.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
