"""Read defined function symbols from our ELF32 little-endian build artifacts."""
import struct

def function_symbols(data):
    if data[:7]!=b'\x7fELF\x01\x01\x01':raise ValueError('expected little-endian ELF32')
    shoff=struct.unpack_from('<I',data,32)[0]
    stride,count=struct.unpack_from('<HH',data,46)
    if stride<40 or shoff+stride*count>len(data):raise ValueError('invalid section table')
    sections=[struct.unpack_from('<10I',data,shoff+i*stride) for i in range(count)]
    result={}
    for section in sections:
        if section[1]!=2:continue
        offset,size,link,entry_size=section[4],section[5],section[6],section[9]
        if link>=count or entry_size<16 or offset+size>len(data):raise ValueError('invalid symbol table')
        strings_section=sections[link];strings=data[strings_section[4]:strings_section[4]+strings_section[5]]
        for at in range(offset,offset+size,entry_size):
            name,value,length,info,other,index=struct.unpack_from('<IIIBBH',data,at)
            if index and info&15==2:
                if name>=len(strings):raise ValueError('invalid symbol name')
                end=strings.find(b'\0',name)
                if end<0:raise ValueError('unterminated symbol name')
                result[strings[name:end].decode()]=value
    return result
