/* Execute the actual offline page script against a minimal Canvas/DOM adapter.
 * Tests application interactions, not browser layout or real GPU rendering. */
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
class Element {
  constructor(tag='div') {this.tagName=tag.toUpperCase();this.children=[];this.style={};this.dataset={};this.value='';this.checked=false;this.textContent='';this.listeners={};this.classes=new Set();this.classList={toggle:(name,on)=>{if(on)this.classes.add(name);else this.classes.delete(name);}};this.nodes={};this.width=480;this.height=88;}
  appendChild(child){this.children.push(child);}
  setAttribute(name,value){this[name]=value;}
  addEventListener(name,callback){this.listeners[name]=callback;}
  click(){this.onclick?.({target:this});}
  querySelector(selector){return this.nodes[selector]??=(new Element(selector==='canvas'?'canvas':'b'));}
  getContext(){return this.context??={createImageData:(w,h)=>({data:new Uint8ClampedArray(w*h*4)}),putImageData:image=>{this.lastImage=image;},clearRect(){},beginPath(){},moveTo(){},lineTo(){},stroke(){}};}
}
const elements=new Map(),document={body:new Element('body'),activeElement:new Element('body'),listeners:{},getElementById(id){if(!elements.has(id))elements.set(id,new Element());return elements.get(id);},createElement:tag=>new Element(tag),addEventListener(name,callback){this.listeners[name]=callback;}};
const $=id=>document.getElementById(id);const frames=[];
$('speed').value='1';const window={};
const html=fs.readFileSync('build/simulator/index.html','utf8');
assert(!html.includes('__WASM_BASE64__'));assert(!html.includes('src="http'));assert(!html.includes('<link'));
const script=html.match(/<script>([\s\S]*)<\/script>/)[1];
vm.runInNewContext(script,{document,window,WebAssembly,Uint8Array,Uint8ClampedArray,atob,performance:{now:()=>0},requestAnimationFrame:callback=>frames.push(callback),setTimeout,URL:{createObjectURL:()=> 'blob:test',revokeObjectURL(){}},Blob,console});
for(let i=0;i<1000 && !document.body.dataset.ready && !document.body.dataset.error;i++)await new Promise(resolve=>setTimeout(resolve,5));
assert.equal(document.body.dataset.error,undefined);assert.equal(document.body.dataset.ready,'true');
const sim=window.pnwSimulator,plain=value=>JSON.parse(JSON.stringify(value));
assert.equal(sim.state().bpm,120);assert.equal($('jacks').children.length,8);assert.equal($('scopes').children.length,8);
assert($('oled').lastImage.data.some((v,i)=>i%4===0 && v===127));
$('encoder').click();assert.equal(sim.state().editing,true);
$('right').click();assert.equal(sim.state().bpm,121);
$('encoder').click();assert.equal(sim.state().editing,false);
$('start').click();assert.equal(sim.state().running,true);
$('left').click();assert.equal(sim.state().page,8);
$('encoder').click();assert.equal(sim.state().selecting,true);assert.equal(sim.state().editing,false);
$('right').click();assert.equal(sim.state().field,1);assert.equal(sim.state().page,8);
$('encoder').click();assert.equal(sim.state().editing,true);
$('right').click();$('encoder').click();assert.equal(sim.state().editing,false);assert.equal(sim.state().selecting,true);
$('back').click();assert.equal(sim.state().selecting,false);assert.equal(sim.state().page,8);
$('right').click();assert.equal(sim.state().page,0);
const before=sim.state().time;$('step').click();assert.equal(sim.state().time,before+1);
$('play').click();frames.shift()(16);assert(sim.state().time>before+1);$('play').click();
$('cv0').value='4095';$('cv0').oninput({target:$('cv0')});assert.equal(sim.core.sim_get(4),4095);
$('run').checked=true;$('run').onchange({target:$('run')});assert.equal(sim.core.sim_get(6),1);
$('clock').checked=true;$('clock').onchange({target:$('clock')});assert.equal(sim.core.sim_get(8),1);
$('seed').value='42';$('seed').onchange({target:$('seed')});$('random').click();
const session=plain(sim.session()),state=plain(sim.state()),pixels=Array.from($('oled').lastImage.data);
sim.replay(session);assert.deepEqual(plain(sim.state()),state);assert.deepEqual(Array.from($('oled').lastImage.data),pixels);
assert.match(session.coreHash,/^[0-9a-f]{64}$/);
sim.replay({...session,coreHash:'0'.repeat(64)});assert.match($('notice').textContent,/Different core build/);
const stable=plain(sim.state());assert.throws(()=>sim.replay({...session,core:'wrong-core'}));assert.deepEqual(plain(sim.state()),stable);
assert.throws(()=>sim.replay({...session,actions:[{type:'input',index:0,value:9999}]}));
assert.throws(()=>sim.replay({...session,actions:[{type:'advance',ms:60001}]}));
assert.throws(()=>sim.replay({...session,actions:[{type:'arbitrary'}]}));
const draft=sim.state().page;document.activeElement=$('encoder');document.activeElement.tagName='BUTTON';
document.listeners.keydown({key:'ArrowRight',preventDefault(){}});assert.equal(sim.state().page,(draft+1)%9);
document.activeElement=$('seed');document.activeElement.tagName='INPUT';$('seed').value='123';
sim.perform({type:'advance',ms:1});assert.equal($('seed').value,'123');
assert(sim.state().outputs.every(v=>v>=0 && v<=1023));
$('phase-reset').click();assert.equal(sim.core.sim_get(15),1);
$('start').click();assert.equal(sim.state().running,false);assert(sim.state().outputs.every(v=>v===0));
$('reset').click();assert.equal(sim.state().time,0);assert.equal(sim.state().seed,12345);assert.equal(sim.session().actions.length,0);
// External waiting state and Run/clock controls use the actual shared UI.
sim.perform({type:'hold-program'});sim.perform({type:'program'});sim.perform({type:'turn',direction:1});
sim.perform({type:'program'});sim.perform({type:'hold-program'});assert.equal(sim.state().external,true);
$('start').click();assert.equal(sim.state().armed,true);assert.equal(sim.state().running,false);assert.equal($('start').textContent,'DISARM');
sim.perform({type:'input',index:3,value:1});sim.perform({type:'advance',ms:1000});
assert.equal(sim.state().running,true);assert.equal($('transport-state').textContent,'RUNNING');
sim.perform({type:'input',index:3,value:0});sim.perform({type:'advance',ms:1200});
assert.equal(sim.state().running,false);assert.equal($('transport-state').textContent,'CLOCK LOST');
assert(sim.state().outputs.every(v=>v===0));
// CV routing uses only the same encoder gestures available on the module.
$('reset').click();$('right').click();$('encoder').click();
for(let i=0;i<4;i++)$('right').click();$('encoder').click();
for(let i=0;i<100;i++)$('left').click();$('encoder').click();
$('right').click();$('right').click();$('encoder').click();assert.equal(sim.state().cvMenu,true);
$('encoder').click();for(let i=0;i<4;i++)$('right').click();$('encoder').click();
$('right').click();$('encoder').click();$('right').click();$('encoder').click();$('back').click();
$('start').click();$('cv0').value='4095';$('cv0').oninput({target:$('cv0')});sim.perform({type:'advance',ms:250});
assert.equal(sim.core.sim_effective(0,4),100);
$('cv0').value='0';$('cv0').oninput({target:$('cv0')});sim.perform({type:'advance',ms:250});
assert.equal(sim.core.sim_effective(0,4),0);assert.equal(sim.core.sim_cv_route(0,4,0),1);
console.log('Offline page: controls, clock/CV inputs, pause/step, seeded replay, framebuffer and invalid-session checks passed.');
