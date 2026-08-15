from pathlib import Path
import struct

root=Path(__file__).resolve().parent
ico=(root/'zeta.ico').read_bytes()
reserved,itype,count=struct.unpack_from('<HHH',ico,0)
assert reserved==0 and itype==1 and count>0
entries=[]
for i in range(count):
    off=6+i*16
    w,h,cc,res,planes,bpp,size,img_off=struct.unpack_from('<BBBBHHII',ico,off)
    data=ico[img_off:img_off+size]
    entries.append((w,h,cc,res,planes,bpp,size,data))

def pad4(b:bytes)->bytes:
    return b+b'\0'*((-len(b))&3)

def res_entry(rtype:int,name:int,data:bytes,lang=0,flags=0x0030)->bytes:
    typ=struct.pack('<HH',0xffff,rtype)
    nam=struct.pack('<HH',0xffff,name)
    prefix=struct.pack('<II',len(data),0)
    middle=pad4(typ)+pad4(nam)
    tail=struct.pack('<IHHII',0,flags,lang,0,0)
    header=prefix+middle+tail
    header=struct.pack('<II',len(data),len(header))+header[8:]
    return header+pad4(data)

# Required initial null resource header.
out=struct.pack('<IIHHHHIHHII',0,32,0xffff,0,0xffff,0,0,0,0,0,0)
for idx,e in enumerate(entries,1):
    out += res_entry(3,idx,e[7]) # RT_ICON

grp=struct.pack('<HHH',0,1,count)
for idx,e in enumerate(entries,1):
    w,h,cc,res,planes,bpp,size,_=e
    grp += struct.pack('<BBBBHHIH',w,h,cc,res,planes,bpp,size,idx)
out += res_entry(14,1,grp) # RT_GROUP_ICON
(root/'app.res').write_bytes(out)
print(f'wrote app.res: {len(out)} bytes, {count} icon images')
