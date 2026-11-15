/* SPDX-License-Identifier: MIT. Explicit 334-byte BIA-only scene format. */
const BiaJson=(()=>{
 const ppqn=[1,2,4,8,12,16,24,48],runs=['OFF','GATE','RESET'],sources=['OFF','CV1','CV2'];
 const integer=(n,a,b)=>{if(!Number.isInteger(n)||n<a||n>b)throw Error('Preset value out of range');return n;};
 const choice=(v,a)=>{const i=a.indexOf(v);if(i<0)throw Error('Invalid preset choice');return i;};
 function upgrade(p){return {...p,step_accents:Array(16).fill(0),fill_mask:40960,motion:Array.from({length:8},()=>({fold_offset:0,fold_ms:50,harm_offset:0,harm_ms:50,retrigger:true,accent:[0,0,0],variation:Array(6).fill(20)})),performance:{effect:'FILL',strengths:[50,50,25]}};}
 function decode(b){const word=i=>b[i]|b[i+1]<<8;let at=6;
  const p={tempo_bpm:word(4),scenes:Array.from({length:8},()=>Array.from({length:7},()=>{const v=word(at);at+=2;return v;})),steps:Array.from({length:16},()=>{const s={scene:b[at]+1,probability:b[at+1],ratchets:b[at+2],enabled:!!b[at+3]};at+=4;return s;})};
  p.variation=b[at++];p.cv_lead_ms=b[at++];p.pulse_ms=b[at++];p.swing=b[at++];p.clock_source=b[at++]?'EXT':'INT';p.ppqn=ppqn[b[at++]];p.run_input=runs[b[at++]];p.variation_cv=sources[b[at++]];
  p.calibration=Array.from({length:2},()=>{const c={zero:word(at),positive_5v:word(at+2)};at+=4;return c;});p.cv_swap=!!b[at++];const result=upgrade(p);
  if(b[3]===2){result.step_accents=Array.from(b.slice(at,at+16));at+=16;result.fill_mask=word(at);at+=2;const signed=n=>n>127?n-256:n;
   for(const m of result.motion){m.fold_offset=signed(b[at++]);m.fold_ms=b[at++]*5;m.harm_offset=signed(b[at++]);m.harm_ms=b[at++]*5;m.accent=Array.from(b.slice(at,at+3),signed);at+=3;m.variation=Array.from(b.slice(at,at+6));at+=6;}
   const bits=b[at++];result.motion.forEach((m,i)=>m.retrigger=!!(bits&(1<<i)));result.performance={effect:['FILL','ACCENT','VAR'][b[at++]],strengths:Array.from(b.slice(at,at+3))};
  }return result;
 }
 function encode(p){const b=new Uint8Array(334),word=(i,n)=>{b[i]=n&255;b[i+1]=n>>8;};b.set([66,73,65,2]);word(4,integer(p.tempo_bpm,10,300));let at=6;
  if(!Array.isArray(p.scenes)||p.scenes.length!==8||!Array.isArray(p.steps)||p.steps.length!==16||!Array.isArray(p.calibration)||p.calibration.length!==2)throw Error('Invalid preset dimensions');
  for(const scene of p.scenes){if(!Array.isArray(scene)||scene.length!==7)throw Error('Seven CV lanes required');for(const v of scene){word(at,integer(v,0,1023));at+=2;}}
  for(const s of p.steps){if(typeof s.enabled!=='boolean')throw Error('Step enabled must be boolean');b.set([integer(s.scene,1,8)-1,integer(s.probability,0,100),integer(s.ratchets,1,8),Number(s.enabled)],at);at+=4;}
  b.set([integer(p.variation,0,100),integer(p.cv_lead_ms,1,20),integer(p.pulse_ms,1,10),integer(p.swing,0,50),choice(p.clock_source,['INT','EXT']),choice(p.ppqn,ppqn),choice(p.run_input,runs),choice(p.variation_cv,sources)],at);at+=8;
  for(const c of p.calibration){word(at,integer(c.zero,0,4095));word(at+2,integer(c.positive_5v,0,4095));at+=4;}
  if(typeof p.cv_swap!=='boolean')throw Error('CV swap must be boolean');b[at++]=Number(p.cv_swap);
  if(!Array.isArray(p.step_accents)||p.step_accents.length!==16||!Array.isArray(p.motion)||p.motion.length!==8)throw Error('Invalid motion dimensions');
  for(const n of p.step_accents)b[at++]=integer(n,0,100);word(at,integer(p.fill_mask,0,65535));at+=2;let bits=0;
  p.motion.forEach((m,i)=>{for(const prefix of ['fold','harm']){const ms=integer(m[prefix+'_ms'],5,500);if(ms%5)throw Error('Envelope durations use 5 ms increments');b[at++]=integer(m[prefix+'_offset'],-100,100)&255;b[at++]=ms/5;}
   if(!Array.isArray(m.accent)||m.accent.length!==3||!Array.isArray(m.variation)||m.variation.length!==6||typeof m.retrigger!=='boolean')throw Error('Invalid scene motion');
   for(const n of m.accent)b[at++]=integer(n,-100,100)&255;for(const n of m.variation)b[at++]=integer(n,0,100);if(m.retrigger)bits|=1<<i;
  });b[at++]=bits;b[at++]=choice(p.performance.effect,['FILL','ACCENT','VAR']);
  if(!Array.isArray(p.performance.strengths)||p.performance.strengths.length!==3)throw Error('Three effect strengths required');for(const n of p.performance.strengths)b[at++]=integer(n,0,100);
  return b;
 }
 function validate(lib){if(!lib||lib.format!=='newfangled-bia-presets'||![1,2].includes(lib.version)||!Array.isArray(lib.slots)||lib.slots.length>8)throw Error('Not a BIA preset library');
  const seen=new Set(),slots=lib.slots.map(s=>{integer(s.slot,1,8);if(seen.has(s.slot))throw Error('Duplicate slot');seen.add(s.slot);return {slot:s.slot,bytes:encode(lib.version===1?upgrade(s.patch):s.patch)};});
  if((slots.length&&lib.boot_slot===null)||(lib.boot_slot!==null&&!seen.has(lib.boot_slot)))throw Error('Boot slot missing');
  return slots.sort((a,b)=>a.slot===lib.boot_slot?1:b.slot===lib.boot_slot?-1:a.slot-b.slot);
 }
 return {decode,encode,validate,upgrade};
})();
