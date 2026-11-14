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
}
core.sim_reset();
for(const [index,fixture] of scenarios.entries()) {
  apply(fixture.action);
  assert.deepEqual(Array.from({length:19},(_,i)=>core.sim_get(i)),fixture.state,`state ${index}`);
  assert.deepEqual(Array.from({length:8},(_,i)=>core.sim_output(i)),fixture.outputs,`outputs ${index}`);
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
