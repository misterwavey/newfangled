/* SPDX-License-Identifier: MIT */
(async function () {
  'use strict';
  const CORE_HASH='__CORE_HASH__';
  const $ = id => document.getElementById(id);
  const notice = (text,error=false) => { $('notice').textContent=text;$('notice').classList.toggle('error',error); };
  try {
    const bytes=Uint8Array.from(atob(WASM_BASE64),c=>c.charCodeAt(0));
    const {instance}=await WebAssembly.instantiate(bytes,{}),core=instance.exports;
    let playing=false,speed=1,seed=12345,angle=0,actions=[],history=[],last=performance.now(),fraction=0;
    const contexts=[];
    for(let i=0;i<8;i++) {
      const jack=document.createElement('div');jack.className='jack';jack.innerHTML=`<i></i>${i+1}`;$('jacks').appendChild(jack);
      const scope=document.createElement('div');scope.className='scope';
      scope.innerHTML=`<div class="scope-label"><span>OUT ${i+1}</span><b>0.000 V</b></div><canvas width="480" height="88" aria-label="Output ${i+1} waveform"></canvas>`;
      $('scopes').appendChild(scope);contexts.push({canvas:scope.querySelector('canvas'),label:scope.querySelector('b')});
    }
    function sample() {
      history.push({time:core.sim_get(0),outputs:Array.from({length:8},(_,i)=>core.sim_output(i))});
      if(history.length>2000)history.shift();
    }
    function random32() {seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;seed>>>=0;return seed;}
    function validate(action) {
      if(!action || typeof action!=='object')throw Error('Invalid session action.');
      const integer=(v,min,max)=>Number.isInteger(v)&&v>=min&&v<=max;
      switch(action.type) {
        case 'advance':if(integer(action.ms,0,60000))return;break;
        case 'turn':if(action.direction===1 || action.direction===-1)return;break;
        case 'program':case 'hold-program':case 'start':case 'phase-reset':case 'random':return;
        case 'seed':if(integer(action.value,1,4294967295))return;break;
        case 'input':if(integer(action.index,0,5)&&integer(action.value,0,action.index<2?4095:action.index<4?1:action.index===4?330:48)&&(!(action.index>=4)||action.value>=(action.index===4?10:1)))return;break;
      }
      throw Error('Unsupported or out-of-range session action.');
    }
    function perform(action,record=true) {
      validate(action);
      if(record && actions.length>=100000)throw Error('Session limit reached. Export or reset this session.');
      const elapsed=action.type==='advance'?action.ms:action.type==='turn'?4:action.type==='phase-reset'?75:action.type==='hold-program'?575:['program','start'].includes(action.type)?50:0;
      if(core.sim_get(0)+elapsed>3600000)throw Error('One-hour virtual session limit reached. Export or reset.');
      switch(action.type) {
        case 'advance': {
          let remaining=action.ms;
          while(remaining>0) {const n=Math.min(remaining,1);core.sim_advance(n);sample();remaining-=n;}
          break;
        }
        case 'turn':core.sim_turn(action.direction>0?1:0);angle+=action.direction*15;sample();break;
        case 'program':core.sim_click(1);sample();break;
        case 'hold-program':core.sim_hold_program();sample();break;
        case 'start':core.sim_click(0);sample();break;
        case 'phase-reset':core.sim_reset_phase();sample();break;
        case 'input':core.sim_input(action.index,action.value);sample();break;
        case 'seed':seed=action.value>>>0;break;
        case 'random':core.sim_input(0,random32()&4095);core.sim_input(1,random32()&4095);sample();break;
      }
      if(record)actions.push({...action});
    }
    function draw() {
      const framebuffer=new Uint8Array(core.memory.buffer,core.sim_framebuffer(),512);
      const ctx=$('oled').getContext('2d'),image=ctx.createImageData(128,32);
      for(let y=0;y<32;y++)for(let x=0;x<128;x++) {
        const on=framebuffer[(y>>3)*128+x]&(1<<(y&7)),at=(y*128+x)*4;
        image.data[at]=on?127:9;image.data[at+1]=on?231:15;image.data[at+2]=on?196:17;image.data[at+3]=255;
      }
      ctx.putImageData(image,0,0);
      const time=core.sim_get(0);$('time').textContent=`${String(Math.floor(time/60000)).padStart(2,'0')}:${String(Math.floor(time/1000)%60).padStart(2,'0')}.${String(time%1000).padStart(3,'0')}`;
      $('start').textContent=core.sim_get(25)?(core.sim_get(12)?'STOP':'DISARM'):'START';
      $('play').textContent=playing?'Pause':'Play';$('play').setAttribute('aria-pressed',String(playing));
      $('needle').style.transform=`rotate(${angle}deg)`;
      for(let i=0;i<2;i++) {
        const raw=core.sim_get(4+i);$(i?'cv1':'cv0').value=raw;
        $(i?'cv1-value':'cv0-value').textContent=`${(i?raw*10/4095-5:raw*5/4095).toFixed(2)} V`;
      }
      $('run').checked=!!core.sim_get(6);$('clock').checked=!!core.sim_get(8);
      $('run-led').classList.toggle('on',!!core.sim_get(6));$('clock-led').classList.toggle('on',!!core.sim_get(7));
      if(document.activeElement!==$('clock-bpm'))$('clock-bpm').value=core.sim_get(9);
      $('ppqn').value=core.sim_get(10);if(document.activeElement!==$('seed'))$('seed').value=seed;
      $('action-count').textContent=`${actions.length.toLocaleString()} actions`;
      $('inspector').textContent=`PAGE ${core.sim_get(2)} / ${core.sim_get(1)} BPM / ${core.sim_get(3)?'EDIT':'BROWSE'}`;
      $('transport-state').textContent=core.sim_get(12)?'RUNNING':core.sim_get(19)&&core.sim_get(22)===3?'CLOCK LOST':core.sim_get(25)?'ARMED':'STOPPED';
      $('phase').textContent=`${(core.sim_get(13)/600).toFixed(1)}%`;$('beats').textContent=core.sim_get(14);$('edge-count').textContent=core.sim_edge_count();
      const edges=[];for(let i=Math.max(0,core.sim_edge_count()-10);i<core.sim_edge_count();i++)edges.push(`${core.sim_edge_time(i)} ms  code ${core.sim_edge_level(i)}`);
      $('edges').textContent=edges.length?edges.join('   ·   '):'No edges yet. Press module Start.';
      contexts.forEach(({canvas,label},channel)=>{
        const g=canvas.getContext('2d'),w=canvas.width,h=canvas.height;
        g.clearRect(0,0,w,h);g.strokeStyle='#29373b';g.lineWidth=1;
        for(const y of [8,h/2,h-8]) {g.beginPath();g.moveTo(0,y);g.lineTo(w,y);g.stroke();}
        const samples=history.length?history:[{time:0,outputs:Array(8).fill(0)}];
        const first=samples[0].time,span=Math.max(1,samples[samples.length-1].time-first);g.strokeStyle='#7fe7c4';g.lineWidth=2;g.beginPath();
        let previous=samples[0].outputs[channel];g.moveTo(0,h-8-previous/1023*(h-16));
        for(const sample of samples) {
          const x=(sample.time-first)*w/span,value=sample.outputs[channel];
          g.lineTo(x,h-8-previous/1023*(h-16));g.lineTo(x,h-8-value/1023*(h-16));previous=value;
        }
        g.stroke();label.textContent=`${(core.sim_output(channel)*5/1023).toFixed(3)} V`;
      });
    }
    function stop() {playing=false;fraction=0;last=performance.now();}
    function reset() {stop();core.sim_reset();seed=12345;angle=0;actions=[];history=[];sample();draw();}
    function act(action) {try {perform(action);draw();}catch(error){stop();notice(error.message,true);}}
    function replay(session) {
      if(!session || session.format!=='pnw-newfangled-session' || session.version!==1 || session.core!=='newfangled-v0.3' || !/^[0-9a-f]{64}$/.test(session.coreHash??'') || !Array.isArray(session.actions) || session.actions.length>100000)throw Error('Not a supported Newfangled session.');
      let duration=0;
      for(const action of session.actions) {validate(action);duration+=action.type==='advance'?action.ms:action.type==='turn'?4:action.type==='phase-reset'?75:action.type==='hold-program'?575:['program','start'].includes(action.type)?50:0;}
      if(duration>3600000)throw Error('Replay is limited to one hour of virtual time.');
      reset();for(const action of session.actions)perform(action);draw();notice(`Replayed ${actions.length.toLocaleString()} actions. Simulator paused.${session.coreHash!==CORE_HASH?' Different core build: results may differ.':''}`);
    }
    function exportSession() {
      const session={format:'pnw-newfangled-session',version:1,core:'newfangled-v0.3',coreHash:CORE_HASH,actions};
      const url=URL.createObjectURL(new Blob([JSON.stringify(session,null,2)],{type:'application/json'}));
      const link=document.createElement('a');link.href=url;link.download='newfangled-session.json';link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
      notice('Session exported. Input actions and virtual time can be replayed exactly.');
    }
    $('left').onclick=()=>act({type:'turn',direction:-1});$('right').onclick=()=>act({type:'turn',direction:1});
    $('encoder').onclick=()=>act({type:'program'});$('start').onclick=()=>act({type:'start'});
    $('encoder').addEventListener('wheel',event=>{event.preventDefault();if(event.deltaY)act({type:'turn',direction:event.deltaY<0?1:-1});},{passive:false});
    $('play').onclick=()=>{playing=!playing;last=performance.now();fraction=0;draw();};
    $('back').onclick=()=>act({type:'hold-program'});
    $('phase-reset').onclick=()=>act({type:'phase-reset'});
    $('step').onclick=()=>{stop();act({type:'advance',ms:1});};$('reset').onclick=()=>{reset();notice('Session reset. No saved settings were changed.');};
    $('speed').onchange=()=>{speed=Number($('speed').value);};
    for(let i=0;i<2;i++)$(i?'cv1':'cv0').oninput=event=>act({type:'input',index:i,value:Number(event.target.value)});
    $('run').onchange=event=>act({type:'input',index:2,value:Number(event.target.checked)});
    $('clock').onchange=event=>act({type:'input',index:3,value:Number(event.target.checked)});
    $('clock-bpm').onchange=event=>act({type:'input',index:4,value:Number(event.target.value)});
    $('ppqn').onchange=event=>act({type:'input',index:5,value:Number(event.target.value)});
    $('seed').onchange=event=>act({type:'seed',value:Number(event.target.value)});$('random').onclick=()=>act({type:'random'});
    $('export').onclick=exportSession;$('import').onclick=()=>$('file').click();
    $('file').onchange=async event=>{try {const file=event.target.files[0];if(!file)return;if(file.size>15000000)throw Error('Session file is too large.');replay(JSON.parse(await file.text()));}catch(error){notice(error.message,true);}finally{event.target.value='';}};
    document.addEventListener('keydown',event=>{
      if(event.ctrlKey || event.metaKey || event.altKey)return;
      if(['INPUT','SELECT','TEXTAREA'].includes(document.activeElement.tagName))return;
      if(document.activeElement.tagName==='BUTTON' && ['Enter',' '].includes(event.key))return;
      const handlers={ArrowLeft:()=>$('left').click(),ArrowRight:()=>$('right').click(),Enter:()=>$('encoder').click(),Escape:()=>$('back').click(),' ':()=>$('start').click(),k:()=>$('play').click(),K:()=>$('play').click(),'.':()=>$('step').click()};
      if(handlers[event.key]) {event.preventDefault();handlers[event.key]();}
    });
    function frame(real) {
      if(playing) {
        fraction+=Math.min(100,Math.max(0,real-last))*speed;
        const milliseconds=Math.floor(fraction);fraction-=milliseconds;
        if(milliseconds) {try {perform({type:'advance',ms:milliseconds});draw();}catch(error){stop();notice(error.message,true);}}
      }
      last=real;requestAnimationFrame(frame);
    }
    reset();$('status').textContent='Shared C core · offline';document.body.dataset.ready='true';
    window.pnwSimulator={core,perform:action=>{perform(action);draw();},replay,reset,
      session:()=>({format:'pnw-newfangled-session',version:1,core:'newfangled-v0.3',coreHash:CORE_HASH,actions:actions.map(a=>({...a}))}),
      state:()=>({time:core.sim_get(0),bpm:core.sim_get(1),external:!!core.sim_get(19),ppqn:core.sim_get(20),runMode:core.sim_get(21),armed:!!core.sim_get(25),syncStatus:core.sim_get(22),externalBpm:core.sim_get(23),page:core.sim_get(2),editing:!!core.sim_get(3),selecting:!!core.sim_get(17),field:core.sim_get(18),cvMenu:!!core.sim_get(26),cvMonitor:!!core.sim_get(27),cvField:core.sim_get(28),cvTarget:core.sim_get(29),running:!!core.sim_get(12),phase:core.sim_get(13),beats:core.sim_get(14),seed,outputs:Array.from({length:8},(_,i)=>core.sim_output(i))})};
    requestAnimationFrame(frame);
  } catch(error) {$('status').textContent='Unable to load simulator';notice(error.message,true);document.body.dataset.error=error.message;}
})();
