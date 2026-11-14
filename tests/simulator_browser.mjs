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
const local=new Map(),localStorage={getItem:k=>local.get(k)??null,setItem:(k,v)=>local.set(k,v)};
const elements=new Map(),document={body:new Element('body'),activeElement:new Element('body'),listeners:{},getElementById(id){if(!elements.has(id))elements.set(id,new Element());return elements.get(id);},createElement:tag=>new Element(tag),addEventListener(name,callback){this.listeners[name]=callback;}};
const $=id=>document.getElementById(id);const frames=[];
$('speed').value='1';const window={};
const html=fs.readFileSync('build/simulator/index.html','utf8');
assert(!html.includes('__WASM_BASE64__'));assert(!html.includes('src="http'));assert(!html.includes('<link'));
const script=html.match(/<script>([\s\S]*)<\/script>/)[1];
vm.runInNewContext(script,{localStorage,document,window,WebAssembly,Uint8Array,Uint8ClampedArray,atob,performance:{now:()=>0},requestAnimationFrame:callback=>frames.push(callback),setTimeout,URL:{createObjectURL:()=> 'blob:test',revokeObjectURL(){}},Blob,console});
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

// Preset operations use hardware gestures; Save while armed is refused.
// Start with a fresh UI while preserving the empty library.
$('reset').click();
$('encoder').click();$('right').click();$('encoder').click();assert.equal(sim.state().bpm,121);
$('back').click();$('left').click();$('encoder').click();assert.equal(sim.core.sim_get(35),1);
$('right').click();$('start').click();$('encoder').click();assert.equal(sim.core.sim_get(38),5);assert.equal(sim.core.sim_get(39),0);
$('start').click();$('encoder').click();assert.equal(sim.core.sim_get(38),2);assert.equal(sim.core.sim_get(39),1);
const saved=plain(sim.exportPresets());assert.equal(saved.slots[0].patch.tempo_bpm,121);assert.equal(saved.boot_slot,1);
assert.equal(JSON.parse(local.get('newfangled-presets-v1')).slots[0].patch.tempo_bpm,121);
$('back').click();$('encoder').click();$('right').click();$('encoder').click();assert.equal(sim.state().bpm,122);
$('reset').click();assert.equal(sim.state().bpm,121);assert.equal(sim.state().armed,false);
// JSON round-trips all CV routes/calibration and slot identities.
const imported=plain(saved);imported.boot_slot=8;imported.slots[0].slot=8;imported.slots[0].patch.tempo_bpm=179;
imported.slots[0].patch.calibration[1]={zero:2000,positive_5v:120};imported.slots[0].patch.outputs[7].cv.offset={source:'CV2',depth:-79,bias:51};
sim.importPresets(imported);assert.equal(sim.state().bpm,179);assert.equal(sim.core.sim_get(39),129);
assert.equal(sim.core.sim_cv_route(7,5,0),2);assert.equal(sim.core.sim_cv_route(7,5,1)|0,-79);
const backed=plain(sim.exportPresets());assert.deepEqual(backed.slots[1],imported.slots[0]);
const replayed=plain(sim.session());sim.replay(replayed);assert.deepEqual(plain(sim.exportPresets()),backed);
const invalid=plain(imported);invalid.slots[0].patch.tempo_bpm=70000;
assert.throws(()=>sim.importPresets(invalid));assert.deepEqual(plain(sim.exportPresets()),backed);
$('start').click();assert.throws(()=>sim.importPresets(imported));$('start').click();
// Load SLOT 1 without changing boot SLOT 8. Holding exits directly.
$('back').click();$('left').click();$('encoder').click();$('encoder').click();
for(let i=0;i<7;i++)$('left').click();$('encoder').click();$('right').click();$('right').click();$('encoder').click();
assert.equal(sim.core.sim_get(38),3);assert.equal(sim.state().bpm,121);assert.equal(sim.state().armed,false);
$('reset').click();assert.equal(sim.state().bpm,179);
console.log('Presets: stopped-only Save/Load, power-cycle, JSON validation, CV/calibration, session replay and local persistence passed.');
// Euclidean/probability/loop editing uses the same output menu as hardware.
$('right').click();$('encoder').click();
function setRhythmField(field,delta){while(sim.core.sim_get(18)!==field)$('right').click();$('encoder').click();for(let i=0;i<Math.abs(delta);i++)$(delta>0?'right':'left').click();$('encoder').click();}
setRhythmField(8,7);setRhythmField(9,2);setRhythmField(10,1);setRhythmField(11,-50);setRhythmField(12,1);
assert.deepEqual(Array.from({length:5},(_,i)=>sim.core.sim_rhythm(0,i)),[8,3,1,50,1]);assert.equal(sim.core.sim_rhythm(0,7),0x92);
$('back').click();$('start').click();assert.equal(sim.core.sim_output(0),0);sim.perform({type:'advance',ms:500});
const decision=sim.core.sim_rhythm(0,6);sim.perform({type:'advance',ms:Math.ceil(8*60000/sim.state().bpm)});
assert.equal(sim.core.sim_rhythm(0,5),1);assert.equal(sim.core.sim_rhythm(0,6),decision);$('start').click();
$('left').click();$('back').click();$('left').click();$('encoder').click();$('right').click();$('encoder').click();assert.equal(sim.core.sim_get(38),2);
$('reset').click();assert.deepEqual(Array.from({length:5},(_,i)=>sim.core.sim_rhythm(0,i)),[8,3,1,50,1]);
const rhythmBackup=plain(sim.exportPresets());assert.equal(rhythmBackup.version,4);assert.equal(rhythmBackup.slots[1].patch.outputs[0].rhythm.hits,3);
sim.replay(plain(sim.session()));assert.deepEqual(plain(sim.exportPresets()),rhythmBackup);
// Old JSON imports retain pre-rhythm behavior without changing CV calibration.
const legacy=plain(saved);legacy.version=1;legacy.slots.forEach(s=>s.patch.outputs.forEach(o=>delete o.rhythm));
sim.importPresets(legacy);assert.deepEqual(Array.from({length:5},(_,i)=>sim.core.sim_rhythm(0,i)),[1,1,0,100,0]);
console.log('Euclidean rhythm UI, probability loop, preset persistence and legacy JSON upgrade passed.');
// Swing/ratchets share UI, deterministic traces, presets and migration.
$('right').click();$('encoder').click();setRhythmField(13,33);setRhythmField(14,3);
assert.equal(sim.core.sim_groove(0,0),33);assert.equal(sim.core.sim_groove(0,1),4);assert.equal(sim.core.sim_groove(0,2),4);
$('back').click();$('start').click();sim.perform({type:'advance',ms:3000});
const swungSession=plain(sim.session()),swungState=plain(sim.state());sim.replay(swungSession);assert.deepEqual(plain(sim.state()),swungState);assert.equal(sim.core.sim_groove(0,0),33);
$('start').click();$('left').click();$('back').click();$('left').click();$('encoder').click();$('right').click();$('encoder').click();assert.equal(sim.core.sim_get(38),2);
$('reset').click();assert.equal(sim.core.sim_groove(0,0),33);assert.equal(sim.core.sim_groove(0,1),4);
const grooveBackup=plain(sim.exportPresets());assert.deepEqual(grooveBackup.slots[0].patch.outputs[0].groove,{swing:33,ratchet:4});
const badGroove=plain(grooveBackup);badGroove.slots[0].patch.outputs[0].groove.ratchet=9;
assert.throws(()=>sim.importPresets(badGroove));assert.deepEqual(plain(sim.exportPresets()),grooveBackup);
const v5=plain(rhythmBackup);v5.version=2;v5.slots.forEach(s=>s.patch.outputs.forEach(o=>delete o.groove));sim.importPresets(v5);
assert.equal(sim.core.sim_groove(0,0),0);assert.equal(sim.core.sim_groove(0,1),1);assert.equal(sim.core.sim_rhythm(0,0),8);
console.log('Swing/ratchets: controls, trace replay, presets, invalid input and v0.5 migration passed.');
// Musical RANDOM output, glide, persistence and old-preset neutrality.
$('right').click();$('encoder').click();setRhythmField(1,4);setRhythmField(15,10);setRhythmField(16,2);setRhythmField(17,1);setRhythmField(18,1);
assert.deepEqual(Array.from({length:4},(_,i)=>sim.core.sim_pitch(0,i)),[2,1,2,200]);
// Make all steps active so notes and glide are observable in this fixture.
setRhythmField(9,5);setRhythmField(10,-1);setRhythmField(11,50);$('back').click();$('start').click();
sim.perform({type:'advance',ms:500});assert(sim.core.sim_pitch(0,4)!==255);assert(sim.core.sim_pitch(0,5)>0);
const pitchSession=plain(sim.session()),pitchState=plain(sim.state());sim.replay(pitchSession);assert.deepEqual(plain(sim.state()),pitchState);
const milliseconds=sim.core.sim_get(0),remaining=sim.core.sim_pitch(0,5);$('export-presets').click();assert.equal(sim.core.sim_get(0),milliseconds);assert.equal(sim.core.sim_pitch(0,5),remaining);
$('start').click();assert(sim.state().outputs.every(v=>v===0));assert.equal(sim.core.sim_pitch(0,4),255);
$('left').click();$('back').click();$('left').click();$('encoder').click();$('right').click();$('encoder').click();assert.equal(sim.core.sim_get(38),2);
$('reset').click();assert.deepEqual(Array.from({length:4},(_,i)=>sim.core.sim_pitch(0,i)),[2,1,2,200]);
const pitchBackup=plain(sim.exportPresets());assert.equal(pitchBackup.version,4);assert.deepEqual(pitchBackup.slots[1].patch.outputs[0].pitch,{scale:'MAJOR',root:'C#',octaves:2,slew_ms:200});
const invalidPitch=plain(pitchBackup);invalidPitch.slots[1].patch.outputs[0].pitch.slew_ms=21;
assert.throws(()=>sim.importPresets(invalidPitch));assert.deepEqual(plain(sim.exportPresets()),pitchBackup);
const oldV6=plain(pitchBackup);oldV6.version=3;oldV6.slots.forEach(s=>s.patch.outputs.forEach(o=>delete o.pitch));sim.importPresets(oldV6);
assert.deepEqual(Array.from({length:4},(_,i)=>sim.core.sim_pitch(0,i)),[0,0,1,0]);
console.log('Pitch/slew controls, target notes, glide, Stop, preset replay and v0.6 migration passed.');
