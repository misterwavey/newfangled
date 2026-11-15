"""DFU identity/range guards and separate BIA/Newfangled preset namespaces."""
import copy
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import bia_format as fmt
import preset_format as nf
import presets as cli

def example():
    return fmt.upgrade(dict(tempo_bpm=120,scenes=[[i*100+f*10 for f in range(7)] for i in range(8)],steps=[dict(scene=i%8+1,probability=73,ratchets=3,enabled=not i%2) for i in range(16)],variation=37,cv_lead_ms=10,pulse_ms=3,swing=25,clock_source='EXT',ppqn=1,run_input='RESET',variation_cv='CV2',calibration=[dict(zero=83,positive_5v=3900),dict(zero=2100,positive_5v=127)],cv_swap=True))
class BiaTools(unittest.TestCase):
    def test_codec_and_foreign_banks(self):
        p=example();payload=fmt.encode(p);self.assertEqual(fmt.decode(payload),p)
        bank=fmt.bank_image({1:payload,8:payload},8,1,0);state=fmt.scan(bank+b'\xff'*4096);self.assertEqual(state['boot_slot'],8);self.assertEqual(fmt.validate(fmt.library(state)),state['slots'])
        self.assertTrue(nf.scan(bank+b'\xff'*4096)['future'])
        foreign=bytearray(bank);foreign[:4]=b'NFB1';struct.pack_into('<I',foreign,20,zlib.crc32(foreign[:20]));self.assertTrue(fmt.scan(foreign+b'\xff'*4096)['future'])
        for at in [0,16,20,384+16,768+16,4094]:
            broken=bytearray(bank);broken[at]^=1;self.assertIsNone(fmt.scan(broken+b'\xff'*4096)['bank'])
        for field,value in [('cv_lead_ms',0),('pulse_ms',11),('variation',101),('cv_swap',1)]:
            bad=copy.deepcopy(p);bad[field]=value
            with self.assertRaises(ValueError):fmt.encode(bad)
        with self.assertRaises(ValueError):fmt.validate(dict(format='newfangled-presets',version=6,boot_slot=None,slots=[]))
    def test_dfu_bia_identity_and_preview(self):
        with tempfile.TemporaryDirectory(prefix='bia-dfu-') as directory:
            root=Path(directory);export=root/'backup.json';payload=fmt.encode(example());media=fmt.bank_image({1:payload},1,1,0)+b'\xff'*4096;calls=[]
            def run(command,check):
                calls.append(command)
                if '-U' in command:
                    n=int(command[command.index('-s')+1].split(':')[1]);Path(command[-1]).write_bytes((b'BIPRES01'+struct.pack('<II',fmt.BASE,8192)) if n==16 else media)
                return subprocess.CompletedProcess(command,0)
            with patch.object(sys,'argv',['presets.py','--bia','--export',str(export),'--serial','test']),patch.object(cli.shutil,'which',return_value='dfu-util'),patch.object(cli.subprocess,'run',side_effect=run):cli.main()
            lib=json.loads(export.read_text());self.assertEqual(lib['format'],'newfangled-bia-presets');self.assertEqual(lib['slots'][0]['patch'],example())
            with patch.object(sys,'argv',['presets.py','--bia','--import',str(export),'--serial','test']),patch.object(cli.shutil,'which',return_value='dfu-util'),patch.object(cli.subprocess,'run',side_effect=run):cli.main()
            self.assertFalse(any('-D' in c for c in calls))
            current=bytearray(media)
            def writable(command,check):
                calls.append(command)
                if '-U' in command:
                    n=int(command[command.index('-s')+1].split(':')[1]);Path(command[-1]).write_bytes((b'BIPRES01'+struct.pack('<II',fmt.BASE,8192)) if n==16 else current)
                elif '-D' in command:
                    address=int(command[command.index('-s')+1],16);self.assertEqual(address,fmt.BASE+4096)
                    incoming=Path(command[-1]).read_bytes();self.assertEqual(len(incoming),4096);current[4096:]=incoming
                return subprocess.CompletedProcess(command,0)
            with patch.object(sys,'argv',['presets.py','--bia','--import',str(export),'--serial','test','--write']),patch.object(cli.shutil,'which',return_value='dfu-util'),patch.object(cli.subprocess,'run',side_effect=writable),patch.object(cli,'__file__',str(root/'tools/presets.py')):cli.main()
            self.assertEqual(current[:4096],media[:4096]);self.assertEqual(fmt.scan(current)['slots'],{1:payload})

            with patch.object(sys,'argv',['presets.py','--export',str(root/'wrong.json'),'--serial','test']),patch.object(cli.shutil,'which',return_value='dfu-util'),patch.object(cli.subprocess,'run',side_effect=run):
                with self.assertRaises(ValueError):cli.main()
