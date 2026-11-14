/* SPDX-License-Identifier: MIT. JSON is shared with tools/presets.py. */
const PresetJson=(()=>{
  const fields=['ratio','shape','width','phase','level','offset'];
  const ratios=['/16','/8','/4','/3','/2','X1','X2','X3','X4','X8','X16'];
  const shapes=['GATE','TRIANGLE','RAMP UP','RAMP DOWN','RANDOM'];
  const ppqn=[1,2,4,8,12,16,24,48],runs=['OFF','GATE','RESET'],sources=['OFF','CV1','CV2'];
  const integer=(v,lo,hi)=>{if(!Number.isInteger(v)||v<lo||v>hi)throw Error('Invalid preset number.');return v;};
  const choice=(v,values)=>{const i=values.indexOf(v);if(i<0)throw Error('Invalid preset choice.');return i;};
  function decode(b){
    const word=at=>b[at]|b[at+1]<<8,signed=v=>v>127?v-256:v;
    return {tempo_bpm:word(0),clock_source:b[2]?'EXT':'INT',ppqn:ppqn[b[3]],run_input:runs[b[4]],cv_swap:!!b[5],
      calibration:[0,1].map(i=>({zero:word(8+i*4),positive_5v:word(10+i*4)})),
      outputs:Array.from({length:8},(_,i)=>{
        const p={};fields.forEach((name,f)=>p[name]=f===0?ratios[b[16+i*6]]:f===1?shapes[b[17+i*6]]:b[16+i*6+f]);
        const at=208+i*3,bits=b[at]|b[at+1]<<8|b[at+2]<<16;
        p.rhythm={steps:(bits&31)+1,hits:(bits>>>5)&63,rotate:(bits>>>11)&31,probability:(bits>>>16)&127,loop:!!(bits>>>23)};
        p.cv={};fields.forEach((name,f)=>{const at=64+(i*6+f)*3;p.cv[name]={source:sources[b[at]],depth:signed(b[at+1]),bias:signed(b[at+2])};});return p;
      })};
  }
  function encode(p,legacy=false){
    if(!p || typeof p!=='object')throw Error('Missing preset patch.');
    const b=new Uint8Array(232),word=(at,v)=>{b[at]=v&255;b[at+1]=v>>8;};
    word(0,integer(p.tempo_bpm,10,300));b[2]=choice(p.clock_source,['INT','EXT']);b[3]=choice(p.ppqn,ppqn);b[4]=choice(p.run_input,runs);
    if(typeof p.cv_swap!=='boolean')throw Error('Invalid CV swap.');b[5]=Number(p.cv_swap);
    if(!Array.isArray(p.calibration)||p.calibration.length!==2||!Array.isArray(p.outputs)||p.outputs.length!==8)throw Error('Invalid preset dimensions.');
    p.calibration.forEach((c,i)=>{word(8+i*4,integer(c.zero,0,4095));word(10+i*4,integer(c.positive_5v,0,4095));});
    p.outputs.forEach((o,i)=>fields.forEach((name,f)=>{
      b[16+i*6+f]=f===0?choice(o[name],ratios):f===1?choice(o[name],shapes):integer(o[name],f===2?1:0,f<4?99:100);
      const r=o.cv[name],at=64+(i*6+f)*3;b[at]=choice(r.source,sources);b[at+1]=integer(r.depth,-100,100)&255;b[at+2]=integer(r.bias,-100,100)&255;
    }));
    p.outputs.forEach((o,i)=>{
      const r=legacy?{steps:1,hits:1,rotate:0,probability:100,loop:false}:o.rhythm;
      if(!r || typeof r.loop!=='boolean')throw Error('Invalid rhythm.');
      const steps=integer(r.steps,1,32),bits=(steps-1)|(integer(r.hits,0,steps)<<5)|(integer(r.rotate,0,steps-1)<<11)|(integer(r.probability,0,100)<<16)|(Number(r.loop)<<23);
      for(let j=0;j<3;j++)b[208+i*3+j]=(bits>>>(j*8))&255;
    });return b;
  }
  function validate(library){
    if(!library || library.format!=='newfangled-presets'||![1,2].includes(library.version)||!Array.isArray(library.slots)||library.slots.length>8)throw Error('Unsupported preset backup.');
    const seen=new Set();const slots=library.slots.map(s=>{integer(s.slot,1,8);if(seen.has(s.slot))throw Error('Duplicate slot.');seen.add(s.slot);return {slot:s.slot,bytes:encode(s.patch,library.version===1)};});
    if((slots.length && library.boot_slot===null) || (library.boot_slot!==null && (!Number.isInteger(library.boot_slot) || !seen.has(library.boot_slot))))throw Error('Boot slot must be included in backup.');
    return slots.sort((a,b)=>(a.slot===library.boot_slot?1:b.slot===library.boot_slot?-1:a.slot-b.slot));
  }
  return {decode,encode,validate};
})();
