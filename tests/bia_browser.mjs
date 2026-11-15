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
const window={};
const html=fs.readFileSync('build/bia-simulator/index.html','utf8');assert(!html.includes('__WASM__'));assert(!html.includes('src="http'));
vm.runInNewContext(html.match(/<script>([\s\S]*)<\/script>/)[1],{localStorage,document,window,WebAssembly,Uint8Array,Uint8ClampedArray,atob,performance:{now:()=>0},requestAnimationFrame:cb=>frames.push(cb),setTimeout,URL:{createObjectURL:()=> 'blob:test',revokeObjectURL(){}},Blob,console});
for(let i=0;i<1000&&!document.body.dataset.ready&&!document.body.dataset.error;i++)await new Promise(r=>setTimeout(r,5));assert.equal(document.body.dataset.error,undefined);assert.equal(document.body.dataset.ready,'true');
const sim=window.biaSimulator,c=sim.core,plain=o=>JSON.parse(JSON.stringify(o));
assert.equal($('steps').children.length,16);assert.equal($('scenes').children.length,8);assert.equal($('outputs').children.length,8);assert.equal(sim.patch().tempo_bpm,120);
$('back').click();$('encoder').click();$('right').click();$('encoder').click();$('back').click();assert.equal(sim.patch().tempo_bpm,121);
$('start').click();assert.equal(c.bs_get(1),1);assert.throws(()=>sim.importPresets({format:'newfangled-bia-presets',version:1,boot_slot:null,slots:[]}));
const seedSession=plain(sim.session());sim.advance(2000);assert(c.bs_edges()>4);$('start').click();assert.equal(c.bs_get(1),0);for(let i=0;i<8;i++)assert.equal(c.bs_output(i),0);
// Exact lead/pulse oracle using integer 120 BPM, no UI time advances.
sim.edit(0,0,0,120);sim.edit(0,0,2,10);sim.edit(0,0,3,3);c.bs_audition(0);assert.equal(c.bs_get(8),1);const before=c.bs_edges();sim.advance(9);assert.equal(c.bs_edges(),before);sim.advance(1);assert.equal(c.bs_output(0),1023);sim.advance(3);assert.equal(c.bs_output(0),0);sim.advance(1000);assert.equal(c.bs_edges(),before+2);
$('save').click();assert.match($('notice').textContent,/STOP FIRST/);$('start').click();assert.equal(c.bs_get(8),0);for(let i=0;i<8;i++)assert.equal(c.bs_output(i),0);
// Scene editing and a per-step probability/ratchet configuration survive power cycle.
sim.edit(2,7,6,1001);sim.edit(1,0,0,7);sim.edit(1,0,1,73);sim.edit(1,0,2,5);sim.edit(0,0,1,37);$('save').click();assert.equal(c.bs_get(9),1);
const library=plain(sim.exportPresets());assert.equal(library.format,'newfangled-bia-presets');assert.equal(library.slots[0].patch.scenes[7][6],1001);assert.equal(library.slots[0].patch.steps[0].ratchets,5);
assert(local.has('newfangled-bia-presets-v1'));assert(!local.has('newfangled-presets-v1'));
sim.edit(2,7,6,0);$('reboot').click();assert.equal(sim.patch().scenes[7][6],1001);assert.equal(c.bs_get(2),0);
// Empty slots do not change the current patch.
$('slot').value=7;const previous=plain(sim.patch());$('load').click();assert.match($('notice').textContent,/EMPTY/);assert.deepEqual(plain(sim.patch()),previous);
const bad=plain(library);bad.slots[0].patch.steps[0].scene=9;assert.throws(()=>sim.importPresets(bad));assert.deepEqual(plain(sim.exportPresets()),library);
assert.throws(()=>sim.importPresets({format:'newfangled-presets',version:6,boot_slot:null,slots:[]}));
// Session recovery preserves unsaved edits, deliberately stopped.
sim.edit(0,0,4,25);const session=plain(sim.session());sim.edit(0,0,4,0);sim.replay(session);assert.equal(sim.patch().swing,25);assert.equal(c.bs_get(2),0);
// All scene/step pages and home settings use the same physical encoder UI.
sim.replay(seedSession);$('right').click();assert.equal(c.bs_get(12),1);$('encoder').click();$('encoder').click();$('left').click();$('encoder').click();assert.equal(sim.patch().steps[0].enabled,false);$('back').click();
for(let i=0;i<16;i++)$('right').click();assert.equal(c.bs_get(12),17);$('encoder').click();$('encoder').click();assert.equal(c.bs_get(8),1);$('start').click();assert.equal(c.bs_get(8),0);
$('right').click();$('encoder').click();$('right').click();$('encoder').click();assert.equal(sim.patch().scenes[0][0],17);$('back').click();
for(let i=0;i<8;i++)$('right').click();assert.equal(c.bs_get(12),0);$('back').click();assert.equal(c.bs_get(14),1);$('right').click();$('encoder').click();$('right').click();$('encoder').click();assert.equal(sim.patch().variation,1);
console.log('BIA browser: OLED/controls, 16 steps/8 scenes, CV lead, audition, slot/JSON/session persistence and invalid-input guards passed.');
// v0.2 performance gestures and direct browser controls share the firmware state.
sim.replay(seedSession);sim.edit(0,0,0,120);sim.edit(2,0,3,400);sim.edit(4,0,0,20);sim.edit(4,0,1,2);sim.edit(4,0,5,20);
assert.equal($('motions').children.length,8);assert.equal(sim.patch().motion[0].fold_ms,10);
const motionBefore=plain(sim.patch().motion),stepsBefore=plain(sim.patch().steps);
const pointer={pointerId:1,preventDefault(){}};
$('start').onpointerdown(pointer);assert.equal(c.bs_get(2),0);$('start').onpointerup(pointer);$('start').click();assert.equal(c.bs_get(2),1);
$('start').onpointerdown(pointer);assert.equal(c.bs_get(1),1);sim.advance(295);assert.equal(c.bs_get(22),0);sim.advance(1);assert.equal(c.bs_get(22),1);assert.equal(c.bs_get(23),0);
$('encoder').click();assert.equal(c.bs_get(23),1);assert.equal(c.bs_get(1),1);const strength=sim.patch().performance.strengths[1];$('right').click();assert.equal(sim.patch().performance.strengths[1],strength+1);
$('start').onpointerup(pointer);$('start').click();assert.equal(c.bs_get(1),1);assert.equal(c.bs_get(22),0);assert.deepEqual(plain(sim.patch().motion),motionBefore);assert.deepEqual(plain(sim.patch().steps),stepsBefore);
for(const [level,encoded] of [0,25,50,66,83,100].entries()){$('strength-fill').value=level;$('strength-fill').oninput();assert.equal(sim.patch().performance.strengths[0],encoded);}
$('strength-fill').value=5;$('strength-fill').oninput();$('perf-fill').onpointerdown(pointer);assert.equal(c.bs_get(22),1);
let guard=0;while(c.bs_get(3)!==13 && guard++<2500)sim.advance(1);assert(guard<2500);assert.equal(c.bs_get(5),4);$('perf-fill').onpointercancel();assert.equal(c.bs_get(22),0);assert.equal(c.bs_get(1),1);
// Space is held rather than auto-repeating Start clicks.
document.listeners.keydown({key:' ',repeat:false,preventDefault(){}});sim.advance(301);assert.equal(c.bs_get(22),1);document.listeners.keydown({key:' ',repeat:true,preventDefault(){}});assert.equal(c.bs_get(1),1);document.listeners.keyup({key:' ',preventDefault(){}});assert.equal(c.bs_get(22),0);assert.equal(c.bs_get(1),1);
$('start').click();assert.equal(c.bs_get(1),0);$('slot').value=0;$('save').click();const v2=plain(sim.exportPresets());assert.equal(v2.version,2);assert.equal(v2.slots[0].patch.motion[0].fold_offset,20);assert.equal(v2.slots[0].patch.performance.strengths[0],100);
const malformed=plain(v2);malformed.slots[0].patch.motion[0].fold_ms=7;assert.throws(()=>sim.importPresets(malformed));assert.deepEqual(plain(sim.exportPresets()),v2);
const legacy=plain(v2);legacy.version=1;for(const slot of legacy.slots)for(const k of ['motion','step_accents','fill_mask','performance'])delete slot.patch[k];sim.importPresets(legacy);assert.equal(sim.patch().motion[0].fold_offset,0);assert.deepEqual(plain(sim.patch().motion[0].variation),[20,20,20,20,20,20]);assert.equal(c.bs_get(22),0);
// Home now opens performance; tempo remains directly available in settings.
$('encoder').click();assert.equal(c.bs_get(26),1);$('right').click();$('encoder').click();$('right').click();$('encoder').click();$('back').click();assert.equal(c.bs_get(26),0);
console.log('BIA v0.2 browser: pointer/Space holds, effect cycling, reversible fill, motion editors, schema migration and persistence passed.');
