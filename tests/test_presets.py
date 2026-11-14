"""Cross-language JSON/storage compatibility and read-only/guarded DFU tooling."""
import copy
import ctypes
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import preset_format as fmt
import presets as cli

def default_patch(bpm=173):
    route=dict(source='CV2',depth=-100,bias=50)
    output=dict(pitch=dict(scale='MINOR PENTATONIC',root='F#',octaves=3,slew_ms=740),groove=dict(swing=37,ratchet=7),rhythm=dict(steps=13,hits=5,rotate=11,probability=73,loop=True),ratio='X3',shape='RANDOM',width=27,phase=11,level=83,offset=5,cv={name:dict(route) for name in fmt.FIELDS})
    outputs=[copy.deepcopy(output) for _ in range(8)]
    for i in range(8):outputs[i]['muted']=bool(0xa5&(1<<i))
    for i in range(7):
        for f,name in enumerate(fmt.FIELDS):outputs[i]['cv'][name]=dict(source=fmt.SOURCES[(i+f)%3],depth=-100+(i*13+f*37)%201,bias=100-(i*17+f*29)%201)
    return dict(tempo_bpm=bpm,clock_source='EXT',ppqn=4,run_input='RESET',cv_swap=True,calibration=[dict(zero=51,positive_5v=3900),dict(zero=2000,positive_5v=120)],outputs=outputs)

class Format(unittest.TestCase):
    def test_roundtrip_and_bank_faults(self):
        p=default_patch();payload=fmt.encode(p);self.assertEqual(fmt.decode(payload),p)
        bank=fmt.bank_image({1:payload,8:fmt.encode(default_patch(178))},1,0xffffffff,0xfffffffe)
        media=bank+b'\xff'*4096;state=fmt.scan(media);self.assertEqual(state['boot_slot'],1);self.assertEqual(state['sequence'],0)
        newer=fmt.bank_image(state['slots'],8,0,0)
        state2=fmt.scan(bank+newer);self.assertEqual(state2['bank'],1);self.assertEqual(state2['boot_slot'],8)
        for at in [0,16,20,256+16,512+16,4094]:
            damaged=bytearray(newer);damaged[at]^=1
            self.assertEqual(fmt.scan(bank+damaged)['bank'],0)
        self.assertEqual(fmt.validate(fmt.library(state2)),state2['slots'])
        p['outputs'][0]['cv']['level']['depth']=-101
        with self.assertRaises(ValueError):fmt.encode(p)
        p=default_patch();p['tempo_bpm']=True
        with self.assertRaises(ValueError):fmt.encode(p)

    def test_c_loads_host_bank_and_power_cycle(self):
        with tempfile.TemporaryDirectory() as directory:
            source=Path(directory)/'check.c';image=Path(directory)/'bank.bin';exe=Path(directory)/'check'
            image.write_bytes(fmt.bank_image({3:fmt.encode(default_patch())},3,12,42)+b'\xff'*4096)
            source.write_text('''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "store.h"
static unsigned char bytes[8192];
static bool rd(void *c,unsigned a,void *p,unsigned n){(void)c;memcpy(p,bytes+a,n);return true;}
static bool er(void *c,unsigned b){(void)c;(void)b;assert(0);return false;}
static bool wr(void *c,unsigned a,const void *p,unsigned n){(void)c;(void)a;(void)p;(void)n;assert(0);return false;}
int main(int argc,char **argv){assert(argc==2 || argc==3);FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(bytes,1,8192,f)==8192);fclose(f);
NfStore s;NfStoreIo io={0,rd,er,wr};assert(nf_store_open(&s,io)==NF_STORE_OK);assert(s.last_slot==2 && s.sequence==43);
OriginalUi ui={.running=true};assert(nf_presets_boot(&s,&ui));assert(ui.bpm==173 && !ui.running && ui.external && ui.cv_swapped);
assert(ui.routes[7][5].depth==-100 && ui.calibration[1].positive==120);
if(argc==3)assert(ui.muted==0);
if(argc==2){assert(ui.muted==0xa5);assert(ui.tones[7].scale==6 && ui.tones[7].root==6 && ui.tones[7].range==3 && ui.tones[7].slew==37);assert(ui.grooves[7].swing==37 && ui.grooves[7].ratchet==7);assert(ui.rhythms[7].steps==13 && ui.rhythms[7].hits==5 && ui.rhythms[7].rotate==11 && ui.rhythms[7].probability==73 && ui.rhythms[7].loop);}
else if(argv[2][0]=='7'){assert(ui.tones[7].scale==6 && ui.grooves[7].swing==37 && ui.rhythms[7].steps==13);}
else if(argv[2][0]=='6'){assert(ui.tones[7].scale==0 && ui.tones[7].slew==0 && ui.grooves[7].swing==37 && ui.rhythms[7].steps==13);}
else if(argv[2][0]=='5'){assert(ui.grooves[7].swing==0 && ui.grooves[7].ratchet==1);assert(ui.rhythms[7].steps==13 && ui.rhythms[7].hits==5);}
else {assert(ui.grooves[7].swing==0 && ui.grooves[7].ratchet==1);assert(ui.rhythms[7].steps==1 && ui.rhythms[7].hits==1 && ui.rhythms[7].probability==100 && !ui.rhythms[7].loop);}
NfSettings p;unsigned char out[NF_SETTINGS_BYTES];nf_settings_capture(&ui,&p);assert(nf_settings_encode(&p,out));fwrite(out,1,NF_SETTINGS_BYTES,stdout);
}''')
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Ifirmware','-Iinclude',str(source),'firmware/store.c','firmware/settings.c','-o',str(exe)],cwd=ROOT,check=True)
            payload=subprocess.check_output([str(exe),str(image)]);self.assertEqual(payload,fmt.encode(default_patch()))
            old=fmt.encode(default_patch(),schema=1)
            image.write_bytes(fmt.bank_image({3:old},3,12,42)+b'\xff'*4096)
            payload=subprocess.check_output([str(exe),str(image),'legacy'])
            self.assertEqual(payload,fmt.encode(fmt.decode(old)))
            old=fmt.encode(default_patch(),schema=2)
            image.write_bytes(fmt.bank_image({3:old},3,12,42)+b'\xff'*4096)
            payload=subprocess.check_output([str(exe),str(image),'5'])
            self.assertEqual(payload,fmt.encode(fmt.decode(old)))
            old=fmt.encode(default_patch(),schema=3)
            image.write_bytes(fmt.bank_image({3:old},3,12,42)+b'\xff'*4096)
            payload=subprocess.check_output([str(exe),str(image),'6'])
            self.assertEqual(payload,fmt.encode(fmt.decode(old)))
            old=fmt.encode(default_patch(),schema=4)
            image.write_bytes(fmt.bank_image({3:old},3,12,42)+b'\xff'*4096)
            payload=subprocess.check_output([str(exe),str(image),'7'])
            self.assertEqual(payload,fmt.encode(fmt.decode(old)))

    def test_legacy_preset_upgrade(self):
        patch=default_patch()
        for output in patch['outputs']:output.pop('rhythm')
        old=fmt.encode(patch,legacy=True)[:208]
        current=fmt.decode(old)
        self.assertEqual(current['outputs'][7]['rhythm'],dict(steps=1,hits=1,rotate=0,probability=100,loop=False))
        backup=dict(format='newfangled-presets',version=1,boot_slot=3,slots=[dict(slot=3,patch=patch)])
        upgraded=fmt.validate(backup)[3];self.assertEqual(len(upgraded),230)
        self.assertEqual(fmt.decode(upgraded),current)
        state=fmt.scan(fmt.bank_image({3:old,8:upgraded},3,1,0)+b'\xff'*4096)
        self.assertFalse(state['future']);self.assertEqual(state['slots'][3],old)
        self.assertEqual(fmt.library(state)['version'],5)
        patch=default_patch()
        for o in patch['outputs']:o.pop('groove')
        v5=dict(format='newfangled-presets',version=2,boot_slot=2,slots=[dict(slot=2,patch=patch)])
        migrated=fmt.decode(fmt.validate(v5)[2]);self.assertEqual(migrated['outputs'][7]['groove'],dict(swing=0,ratchet=1));self.assertEqual(migrated['outputs'][7]['rhythm'],patch['outputs'][7]['rhythm'])

    def test_v6_and_pitch_validation(self):
        p=default_patch();p['outputs'][0]['pitch']['slew_ms']=21
        with self.assertRaises(ValueError):fmt.encode(p)
        p=default_patch();p['outputs'][0]['pitch']['root']='H'
        with self.assertRaises(ValueError):fmt.encode(p)
        p=default_patch()
        for o in p['outputs']:o.pop('pitch')
        old=dict(format='newfangled-presets',version=3,boot_slot=1,slots=[dict(slot=1,patch=p)])
        upgraded=fmt.decode(fmt.validate(old)[1])
        self.assertEqual(upgraded['outputs'][7]['pitch'],dict(scale='OFF',root='C',octaves=1,slew_ms=0))
        self.assertEqual(upgraded['outputs'][7]['groove'],p['outputs'][7]['groove'])

    def test_v7_and_mute_validation(self):
        p=default_patch();p['outputs'][0]['muted']=1
        with self.assertRaises(ValueError):fmt.encode(p)
        p=default_patch()
        for o in p['outputs']:o.pop('muted')
        old=dict(format='newfangled-presets',version=4,boot_slot=1,slots=[dict(slot=1,patch=p)])
        migrated=fmt.decode(fmt.validate(old)[1]);self.assertTrue(all(o['muted'] is False for o in migrated['outputs']))
        self.assertEqual(migrated['outputs'][7]['pitch'],p['outputs'][7]['pitch'])

    def test_dfu_preview_guard_and_merge(self):
        media=bytearray(fmt.bank_image({1:fmt.encode(default_patch())},1,1,0)+b'\xff'*4096)
        writes=[];identity=b'NFPRES01'+struct.pack('<II',fmt.BASE,8192)
        def run(command,check):
            self.assertTrue(check);self.assertEqual(command[1:6],['-d','0483:df11','-S','ABC','-a'])
            address=command[8];addr=int(address.split(':')[0],16)
            if '-U' in command:
                length=int(address.split(':')[1]);data=identity if addr==0x080000c0 else bytes(media[addr-fmt.BASE:addr-fmt.BASE+length])
                Path(command[-1]).write_bytes(data)
            else:
                self.assertIn('-D',command);writes.append(command);payload=Path(command[-1]).read_bytes();offset=addr-fmt.BASE
                self.assertIn(offset,[0,4096]);self.assertEqual(len(payload),4096);media[offset:offset+4096]=payload
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'presets.json';path.write_text(json.dumps(dict(format='newfangled-presets',version=5,boot_slot=8,slots=[dict(slot=8,patch=default_patch(180))])))
            args=['presets.py','--serial','ABC','--import',str(path)]
            with patch.object(sys,'argv',args),patch.object(cli.shutil,'which',return_value='dfu-util'),patch.object(cli.subprocess,'run',side_effect=run):cli.main()
            self.assertFalse(writes)
            # Redirect the normal backup into the temporary directory.
            fake=Path(directory)/'tools/presets.py'
            with patch.object(sys,'argv',args+['--write']),patch.object(cli,'__file__',str(fake)),patch.object(cli.shutil,'which',return_value='dfu-util'),patch.object(cli.subprocess,'run',side_effect=run):cli.main()
            self.assertEqual(len(writes),1);state=fmt.scan(media);self.assertEqual(set(state['slots']),{1,8});self.assertEqual(state['boot_slot'],8)
            self.assertEqual(len(list(Path(directory).glob('build/backups/*.json'))),1)
            identity=b'\x00'*16;writes.clear()
            with patch.object(sys,'argv',args+['--write']),patch.object(cli.shutil,'which',return_value='dfu-util'),patch.object(cli.subprocess,'run',side_effect=run):
                with self.assertRaisesRegex(ValueError,'Refusing'):cli.main()
            self.assertFalse(writes)
