import fs from 'node:fs';
import assert from 'node:assert/strict';
const {instance:{exports:core}}=await WebAssembly.instantiate(fs.readFileSync(process.argv[2]),{});
const scenarios=JSON.parse(fs.readFileSync(process.argv[3],'utf8'));
function apply(action) {
  if(action.type==='advance')core.sim_advance(action.value);
  if(action.type==='turn')core.sim_turn(action.value);
  if(action.type==='click')core.sim_click(action.value);
  if(action.type==='input')core.sim_input(action.index,action.value);
  if(action.type==='raw')core.sim_raw(action.value);
  if(action.type==='hold')core.sim_hold_program();
  if(action.type==='reset')core.sim_reset();
}
core.sim_reset();
for(const [index,fixture] of scenarios.entries()) {
  apply(fixture.action);
  assert.deepEqual(Array.from({length:35},(_,i)=>core.sim_get(i)>>>0),fixture.state,`state ${index}`);
  assert.deepEqual(Array.from({length:8},(_,i)=>core.sim_output(i)),fixture.outputs,`outputs ${index}`);
  assert.deepEqual(Array.from({length:8},(_,i)=>Array.from({length:6},(_,f)=>core.sim_effective(i,f))),fixture.effective,`effective ${index}`);
  assert.deepEqual(Array.from({length:8},(_,i)=>Array.from({length:6},(_,f)=>Array.from({length:3},(_,k)=>core.sim_cv_route(i,f,k)>>>0))),fixture.routes,`routes ${index}`);
  const pixels=Buffer.from(core.memory.buffer,core.sim_framebuffer(),512);
  assert.equal(pixels.toString('base64'),fixture.framebuffer,`framebuffer ${index}`);
  const edges=Array.from({length:Math.min(2048,core.sim_edge_count())},(_,j)=>{const i=Math.max(0,core.sim_edge_count()-2048)+j;return [core.sim_edge_time(i),core.sim_edge_level(i)];});
  assert.deepEqual(edges,fixture.edges,`edges ${index}`);
  for(let i=0;i<8;i++)assert(core.sim_output(i)>=0 && core.sim_output(i)<=1023);
}
console.log(`${scenarios.length} actions: WebAssembly matches native C state and framebuffer.`);

// Browsing all outputs without clicking must never change modes or values.
core.sim_reset();
for(let i=0;i<27;i++) {
  core.sim_turn(1);assert.equal(core.sim_get(2),(i+1)%9);
  assert.equal(core.sim_get(3),0);assert.equal(core.sim_get(17),0);
}
// Independent end-to-end transport oracle: accepted Start at 21 ms.
core.sim_reset();core.sim_click(0);assert.equal(core.sim_get(12),1);
assert.equal(core.sim_edge_time(0),21);assert.equal(core.sim_edge_level(0),1023);
core.sim_advance(1000);assert.deepEqual(Array.from({length:5},(_,i)=>core.sim_edge_time(i)),[21,271,521,771,1021]);
core.sim_reset_phase();assert.equal(core.sim_get(12),1);assert.equal(core.sim_get(15),1);
core.sim_click(0);assert.equal(core.sim_get(12),0);assert.equal(core.sim_output(0),0);
const count=core.sim_edge_count();core.sim_advance(60000);assert.equal(core.sim_edge_count(),count);
core.sim_reset();core.sim_click(1);for(let i=0;i<180;i++)core.sim_turn(1);core.sim_click(1);core.sim_click(0);
assert.equal(core.sim_get(1),300);
for(let i=0;i<20;i++)core.sim_advance(60000);
assert(core.sim_edge_count()>2048);assert.equal(core.sim_edge_time(core.sim_edge_count()-2049)>>>0,0xffffffff);
assert.notEqual(core.sim_edge_time(core.sim_edge_count()-2048)>>>0,0xffffffff);
console.log('Clock edges, reset, stop and bounded trace-ring checks passed.');

// Exercise the actual encoder UI and generated external clock, not a settings bypass.
function globalSettings() {core.sim_hold_program();assert.equal(core.sim_get(17),1);}
function setExternal() {globalSettings();core.sim_click(1);core.sim_turn(1);core.sim_click(1);core.sim_hold_program();}
for(const ppqn of [1,2,4,8,12,16,24,48]) {
  core.sim_reset();setExternal();assert.equal(core.sim_get(19),1);
  globalSettings();core.sim_turn(1);core.sim_click(1);
  const choices=[1,2,4,8,12,16,24,48],delta=choices.indexOf(ppqn)-6;
  for(let i=0;i<Math.abs(delta);i++)core.sim_turn(delta>0?1:0);
  core.sim_click(1);core.sim_hold_program();assert.equal(core.sim_get(20),ppqn);
  core.sim_click(0);assert.equal(core.sim_get(12),0);assert.equal(core.sim_get(25),1);assert.equal(core.sim_get(22),5); // Armed, waiting.
  core.sim_input(5,ppqn);core.sim_input(3,1);core.sim_advance(2000);
  assert.equal(core.sim_get(12),1);assert.equal(core.sim_get(22),2);assert.equal(core.sim_get(23),120);
  core.sim_input(3,0);core.sim_advance(1200);
  assert.equal(core.sim_get(12),0);assert.equal(core.sim_get(22),3);
  for(let i=0;i<8;i++)assert.equal(core.sim_output(i),0);
  core.sim_input(3,1);core.sim_advance(1200);assert.equal(core.sim_get(12),1);
  core.sim_click(0);core.sim_advance(600);assert.equal(core.sim_get(12),0);
}
// Gate mode requires both manual arming and a high Run gate.
core.sim_reset();globalSettings();core.sim_turn(1);core.sim_turn(1);core.sim_click(1);core.sim_turn(1);
core.sim_click(1);core.sim_hold_program();assert.equal(core.sim_get(21),1);
core.sim_click(0);assert.equal(core.sim_get(12),0);
core.sim_input(2,1);core.sim_advance(1);assert.equal(core.sim_get(12),1);
core.sim_input(2,0);core.sim_advance(1);assert.equal(core.sim_get(12),0);
console.log('External clock: all PPQN settings, loss/reacquisition and gated Run passed.');

// Per-parameter CV through real UI controls; base values remain separate.
core.sim_reset();core.sim_turn(1);core.sim_click(1);
for(let i=0;i<4;i++)core.sim_turn(1); // LEVEL
core.sim_click(1);for(let i=0;i<100;i++)core.sim_turn(0);core.sim_click(1);
core.sim_turn(1);core.sim_turn(1);core.sim_click(1);assert.equal(core.sim_get(26),1); // CV SETUP
core.sim_click(1);for(let i=0;i<4;i++)core.sim_turn(1);core.sim_click(1); // DEST LEVEL
core.sim_turn(1);core.sim_click(1);core.sim_turn(1);core.sim_click(1); // SRC CV1
assert.equal(core.sim_cv_route(0,4,0),1);assert.equal(core.sim_cv_route(0,4,1),100);
core.sim_hold_program();core.sim_click(0);
core.sim_input(0,4095);core.sim_advance(250);assert.equal(core.sim_effective(0,4),100);
core.sim_input(0,0);core.sim_advance(250);assert.equal(core.sim_effective(0,4),0);
// Raw/normalized monitor, then inverted 0 V/+5 V endpoint capture.
core.sim_click(1);core.sim_turn(0);core.sim_click(1);assert.equal(core.sim_get(27),1);
core.sim_input(0,4000);core.sim_advance(200);assert.equal(core.sim_get(31),32007);
core.sim_hold_program();core.sim_turn(0);assert.equal(core.sim_get(2),0);globalSettings();
for(let i=0;i<5;i++)core.sim_turn(1);core.sim_click(1); // C1 ZERO=4000
core.sim_turn(1);core.sim_input(0,200);core.sim_advance(200);core.sim_click(1); // C1 +5=200
core.sim_advance(200);assert.equal(core.sim_get(31),32768);assert.equal(core.sim_effective(0,4),100);
core.sim_input(0,4000);core.sim_advance(200);assert.equal(core.sim_get(31),0);assert.equal(core.sim_effective(0,4),0);
core.sim_click(0);for(let i=0;i<8;i++)assert.equal(core.sim_output(i),0);
console.log('CV routing, modulation endpoints, raw monitor, inverted calibration and Stop-to-zero passed.');
