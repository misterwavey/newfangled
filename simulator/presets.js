/* SPDX-License-Identifier: MIT. JSON is shared with tools/presets.py. */
const PresetJson=(()=>{
  const scales=['OFF','CHROMATIC','MAJOR','MINOR','DORIAN','MAJOR PENTATONIC','MINOR PENTATONIC'],notes=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
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
        const at=172+i*5,bits=b[at]|b[at+1]<<8|b[at+2]<<16;
        p.rhythm={steps:(bits&31)+1,hits:(bits>>>5)&63,rotate:(bits>>>11)&31,probability:(bits>>>16)&127,loop:!!(bits>>>23)};
        p.groove={swing:b[at+3],ratchet:b[at+4]};
        const tone=word(212+i*2);p.pitch={scale:scales[tone&7],root:notes[(tone>>>3)&15],octaves:((tone>>>7)&3)+1,slew_ms:(tone>>>9)*20};
        p.cv={};fields.forEach((name,f)=>{const bit=(i*6+f)*18,at=64+Math.floor(bit/8),v=(b[at]|b[at+1]<<8|b[at+2]<<16)>>>(bit%8);p.cv[name]={source:sources[v&3],depth:signed((v>>>2)&255),bias:signed((v>>>10)&255)};});return p;
      })};
  }
  function encode(p,version=4){
    if(!p || typeof p!=='object')throw Error('Missing preset patch.');
    const b=new Uint8Array(228),word=(at,v)=>{b[at]=v&255;b[at+1]=v>>8;};
    word(0,integer(p.tempo_bpm,10,300));b[2]=choice(p.clock_source,['INT','EXT']);b[3]=choice(p.ppqn,ppqn);b[4]=choice(p.run_input,runs);
    if(typeof p.cv_swap!=='boolean')throw Error('Invalid CV swap.');b[5]=Number(p.cv_swap);
    if(!Array.isArray(p.calibration)||p.calibration.length!==2||!Array.isArray(p.outputs)||p.outputs.length!==8)throw Error('Invalid preset dimensions.');
    p.calibration.forEach((c,i)=>{word(8+i*4,integer(c.zero,0,4095));word(10+i*4,integer(c.positive_5v,0,4095));});
    p.outputs.forEach((o,i)=>fields.forEach((name,f)=>{
      b[16+i*6+f]=f===0?choice(o[name],ratios):f===1?choice(o[name],shapes):integer(o[name],f===2?1:0,f<4?99:100);
      const r=o.cv[name],bit=(i*6+f)*18,at=64+Math.floor(bit/8),v=(choice(r.source,sources)|((integer(r.depth,-100,100)&255)<<2)|((integer(r.bias,-100,100)&255)<<10))<<(bit%8);
      for(let j=0;j<3;j++)b[at+j]|=(v>>>(j*8))&255;
    }));
    p.outputs.forEach((o,i)=>{
      const r=version===1?{steps:1,hits:1,rotate:0,probability:100,loop:false}:o.rhythm;
      if(!r || typeof r.loop!=='boolean')throw Error('Invalid rhythm.');
      const steps=integer(r.steps,1,32),bits=(steps-1)|(integer(r.hits,0,steps)<<5)|(integer(r.rotate,0,steps-1)<<11)|(integer(r.probability,0,100)<<16)|(Number(r.loop)<<23);
      for(let j=0;j<3;j++)b[172+i*5+j]=(bits>>>(j*8))&255;
      const g=version<3?{swing:0,ratchet:1}:o.groove;
      if(!g)throw Error('Missing groove settings.');b[175+i*5]=integer(g.swing,0,50);b[176+i*5]=integer(g.ratchet,1,8);
      const t=version<4?{scale:'OFF',root:'C',octaves:1,slew_ms:0}:o.pitch;
      if(!t)throw Error('Missing pitch settings.');const ms=integer(t.slew_ms,0,2000);if(ms%20)throw Error('Slew must use 20 ms increments.');
      word(212+i*2,choice(t.scale,scales)|(choice(t.root,notes)<<3)|((integer(t.octaves,1,4)-1)<<7)|((ms/20)<<9));
    });return b;
  }
  function validate(library){
    if(!library || library.format!=='newfangled-presets'||![1,2,3,4].includes(library.version)||!Array.isArray(library.slots)||library.slots.length>8)throw Error('Unsupported preset backup.');
    const seen=new Set();const slots=library.slots.map(s=>{integer(s.slot,1,8);if(seen.has(s.slot))throw Error('Duplicate slot.');seen.add(s.slot);return {slot:s.slot,bytes:encode(s.patch,library.version)};});
    if((slots.length && library.boot_slot===null) || (library.boot_slot!==null && (!Number.isInteger(library.boot_slot) || !seen.has(library.boot_slot))))throw Error('Boot slot must be included in backup.');
    return slots.sort((a,b)=>(a.slot===library.boot_slot?1:b.slot===library.boot_slot?-1:a.slot-b.slot));
  }
  return {decode,encode,validate};
})();
