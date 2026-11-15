#!/usr/bin/env python3
"""Local simulator development server with rebuild, reload and session recovery."""
import argparse
import hashlib
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
import json
from pathlib import Path
import subprocess
import sys
import threading
ROOT=Path(__file__).resolve().parents[1]
RELOAD=r'''
<script>
(function(){
 const key='pnw-lab-dev-session';let version=__VERSION__;
 const wait=setInterval(()=>{
  if(!window.pnwSimulator)return;clearInterval(wait);
  try{const saved=sessionStorage.getItem(key);sessionStorage.removeItem(key);if(saved)window.pnwSimulator.replay(JSON.parse(saved));}catch(error){console.warn('Previous session could not be replayed:',error);}
 },100);
 setInterval(async()=>{
  try{
   const status=await(await fetch('/__status',{cache:'no-store'})).json();
   let banner=document.getElementById('pnw-dev-error');
   if(status.error){if(!banner){banner=document.createElement('pre');banner.id='pnw-dev-error';banner.style.cssText='position:fixed;bottom:0;left:0;right:0;max-height:35vh;overflow:auto;background:#411e19;color:#ffd8c7;padding:16px;margin:0;z-index:9999;white-space:pre-wrap';document.body.appendChild(banner);}banner.textContent='Build failed. Showing last successful build.\n'+status.error;}
   else if(banner)banner.remove();
   if(status.version!==version && !status.error){try{if(window.pnwSimulator)sessionStorage.setItem(key,JSON.stringify(window.pnwSimulator.session()));}catch(error){console.warn('Session preservation unavailable:',error);}location.reload();}
  }catch(error){/* Keep the usable page if the development server stops. */}
 },700);
})();
</script>
'''
def fingerprint():
    paths=[ROOT/'tools/build_simulator.py',ROOT/'tools/build_bia_simulator.py',ROOT/'include/pnw_board.h']
    for folder in ['firmware','simulator']:
        paths.extend(p for p in (ROOT/folder).rglob('*') if p.suffix in ['.c','.h','.js','.css','.html'])
    digest=hashlib.sha256()
    for path in sorted(paths):digest.update(str(path.relative_to(ROOT)).encode());digest.update(path.read_bytes())
    return digest.hexdigest()
class DevelopmentBuild:
    def __init__(self,bia=False):self.bia=bia;self.lock=threading.Lock();self.page=b'';self.version='';self.error=None;self.building=False
    def rebuild(self):
        with self.lock:self.building=True
        result=subprocess.run([sys.executable,str(ROOT/('tools/build_bia_simulator.py' if self.bia else 'tools/build_simulator.py'))],cwd=ROOT,capture_output=True,text=True)
        with self.lock:
            self.building=False
            if result.returncode:self.error=(result.stderr or result.stdout)[-4000:];print(self.error,flush=True)
            else:
                self.page=(ROOT/('build/bia-simulator/index.html' if self.bia else 'build/simulator/index.html')).read_bytes();self.version=hashlib.sha256(self.page).hexdigest();self.error=None
                print('Simulator ready: '+self.version[:12],flush=True)
        return result.returncode==0
    def snapshot(self):
        with self.lock:return self.page,dict(version=self.version,error=self.error,building=self.building)
def handler_for(build):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            page,status=build.snapshot();path=self.path.split('?',1)[0]
            if path=='/__status':body=json.dumps(status).encode();kind='application/json';code=200
            elif path in ['/','/index.html'] and page:
                script=RELOAD.replace('__VERSION__',json.dumps(status['version'])).encode()
                body=page.replace(b'</body>',script+b'</body>');kind='text/html; charset=utf-8';code=200
            elif path=='/favicon.ico':body=b'';kind='image/x-icon';code=204
            else:body=b'Not found';kind='text/plain';code=404
            self.send_response(code);self.send_header('Content-Type',kind);self.send_header('Cache-Control','no-store');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
        def log_message(self,*args):pass
    return Handler
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--port',type=int,default=8765);parser.add_argument('--bia',action='store_true');args=parser.parse_args()
    build=DevelopmentBuild(args.bia);seen=fingerprint()
    if not build.rebuild():raise SystemExit('Initial simulator build failed.')
    server=ThreadingHTTPServer(('127.0.0.1',args.port),handler_for(build));stop=threading.Event()
    def watch():
        nonlocal seen
        while not stop.wait(.4):
            try:
                current=fingerprint()
                if current!=seen:seen=current;build.rebuild()
            except OSError as error:print('Watch error: '+str(error),flush=True)
    threading.Thread(target=watch,daemon=True).start()
    print(f'Open http://127.0.0.1:{server.server_port}/ — watching C, font, HTML, CSS and JavaScript. Ctrl-C stops.',flush=True)
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:stop.set();server.server_close()
if __name__=='__main__':main()
