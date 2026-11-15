"""BIA controller JSON and flash journal. Incompatible application IDs are guarded."""
import copy
import struct
import zlib
BASE=0x08006000
BANK_BYTES=4096
STORE_BYTES=8192
PPQN=(1,2,4,8,12,16,24,48)
RUNS=('OFF','GATE','RESET')
SOURCES=('OFF','CV1','CV2')
def integer(n,a,b):
    if type(n) is not int or not a<=n<=b:raise ValueError('BIA preset value out of range')
    return n

def choice(v,values):
    if isinstance(v,bool):raise ValueError('Invalid BIA choice')
    return values.index(v)

def upgrade(p):
    p=copy.deepcopy(p)
    p['step_accents']=[0]*16;p['fill_mask']=0xa000
    p['motion']=[dict(fold_offset=0,fold_ms=50,harm_offset=0,harm_ms=50,retrigger=True,accent=[0,0,0],variation=[20]*6) for _ in range(8)]
    p['performance']=dict(effect='FILL',strengths=[50,50,25]);return p

def encode(p,schema=2):
    b=bytearray(334);b[:4]=b'BIA'+bytes((schema,));struct.pack_into('<H',b,4,integer(p['tempo_bpm'],10,300));at=6
    if len(p['scenes'])!=8 or len(p['steps'])!=16 or len(p['calibration'])!=2:raise ValueError('Invalid BIA preset dimensions')
    for scene in p['scenes']:
        if len(scene)!=7:raise ValueError('Seven CV lanes required')
        for n in scene:struct.pack_into('<H',b,at,integer(n,0,1023));at+=2
    for s in p['steps']:
        if type(s['enabled']) is not bool:raise ValueError('Enabled must be boolean')
        b[at:at+4]=bytes((integer(s['scene'],1,8)-1,integer(s['probability'],0,100),integer(s['ratchets'],1,8),int(s['enabled'])));at+=4
    b[at:at+8]=bytes((integer(p['variation'],0,100),integer(p['cv_lead_ms'],1,20),integer(p['pulse_ms'],1,10),integer(p['swing'],0,50),choice(p['clock_source'],('INT','EXT')),choice(p['ppqn'],PPQN),choice(p['run_input'],RUNS),choice(p['variation_cv'],SOURCES)));at+=8
    for c in p['calibration']:struct.pack_into('<HH',b,at,integer(c['zero'],0,4095),integer(c['positive_5v'],0,4095));at+=4
    if type(p['cv_swap']) is not bool:raise ValueError('Swap must be boolean')
    b[at]=int(p['cv_swap']);at+=1
    if schema==1:return bytes(b)
    if schema!=2:raise ValueError('Unsupported BIA payload version')
    if len(p['step_accents'])!=16 or len(p['motion'])!=8:raise ValueError('Invalid motion dimensions')
    for n in p['step_accents']:b[at]=integer(n,0,100);at+=1
    struct.pack_into('<H',b,at,integer(p['fill_mask'],0,65535));at+=2;bits=0
    for i,m in enumerate(p['motion']):
        for prefix in ('fold','harm'):
            ms=integer(m[prefix+'_ms'],5,500)
            if ms%5:raise ValueError('Envelope duration must use 5 ms increments')
            b[at:at+2]=bytes((integer(m[prefix+'_offset'],-100,100)&255,ms//5));at+=2
        if len(m['accent'])!=3 or len(m['variation'])!=6 or type(m['retrigger']) is not bool:raise ValueError('Invalid scene motion')
        for n in m['accent']:b[at]=integer(n,-100,100)&255;at+=1
        for n in m['variation']:b[at]=integer(n,0,100);at+=1
        if m['retrigger']:bits|=1<<i
    b[at]=bits;b[at+1]=choice(p['performance']['effect'],('FILL','ACCENT','VAR'));at+=2
    if len(p['performance']['strengths'])!=3:raise ValueError('Three effect strengths required')
    for n in p['performance']['strengths']:b[at]=integer(n,0,100);at+=1
    return bytes(b)

def decode(b):
    if len(b)!=334 or b[:3]!=b'BIA' or b[3] not in (1,2) or b[198]>1:raise ValueError('Invalid BIA payload')
    p=dict(tempo_bpm=struct.unpack_from('<H',b,4)[0],scenes=[],steps=[]);at=6
    for _ in range(8):p['scenes'].append(list(struct.unpack_from('<7H',b,at)));at+=14
    for _ in range(16):
        if b[at+3]>1:raise ValueError('Invalid step flag')
        p['steps'].append(dict(scene=b[at]+1,probability=b[at+1],ratchets=b[at+2],enabled=bool(b[at+3])));at+=4
    p.update(variation=b[at],cv_lead_ms=b[at+1],pulse_ms=b[at+2],swing=b[at+3],clock_source=('INT','EXT')[b[at+4]],ppqn=PPQN[b[at+5]],run_input=RUNS[b[at+6]],variation_cv=SOURCES[b[at+7]]);at+=8
    p['calibration']=[dict(zip(('zero','positive_5v'),struct.unpack_from('<HH',b,at+i*4))) for i in range(2)];p['cv_swap']=bool(b[198]);p=upgrade(p);at=199
    if b[3]==2:
        p['step_accents']=list(b[at:at+16]);at+=16;p['fill_mask']=struct.unpack_from('<H',b,at)[0];at+=2
        signed=lambda n:n if n<128 else n-256
        for m in p['motion']:
            m.update(fold_offset=signed(b[at]),fold_ms=b[at+1]*5,harm_offset=signed(b[at+2]),harm_ms=b[at+3]*5);at+=4
            m['accent']=[signed(n) for n in b[at:at+3]];at+=3;m['variation']=list(b[at:at+6]);at+=6
        bits=b[at];at+=1
        for i,m in enumerate(p['motion']):m['retrigger']=bool(bits&(1<<i))
        p['performance']=dict(effect=('FILL','ACCENT','VAR')[b[at]],strengths=list(b[at+1:at+4]));at+=4
    if any(b[at:]):raise ValueError('Nonzero reserved bytes')
    encode(p);return p

def validate(lib):
    if lib['format']!='newfangled-bia-presets' or type(lib['version']) is not int or lib['version'] not in (1,2) or not isinstance(lib['slots'],list) or len(lib['slots'])>8:raise ValueError('Not a BIA preset library')
    slots={}
    for s in lib['slots']:
        n=integer(s['slot'],1,8)
        if n in slots:raise ValueError('Duplicate slot')
        slots[n]=encode(upgrade(s['patch']) if lib['version']==1 else s['patch'])
    boot=lib['boot_slot']
    if (slots and boot is None) or (boot is not None and (type(boot) is not int or boot not in slots)):raise ValueError('Boot slot missing')
    return slots

def newer(a,b):return 0<((a-b)&0xffffffff)<0x80000000

def record(slot,sequence,payload):
    decode(payload);b=bytearray(b'\xff'*384);b[:4]=b'BIP1';struct.pack_into('<HHB',b,4,6 if payload[3]==1 else 7,334,integer(slot,1,8)-1);struct.pack_into('<I',b,12,sequence&0xffffffff);b[16:350]=payload
    struct.pack_into('<I',b,376,zlib.crc32(b[:376]));struct.pack_into('<H',b,382,0xc33c);return bytes(b)

def scan(media):
    if len(media)!=8192:raise ValueError('Need exactly 8192 bytes')
    valid=[];future=False
    for bank in range(2):
        b=media[bank*4096:(bank+1)*4096]
        if b[-2:]!=b'K\xb4' or struct.unpack_from('<I',b,20)[0]!=zlib.crc32(b[:20]):continue
        if b[:4]==b'NFB1':future=True;continue
        if b[:4]!=b'BIB1':continue
        schema,size,generation,count=struct.unpack_from('<HHIH',b,4)
        if schema!=2:future=True;continue
        if size!=384 or count>8 or struct.unpack_from('<I',b,16)[0]!=zlib.crc32(b[384:384*(count+1)]):continue
        valid.append((bank,generation))
    bank=None;generation=0
    for index,g in valid:
        if bank is None or newer(g,generation):bank,generation=index,g
    slots={};sequence=0;boot=None
    if bank is not None:
        for i in range(9):
            at=bank*4096+384*(i+1);r=media[at:at+384]
            if r[:4]!=b'BIP1' or r[-2:]!=b'<\xc3' or struct.unpack_from('<I',r,376)[0]!=zlib.crc32(r[:376]):continue
            schema,size=struct.unpack_from('<HH',r,4)
            if schema not in (6,7):future=True;continue
            if size!=334 or r[8]>=8 or r[19]!=(1 if schema==6 else 2):continue
            try:decode(r[16:350])
            except (ValueError,IndexError):continue
            slot=r[8]+1;seq=struct.unpack_from('<I',r,12)[0]
            if slot not in slots or newer(seq,slots[slot][0]):slots[slot]=(seq,r[16:350])
            if boot is None or newer(seq,sequence):sequence,boot=seq,slot
    return dict(bank=bank,generation=generation,sequence=sequence,boot_slot=boot,slots={s:p[1] for s,p in slots.items()},future=future)

def library(state):return dict(format='newfangled-bia-presets',version=2,boot_slot=state['boot_slot'],slots=[dict(slot=s,patch=decode(p)) for s,p in sorted(state['slots'].items())])

def bank_image(slots,boot,generation,sequence):
    if len(slots)>8 or (boot is not None and boot not in slots):raise ValueError('Invalid BIA slots')
    b=bytearray(b'\xff'*4096);order=sorted(slots,key=lambda n:(n==boot,n))
    for i,s in enumerate(order):b[384*(i+1):384*(i+2)]=record(s,sequence+i+1,slots[s])
    b[:4]=b'BIB1';struct.pack_into('<HHIH',b,4,2,384,generation&0xffffffff,len(slots));struct.pack_into('<I',b,16,zlib.crc32(b[384:384*(len(slots)+1)]));struct.pack_into('<I',b,20,zlib.crc32(b[:20]));struct.pack_into('<H',b,4094,0xb44b);return bytes(b)
