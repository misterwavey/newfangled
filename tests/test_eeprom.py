"""Two real firmware journals on one EEPROM; partition isolation and torn pages."""
import ctypes
import contextlib
import io
import runpy
import struct
import sys
from unittest.mock import patch
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class EepromPresets(unittest.TestCase):
    def test_coexistence_and_interrupted_writes(self):
        with tempfile.TemporaryDirectory(prefix='pnw-eeprom-') as directory:
            libs=[]
            common=['tests/eeprom_bridge.c','tests/oled_stub.c','firmware/eeprom_store.c','firmware/store.c','firmware/font.c','firmware/cv.c','firmware/sync.c']
            for bia in (False,True):
                out=Path(directory)/('bia.dylib' if bia else 'pnw.dylib')
                sources=['firmware/bia/settings.c','firmware/bia/ui.c','firmware/bia/engine.c'] if bia else ['firmware/settings.c','firmware/ui.c','firmware/voice.c']
                subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-pedantic','-shared','-fPIC','-Iinclude','-Ifirmware',*(['-DBIA_BUILD'] if bia else []),*common,*sources,'-o',str(out)],cwd=ROOT,check=True)
                lib=ctypes.CDLL(str(out));lib.test_device.argtypes=[ctypes.POINTER(ctypes.c_uint8)];lib.test_budget.argtypes=[ctypes.c_int,ctypes.c_uint];libs.append(lib)
            # Existing unrelated/stock data is ignored on boot; no auto-format.
            media=(ctypes.c_uint8*65536)(*([0x5a]*65536))
            for lib in libs:lib.test_device(media);self.assertEqual(lib.test_boot(),-1);self.assertTrue(lib.test_ranges())
            for which,lib in enumerate(libs):
                other=bytes(media[(1-which)*8192:(2-which)*8192]);unused=bytes(media[16384:])
                for slot in range(8):self.assertEqual(lib.test_save(slot,100+which*50+slot),0)
                self.assertEqual(bytes(media[(1-which)*8192:(2-which)*8192]),other)
                self.assertEqual(bytes(media[16384:]),unused)
            for which,lib in enumerate(libs):
                for slot in range(8):self.assertEqual(lib.test_load(slot),100+which*50+slot)
                self.assertEqual(lib.test_boot(),107+which*50)
            # Alternate firmwares through repeated compactions, preserving all slots.
            for n in range(40):
                for which,lib in enumerate(libs):self.assertEqual(lib.test_save(n%8,180+which*50+n),0)
            for which,lib in enumerate(libs):
                for slot in range(8):self.assertEqual(lib.test_load(slot),212+which*50+slot)
            for which,lib in enumerate(libs):self.assertEqual(lib.test_save(7,219+which*50),0)
            # Full banks now force replacement. Interrupted EEPROM page writes can
            # corrupt their whole target page, not merely the requested bytes.
            snapshot=bytes(media)
            for which,lib in enumerate(libs):
                for budget in list(range(0,9000,97))+[4095,4096,4097,4477,4478,4479,8191]:
                    ctypes.memmove(media,snapshot,len(snapshot));lib.test_budget(budget,1)
                    lib.test_save(0,299);lib.test_budget(-1,0)
                    self.assertIn(lib.test_load(0),(212+which*50,299))
                    for slot in range(1,8):self.assertEqual(lib.test_load(slot),212+which*50+slot)
                    other=1-which;self.assertEqual(bytes(media[other*8192:(other+1)*8192]),snapshot[other*8192:(other+1)*8192])
            ctypes.memmove(media,snapshot,len(snapshot))
            self.assertTrue(libs[1].test_clear());self.assertEqual(libs[1].test_boot(),-1)
            self.assertEqual(bytes(media[:8192]),snapshot[:8192]);self.assertEqual(libs[0].test_boot(),219)

    def test_flash_limits_and_legacy_tool_refusal(self):
        sys.path.insert(0,str(ROOT/'tools'))
        import presets as legacy
        with tempfile.TemporaryDirectory(prefix='pnw-eeprom-tools-') as directory:
            root=Path(directory)
            for identity in (b'NFEEPR01',b'BIEEPR01',b'NFPRES01'):
                for size in (24576,24577,32768,32769):
                    image=bytearray(size);struct.pack_into('<II',image,0,0x20001800,0x080000d1);image[192:200]=identity
                    filename=root/'image.bin';filename.write_bytes(image)
                    allowed=size<=(32768 if identity!=b'NFPRES01' else 24576)
                    with patch.object(sys,'argv',['flash.py','--write','--image',str(filename)]),patch('shutil.which',return_value='dfu-util'),patch('subprocess.call',return_value=0) as call,contextlib.redirect_stdout(io.StringIO()),contextlib.redirect_stderr(io.StringIO()):
                        with self.assertRaises(SystemExit) as exit:
                            runpy.run_path(str(ROOT/'tools/flash.py'),run_name='__main__')
                        self.assertEqual(exit.exception.code,0 if allowed else 2)
                        self.assertEqual(call.call_count,int(allowed))
            for identity,bia in ((b'NFEEPR01',False),(b'BIEEPR01',True)):
                calls=[]
                def transfer(command,check):
                    calls.append(command)
                    self.assertEqual(command[command.index('-s')+1],'0x080000c0:16')
                    Path(command[-1]).write_bytes(identity+struct.pack('<II',8192 if bia else 0,8192))
                args=['presets.py','--serial','TEST','--export',str(root/'backup.json')]+(['--bia'] if bia else [])
                with patch.object(sys,'argv',args),patch.object(legacy.shutil,'which',return_value='dfu-util'),patch.object(legacy.subprocess,'run',side_effect=transfer):
                    with self.assertRaisesRegex(ValueError,'external EEPROM'):legacy.main()
                self.assertEqual(len(calls),1)
