import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import serve_simulator as dev
class SimulatorDevelopment(unittest.TestCase):
    def request(self,build,path):
        response=SimpleNamespace(path=path,wfile=io.BytesIO(),headers={})
        response.send_response=lambda code:setattr(response,'code',code)
        response.send_header=lambda name,value:response.headers.update({name:value})
        response.end_headers=lambda:None
        dev.handler_for(build).do_GET(response)
        return response
    def test_routes_reload_and_no_source_exposure(self):
        build=dev.DevelopmentBuild();build.page=b'<html><body>lab</body></html>';build.version='abc'
        response=self.request(build,'/?development=1')
        self.assertEqual(response.code,200);self.assertIn(b'sessionStorage',response.wfile.getvalue())
        self.assertIn(b'version="abc"',response.wfile.getvalue());self.assertEqual(response.headers['Cache-Control'],'no-store')
        response=self.request(build,'/__status');self.assertEqual(json.loads(response.wfile.getvalue())['version'],'abc')
        for path in ['/README.md','/../firmware/main.c','/tools/serve_simulator.py']:
            self.assertEqual(self.request(build,path).code,404)
    def test_failed_build_keeps_last_working_page(self):
        build=dev.DevelopmentBuild();build.page=b'last good';build.version='old'
        with patch.object(dev.subprocess,'run',return_value=SimpleNamespace(returncode=1,stdout='',stderr='compile failed')):
            self.assertFalse(build.rebuild())
        page,status=build.snapshot();self.assertEqual(page,b'last good');self.assertEqual(status['version'],'old')
        self.assertEqual(status['error'],'compile failed');self.assertFalse(status['building'])
    def test_success_publishes_complete_page_and_clears_error(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'build/simulator').mkdir(parents=True)
            data=b'<html><body>new</body></html>';(root/'build/simulator/index.html').write_bytes(data)
            build=dev.DevelopmentBuild();build.error='old error'
            with patch.object(dev,'ROOT',root),patch.object(dev.subprocess,'run',return_value=SimpleNamespace(returncode=0,stdout='',stderr='')):
                self.assertTrue(build.rebuild())
            page,status=build.snapshot();self.assertEqual(page,data);self.assertIsNone(status['error'])
            self.assertEqual(status['version'],hashlib.sha256(data).hexdigest())

    def test_bia_build_uses_separate_builder_and_injects_reload(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'build/bia-simulator').mkdir(parents=True)
            data=b'<html><body>BIA controller</body></html>';(root/'build/bia-simulator/index.html').write_bytes(data)
            build=dev.DevelopmentBuild(bia=True)
            with patch.object(dev,'ROOT',root),patch.object(dev.subprocess,'run',return_value=SimpleNamespace(returncode=0,stdout='',stderr='')) as run:
                self.assertTrue(build.rebuild());self.assertIn('build_bia_simulator.py',run.call_args[0][0][1])
            response=self.request(build,'/');self.assertIn(b'BIA controller',response.wfile.getvalue());self.assertIn(b'sessionStorage',response.wfile.getvalue())
