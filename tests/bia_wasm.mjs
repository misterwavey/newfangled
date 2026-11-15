import fs from 'node:fs';import vm from 'node:vm';import assert from 'node:assert/strict';
if(process.argv[2]==='json'){
 const context={Uint8Array};vm.createContext(context);vm.runInContext(fs.readFileSync('simulator/bia/presets.js','utf8')+'\nthis.bia=BiaJson;',context);
 const p=JSON.parse(fs.readFileSync(process.argv[3],'utf8'));const bytes=context.bia.encode(p);assert.deepEqual(JSON.parse(JSON.stringify(context.bia.decode(bytes))),p);console.log(Buffer.from(bytes).toString('base64'));
}else{
 const {instance:{exports:c}}=await WebAssembly.instantiate(fs.readFileSync(process.argv[2]),{});c.bs_reset();const fixtures=JSON.parse(fs.readFileSync(process.argv[3],'utf8'));
 for(const [i,f] of fixtures.entries()){c[f.name](...f.args);assert.deepEqual(Array.from({length:28},(_,n)=>c.bs_get(n)),f.state,`state ${i}`);assert.deepEqual(Array.from({length:8},(_,n)=>c.bs_output(n)),f.outputs,`outputs ${i}`);assert.equal(Buffer.from(c.memory.buffer,c.bs_pixels(),512).toString('base64'),f.pixels,`OLED ${i}`);}
 console.log(`${fixtures.length} BIA actions: native and WASM state, outputs and OLED match.`);
}
