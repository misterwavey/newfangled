"""Newfangled preset wire format and human-readable JSON; no vendor data."""
import struct
import zlib
FIELDS=('ratio','shape','width','phase','level','offset')
RATIOS=('/16','/8','/4','/3','/2','X1','X2','X3','X4','X8','X16')
SHAPES=('GATE','TRIANGLE','RAMP UP','RAMP DOWN','RANDOM')
PPQN=(1,2,4,8,12,16,24,48)
RUNS=('OFF','GATE','RESET')
SOURCES=('OFF','CV1','CV2')
BASE=0x08006000
BANK_BYTES=4096
STORE_BYTES=8192

def integer(value,low,high):
    if type(value) is not int or not low<=value<=high:raise ValueError('Preset number out of range')
    return value

def choice(value,values):
    if isinstance(value,bool):raise ValueError('Invalid preset choice')
    return values.index(value)

def encode(p,legacy=False,schema=3):
    if legacy:schema=1
    length={1:208,2:232,3:212}[schema];b=bytearray(length)
    struct.pack_into('<H',b,0,integer(p['tempo_bpm'],10,300))
    b[2:6]=bytes((choice(p['clock_source'],('INT','EXT')),choice(p['ppqn'],PPQN),choice(p['run_input'],RUNS),int(p['cv_swap'])))
    if type(p['cv_swap']) is not bool:raise ValueError('CV swap must be boolean')
    if len(p['calibration'])!=2 or len(p['outputs'])!=8:raise ValueError('Invalid preset dimensions')
    for i,c in enumerate(p['calibration']):struct.pack_into('<HH',b,8+i*4,integer(c['zero'],0,4095),integer(c['positive_5v'],0,4095))
    for i,o in enumerate(p['outputs']):
        for f,name in enumerate(FIELDS):
            b[16+i*6+f]=choice(o[name],RATIOS) if f==0 else choice(o[name],SHAPES) if f==1 else integer(o[name],1 if f==2 else 0,99 if f<4 else 100)
            r=o['cv'][name];source=choice(r['source'],SOURCES);depth=integer(r['depth'],-100,100);bias=integer(r['bias'],-100,100)
            if schema<3:struct.pack_into('<Bbb',b,64+(i*6+f)*3,source,depth,bias)
            else:
                bit=(i*6+f)*18;at=64+bit//8;v=(source|((depth&255)<<2)|((bias&255)<<10))<<(bit%8)
                for j in range(3):b[at+j]|=(v>>(j*8))&255
        if schema==1:continue
        r=o['rhythm'];steps=integer(r['steps'],1,32)
        if type(r['loop']) is not bool:raise ValueError('Loop must be boolean')
        bits=(steps-1)|(integer(r['hits'],0,steps)<<5)|(integer(r['rotate'],0,steps-1)<<11)|(integer(r['probability'],0,100)<<16)|(int(r['loop'])<<23)
        at=208+i*3 if schema==2 else 172+i*5;b[at:at+3]=bits.to_bytes(3,'little')
        if schema==3:
            g=o['groove'];b[at+3]=integer(g['swing'],0,50);b[at+4]=integer(g['ratchet'],1,8)
    return bytes(b)

def decode(b):
    if len(b) not in (208,232,212) or b[6:8]!=b'\0\0':raise ValueError('Invalid payload')
    p=dict(tempo_bpm=struct.unpack_from('<H',b)[0],clock_source=('INT','EXT')[b[2]],ppqn=PPQN[b[3]],run_input=RUNS[b[4]],cv_swap=bool(b[5]),
           calibration=[dict(zip(('zero','positive_5v'),struct.unpack_from('<HH',b,8+i*4))) for i in range(2)],outputs=[])
    if b[5]>1:raise ValueError('Invalid swap')
    for i in range(8):
        o={name:RATIOS[b[16+i*6]] if f==0 else SHAPES[b[17+i*6]] if f==1 else b[16+i*6+f] for f,name in enumerate(FIELDS)}
        at=208+i*3 if len(b)==232 else 172+i*5
        bits=int.from_bytes(b[at:at+3],'little') if len(b)!=208 else 32|(100<<16)
        o['rhythm']=dict(steps=(bits&31)+1,hits=(bits>>5)&63,rotate=(bits>>11)&31,probability=(bits>>16)&127,loop=bool(bits>>23))
        o['groove']=dict(swing=b[at+3],ratchet=b[at+4]) if len(b)==212 else dict(swing=0,ratchet=1)
        o['cv']={}
        for f,name in enumerate(FIELDS):
            if len(b)==212:
                bit=(i*6+f)*18;offset=64+bit//8;v=int.from_bytes(b[offset:offset+3],'little')>>(bit%8)
                source=v&3;depth=(v>>2)&255;bias=(v>>10)&255;depth=depth if depth<128 else depth-256;bias=bias if bias<128 else bias-256
            else:source,depth,bias=struct.unpack_from('<Bbb',b,64+(i*6+f)*3)
            o['cv'][name]=dict(source=SOURCES[source],depth=depth,bias=bias)
        p['outputs'].append(o)
    encode(p)
    return p

def validate(library):
    if library['format']!='newfangled-presets' or library['version'] not in (1,2,3) or type(library['version']) is not int:raise ValueError('Unsupported preset backup')
    if not isinstance(library['slots'],list) or len(library['slots'])>8:raise ValueError('Invalid slots')
    slots={}
    for slot in library['slots']:
        n=integer(slot['slot'],1,8)
        if n in slots:raise ValueError('Duplicate slot')
        p=slot['patch']
        if library['version']<3:
            p={**p,'outputs':[{**o,'groove':dict(swing=0,ratchet=1),**({'rhythm':dict(steps=1,hits=1,rotate=0,probability=100,loop=False)} if library['version']==1 else {})} for o in p['outputs']]}
        slots[n]=encode(p)
    boot=library['boot_slot']
    if slots and boot is None:raise ValueError('Nonempty library needs a boot slot')
    if boot is not None:
        integer(boot,1,8)
        if boot not in slots:raise ValueError('Boot slot must be present')
    return slots

def newer(a,b):return 0<((a-b)&0xffffffff)<0x80000000

def record(slot,sequence,payload):
    decode(payload);b=bytearray(b'\xff'*256);b[:4]=b'NFP1';struct.pack_into('<HHB',b,4,{208:1,232:2,212:3}[len(payload)],len(payload),slot-1);struct.pack_into('<I',b,12,sequence&0xffffffff);b[16:16+len(payload)]=payload
    struct.pack_into('<I',b,248,zlib.crc32(b[:248]));struct.pack_into('<H',b,254,0xc33c);return bytes(b)

def scan(media):
    if len(media)!=STORE_BYTES:raise ValueError('Need exactly 8192 bytes')
    valid=[];future=False
    for bank in range(2):
        b=media[bank*BANK_BYTES:(bank+1)*BANK_BYTES]
        if b[:4]!=b'NFB1' or b[-2:]!=b'K\xb4' or struct.unpack_from('<I',b,20)[0]!=zlib.crc32(b[:20]):continue
        schema,size,generation,count=struct.unpack_from('<HHIH',b,4)
        if schema!=1:future=True;continue
        if size!=256 or count>8 or struct.unpack_from('<I',b,16)[0]!=zlib.crc32(b[256:256+count*256]):continue
        valid.append((bank,generation))
    if not valid:return dict(bank=None,generation=0,sequence=0,slots={},boot_slot=None,future=future)
    bank,generation=valid[0]
    for candidate,gen in valid[1:]:
        if newer(gen,generation):bank,generation=candidate,gen
    slots={};sequence=0;boot=None
    for i in range(14):
        at=bank*BANK_BYTES+256*(i+1);r=media[at:at+256]
        if r[:4]!=b'NFP1' or r[-2:]!=b'<\xc3' or struct.unpack_from('<I',r,248)[0]!=zlib.crc32(r[:248]):continue
        schema,size=struct.unpack_from('<HH',r,4)
        if schema not in (1,2,3):future=True;continue
        if size!={1:208,2:232,3:212}[schema] or r[8]>=8:continue
        try:decode(r[16:16+size])
        except (ValueError,IndexError):continue
        slot=r[8]+1;seq=struct.unpack_from('<I',r,12)[0]
        if slot not in slots or newer(seq,slots[slot][0]):slots[slot]=(seq,r[16:16+size])
        if boot is None or newer(seq,sequence):sequence,boot=seq,slot
    return dict(bank=bank,generation=generation,sequence=sequence,slots={s:p[1] for s,p in slots.items()},boot_slot=boot,future=future)

def library(state):
    return dict(format='newfangled-presets',version=3,boot_slot=state['boot_slot'],slots=[dict(slot=s,patch=decode(p)) for s,p in sorted(state['slots'].items())])

def bank_image(slots,boot,generation,sequence):
    b=bytearray(b'\xff'*BANK_BYTES)
    order=sorted(slots,key=lambda n:(n==boot,n))
    if boot is not None and boot not in slots:raise ValueError('Missing boot slot')
    if len(slots)>8:raise ValueError('Too many slots')
    for i,s in enumerate(order):
        integer(s,1,8);b[256*(i+1):256*(i+2)]=record(s,sequence+i+1,slots[s])
    b[:4]=b'NFB1';struct.pack_into('<HHIH',b,4,1,256,generation&0xffffffff,len(slots));struct.pack_into('<I',b,16,zlib.crc32(b[256:256*(len(slots)+1)]))
    struct.pack_into('<I',b,20,zlib.crc32(b[:20]));struct.pack_into('<H',b,BANK_BYTES-2,0xb44b);return bytes(b)
