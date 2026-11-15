const fillLevel=n=>!n?0:n<=25?1:n<=50?2:n<=66?3:n<=83?4:5;
const fillStrength=n=>[0,25,50,66,83,100][n];
/* SPDX-License-Identifier: MIT */
(async()=>{
 const $=id=>document.getElementById(id),names=['KICK','SNARE','HAT','BASS','TOM','CLANG','WOOD','NOISE'],lanes=['TRIG','PITCH','DECAY','HARM','FOLD','MORPH','SPREAD','ATTACK'],ppqn=[1,2,4,8,12,16,24,48];
 const wasm=Uint8Array.from(atob('__WASM__'),c=>c.charCodeAt(0)),{instance:{exports:c}}=await WebAssembly.instantiate(wasm,{});
 let playing=false,last=performance.now(),fraction=0,lastSequence=0,history=[],noticeText='',startHeld=false,pointerHandled=false,activeMomentary=null;
 c.bs_reset();const memory=()=>new Uint8Array(c.memory.buffer,c.bs_buffer(),334);
 function patch(){if(!c.bs_patch_read())throw Error('Patch read failed');return BiaJson.decode(memory());}
 function exportPresets(){const slots=[];for(let i=0;i<8;i++)if(c.bs_get(9)&(1<<i)){if(!c.bs_preset_read(i))throw Error('Preset read failed');slots.push({slot:i+1,patch:BiaJson.decode(memory())});}return {format:'newfangled-bia-presets',version:2,boot_slot:slots.length?c.bs_get(10)+1:null,slots};}
 function stopped(){if(c.bs_get(2)||c.bs_get(1)||c.bs_get(8))throw Error('Stop/disarm and end audition first');}
 function importPresets(lib){const incoming=BiaJson.validate(lib);stopped();for(const s of incoming){memory().set(s.bytes);if(!c.bs_validate())throw Error('Invalid patch');}for(const s of incoming){memory().set(s.bytes);if(!c.bs_preset_import(s.slot-1))throw Error('Preset write failed');}c.bs_reboot();lastSequence=c.bs_get(11);persist();history=[];draw();}
 function persist(){try{localStorage.setItem('newfangled-bia-presets-v1',JSON.stringify(exportPresets()));}catch(e){noticeText='Browser storage unavailable; export a JSON backup.';}}
 function safe(f){try{f();noticeText='';}catch(e){noticeText=e.message;}draw();}
 function edit(kind,index,field,value){if(!Number.isInteger(value))throw Error('Use whole steps; envelope times use 5 ms increments');if(!c.bs_edit(kind,index,field,value))throw Error('Value rejected by firmware');}
 function sample(){history.push(Array.from({length:8},(_,i)=>c.bs_output(i)));if(history.length>400)history.shift();}
 function advance(ms){for(let n=0;n<ms;n++){c.bs_advance(1);sample();}}
 const stepControls=[],sceneControls=[],motionControls=[],scopes=[];
 for(let i=0;i<16;i++){
  const el=document.createElement('div');el.className='step';const hit=document.createElement('button');hit.onclick=()=>safe(()=>edit(1,i,3,patch().steps[i].enabled?0:1));el.appendChild(hit);
  const scene=document.createElement('select');names.forEach((n,j)=>{const o=document.createElement('option');o.value=j;o.textContent=n;scene.appendChild(o);});scene.onchange=()=>safe(()=>edit(1,i,0,Number(scene.value)));el.appendChild(scene);
  const number=(name,field,min,max)=>{const label=document.createElement('label');label.textContent=name;const input=document.createElement('input');input.type='number';input.min=min;input.max=max;input.onchange=()=>safe(()=>edit(1,i,field,Number(input.value)));label.appendChild(input);el.appendChild(label);return input;};
  const prob=number('PROB %',1,0,100),ratchet=number('RATCH',2,1,8);
  const accentLabel=document.createElement('label');accentLabel.textContent='ACCENT %';const accent=document.createElement('input');accent.type='number';accent.min=0;accent.max=100;accent.onchange=()=>safe(()=>edit(5,i,0,Number(accent.value)));accentLabel.appendChild(accent);el.appendChild(accentLabel);
  const fillLabel=document.createElement('label');fillLabel.textContent='FILL ELIGIBLE';const fill=document.createElement('input');fill.type='checkbox';fill.onchange=()=>safe(()=>edit(5,i,1,Number(fill.checked)));fillLabel.appendChild(fill);el.appendChild(fillLabel);
  $('steps').appendChild(el);stepControls.push({el,hit,scene,prob,ratchet,accent,fill});
 }
 for(let i=0;i<8;i++){
  const row=document.createElement('tr'),title=document.createElement('td'),button=document.createElement('button');button.textContent=names[i];button.onclick=()=>safe(()=>c.bs_audition(i));title.appendChild(button);row.appendChild(title);const values=[];
  for(let f=0;f<7;f++){const cell=document.createElement('td'),input=document.createElement('input');input.type='number';input.min=0;input.max=f?5:60;input.step=f?.01:1;input.onchange=()=>safe(()=>edit(2,i,f,Math.round(Number(input.value)*1023/(f?5:60))));cell.appendChild(input);row.appendChild(cell);values.push(input);}
  $('scenes').appendChild(row);sceneControls.push(values);
  const out=document.createElement('div');out.className='output';const name=document.createElement('strong'),volts=document.createElement('span'),canvas=document.createElement('canvas');canvas.width=400;canvas.height=60;name.textContent=`OUT${i+1} · ${lanes[i]}`;out.appendChild(name);out.appendChild(volts);out.appendChild(canvas);$('outputs').appendChild(out);scopes.push({volts,canvas});
 }
 for(let i=0;i<8;i++){
  const box=document.createElement('details');box.className='panel';const summary=document.createElement('summary');summary.textContent=names[i]+' · envelopes, accents and protected variation';box.appendChild(summary);const controls=[];
  const fields=[['Fold offset %',0,-100,100,1],['Fold decay ms',1,5,500,5],['Harm offset %',2,-100,100,1],['Harm decay ms',3,5,500,5],['Restart per ratchet',4,0,1,1],['Accent Fold %',5,-100,100,1],['Accent Harm %',6,-100,100,1],['Accent Decay %',7,-100,100,1],...['Decay','Harm','Fold','Morph','Spread','Attack'].map((n,f)=>[n+' VAR limit %',8+f,0,100,1])];
  fields.forEach(([name,field,min,max,step])=>{const label=document.createElement('label');label.textContent=name;const input=document.createElement('input');input.type=field===4?'checkbox':'number';input.min=min;input.max=max;input.step=step;
   input.onchange=()=>safe(()=>edit(4,i,field,field===4?Number(input.checked):(field===1||field===3)?Number(input.value)/5:Number(input.value)));label.appendChild(input);box.appendChild(label);controls.push(input);
  });$('motions').appendChild(box);motionControls.push(controls);
 }
 for(const id of ['ppqn','clock-ppqn'])ppqn.forEach((n,i)=>{const o=document.createElement('option');o.value=id==='ppqn'?i:n;o.textContent=n;$(id).appendChild(o);});$('clock-ppqn').value=24;
 for(let i=1;i<=8;i++){const o=document.createElement('option');o.value=i-1;o.textContent=i;$('slot').appendChild(o);}
 const globals={tempo:0,variation:1,delay:2,pulse:3,swing:4,source:5,ppqn:6,'run-mode':7,macro:8};
 for(const [id,field] of Object.entries(globals))$(id).onchange=()=>safe(()=>edit(0,0,field,Number($(id).value)));
 $('play').onclick=()=>{playing=!playing;draw();};$('start').onclick=()=>{if(pointerHandled){pointerHandled=false;return;}safe(()=>c.bs_click(0));};$('encoder').onclick=()=>safe(()=>c.bs_click(1));$('left').onclick=()=>safe(()=>c.bs_turn(0));$('right').onclick=()=>safe(()=>c.bs_turn(1));$('back').onclick=()=>safe(()=>c.bs_hold());$('advance').onclick=()=>safe(()=>advance(1));$('phase-reset').onclick=()=>safe(()=>c.bs_phase_reset());
 function startDown(){if(startHeld)return;startHeld=true;safe(()=>c.bs_button(0,1));}
 function startUp(){if(!startHeld)return;safe(()=>c.bs_button(0,0));startHeld=false;}
 $('start').onpointerdown=e=>{e.preventDefault?.();pointerHandled=true;$('start').setPointerCapture?.(e.pointerId);startDown();};
 $('start').onpointerup=()=>startUp();$('start').onpointercancel=()=>{safe(()=>c.bs_abort());startHeld=false;pointerHandled=false;};
 function momentary(effect,on){safe(()=>c.bs_perform(effect,on?1:0));activeMomentary=on?effect:null;}
 ['fill','accent','var'].forEach((name,effect)=>{const button=$('perf-'+name);button.onpointerdown=e=>{e.preventDefault?.();button.setPointerCapture?.(e.pointerId);momentary(effect,true);};button.onpointerup=()=>momentary(effect,false);button.onpointercancel=()=>momentary(effect,false);
  button.onkeydown=e=>{if((e.key===' '||e.key==='Enter')&&!e.repeat){e.preventDefault();momentary(effect,true);}};button.onkeyup=e=>{if(e.key===' '||e.key==='Enter'){e.preventDefault();momentary(effect,false);}};
  $('strength-'+name).oninput=()=>safe(()=>edit(0,0,11+effect,effect===0?fillStrength(Number($('strength-'+name).value)):Number($('strength-'+name).value)));
 });
 $('reboot').onclick=()=>safe(()=>{c.bs_reboot();history=[];});
 const status=['','BUSY','SAVED','LOADED','EMPTY','STOP FIRST','BAD DATA','IO ERROR','OTHER DATA'];
 $('save').onclick=()=>safe(()=>{const result=c.bs_preset(Number($('slot').value),0);if(result!==2)throw Error(status[result]||'Save failed');persist();});
 $('load').onclick=()=>safe(()=>{const result=c.bs_preset(Number($('slot').value),1);if(result!==3)throw Error(status[result]||'Load failed');});
 $('clock-on').onchange=()=>safe(()=>c.bs_input(3,Number($('clock-on').checked)));$('run-high').onchange=()=>safe(()=>c.bs_input(2,Number($('run-high').checked)));$('clock-bpm').onchange=()=>safe(()=>c.bs_input(4,Number($('clock-bpm').value)));$('clock-ppqn').onchange=()=>safe(()=>c.bs_input(5,Number($('clock-ppqn').value)));
 $('cv1').oninput=()=>safe(()=>c.bs_input(0,Number($('cv1').value)));$('cv2').oninput=()=>safe(()=>c.bs_input(1,Number($('cv2').value)));
 function download(object){const url=URL.createObjectURL(new Blob([JSON.stringify(object,null,2)],{type:'application/json'})),a=document.createElement('a');a.href=url;a.download='newfangled-bia-presets.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
 $('export').onclick=()=>safe(()=>download(exportPresets()));$('import').onclick=()=>$('file').click();$('file').onchange=async()=>{try{const f=$('file').files?.[0];if(!f)return;if(f.size>100000)throw Error('File exceeds 100 KB');importPresets(JSON.parse(await f.text()));noticeText='Presets imported; transport stopped.';}catch(e){noticeText=e.message;}draw();};
 function draw(){const p=patch(),pixels=new Uint8Array(c.memory.buffer,c.bs_pixels(),512),ctx=$('oled').getContext('2d'),im=ctx.createImageData(128,32);
  for(let y=0;y<32;y++)for(let x=0;x<128;x++){const on=pixels[(y>>3)*128+x]&(1<<(y&7)),at=(y*128+x)*4;im.data[at]=on?137:9;im.data[at+1]=on?232:20;im.data[at+2]=on?185:17;im.data[at+3]=255;}ctx.putImageData(im,0,0);
  const values={tempo:p.tempo_bpm,variation:p.variation,delay:p.cv_lead_ms,pulse:p.pulse_ms,swing:p.swing,source:p.clock_source==='EXT'?1:0,ppqn:ppqn.indexOf(p.ppqn),'run-mode':['OFF','GATE','RESET'].indexOf(p.run_input),macro:['OFF','CV1','CV2'].indexOf(p.variation_cv)};
  for(const [id,v] of Object.entries(values))if(document.activeElement!==$(id))$(id).value=v;$('variation-value').textContent=p.variation+'%';
  $('play').textContent=playing?'Pause time':'Play time';$('start').textContent=c.bs_get(8)?'End audition':c.bs_get(2)?'Stop / Disarm':'Start';
  $('status').textContent=`${(c.bs_get(0)/1000).toFixed(3)} s · ${c.bs_get(8)?'AUDITION':c.bs_get(1)?'RUNNING':c.bs_get(2)?'ARMED':'STOPPED'} · step ${c.bs_get(3)+1} · ratchets ${c.bs_get(5)} · ${['INT','EXT WAIT','EXT LOCK','EXT LOST','RUN LOW'][c.bs_get(7)]}`;
  stepControls.forEach(({el,hit,scene,prob,ratchet,accent,fill},i)=>{const s=p.steps[i];el.classList.toggle('playing',!!c.bs_get(1)&&c.bs_get(3)===i);hit.textContent=`${i+1} · ${s.enabled?'HIT':'REST'}`;hit.classList.toggle('on',s.enabled);if(document.activeElement!==scene)scene.value=s.scene-1;if(document.activeElement!==prob)prob.value=s.probability;if(document.activeElement!==ratchet)ratchet.value=s.ratchets;if(document.activeElement!==accent)accent.value=p.step_accents[i];fill.checked=!!(p.fill_mask&(1<<i));});
  sceneControls.forEach((inputs,i)=>inputs.forEach((input,f)=>{if(document.activeElement!==input)input.value=f?(p.scenes[i][f]*5/1023).toFixed(2):Math.round(p.scenes[i][f]*60/1023);}));
  motionControls.forEach((inputs,i)=>{const m=p.motion[i],values=[m.fold_offset,m.fold_ms,m.harm_offset,m.harm_ms,m.retrigger,...m.accent,...m.variation];inputs.forEach((input,f)=>{if(f===4)input.checked=!!values[f];else if(document.activeElement!==input)input.value=values[f];});});
  ['fill','accent','var'].forEach((name,effect)=>{if(document.activeElement!==$('strength-'+name))$('strength-'+name).value=effect===0?fillLevel(p.performance.strengths[effect]):p.performance.strengths[effect];$('perf-'+name).classList.toggle('on',!!c.bs_get(22)&&c.bs_get(23)===effect);});
  $('performance-status').textContent=c.bs_get(22)?`${['FILL','ACCENT','VAR'][c.bs_get(23)]} ${c.bs_get(23)===0?fillLevel(c.bs_get(24)):c.bs_get(24)}${c.bs_get(23)===0?'':'%'} · release to return`:`Ready: ${p.performance.effect} · base pattern unchanged`;
  scopes.forEach(({volts,canvas},i)=>{volts.textContent=(c.bs_output(i)*5/1023).toFixed(3)+' V';const ctx=canvas.getContext('2d');ctx.clearRect(0,0,400,60);ctx.beginPath();ctx.strokeStyle=i?'#80d9ad':'#e9bf73';history.forEach((row,x)=>{const y=58-row[i]*56/1023;if(x)ctx.lineTo(x,y);else ctx.moveTo(x,y);});ctx.stroke();});
  $('notice').textContent=noticeText;if(lastSequence!==c.bs_get(11)){lastSequence=c.bs_get(11);persist();}
 }
 function session(){return {format:'newfangled-bia-session',version:1,patch:patch(),presets:exportPresets()};}
 function replay(s){if(!s||s.format!=='newfangled-bia-session'||s.version!==1)throw Error('Unsupported BIA session');const encoded=BiaJson.encode(s.patch.motion?s.patch:BiaJson.upgrade(s.patch));BiaJson.validate(s.presets);c.bs_reset();importPresets(s.presets);memory().set(encoded);if(!c.bs_patch_import())throw Error('Session patch rejected');history=[];draw();}
 window.biaSimulator={core:c,patch,edit:(...args)=>{edit(...args);draw();},advance:ms=>{advance(ms);draw();},exportPresets,importPresets,session,replay};window.pnwSimulator=window.biaSimulator;
 document.addEventListener('keydown',e=>{if(e.defaultPrevented)return;if(['INPUT','SELECT'].includes(document.activeElement?.tagName))return;if(e.key===' '){e.preventDefault();if(!e.repeat)startDown();return;}const ids={ArrowLeft:'left',ArrowRight:'right',Enter:'encoder',Escape:'back'};if(ids[e.key]){e.preventDefault();$(ids[e.key]).click();}});
 document.addEventListener('keyup',e=>{if(e.key===' '&&startHeld){e.preventDefault();startUp();}});
 window.addEventListener?.('blur',()=>{if(startHeld||activeMomentary!==null){c.bs_abort();startHeld=false;activeMomentary=null;pointerHandled=false;draw();}});
 try{const stored=localStorage.getItem('newfangled-bia-presets-v1');if(stored)importPresets(JSON.parse(stored));}catch(e){noticeText='Stored presets could not load: '+e.message;}
 function frame(t){if(playing||startHeld||activeMomentary!==null){fraction+=Math.min(100,t-last);const ms=Math.floor(fraction);fraction-=ms;advance(ms);}last=t;draw();requestAnimationFrame(frame);}draw();document.body.dataset.ready='true';requestAnimationFrame(frame);
})().catch(e=>{document.body.dataset.error=e.message;document.getElementById('notice').textContent=e.message;});
