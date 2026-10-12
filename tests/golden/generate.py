import struct as s
from pathlib import Path

# Independent struct-based encoder: runtime output must match these bytes.
OUTPUT = Path(__file__).resolve().parent
u32=lambda x:s.pack('<I',x)
text=lambda x:u32(len(x.encode()))+x.encode()
def obj(t,v,fields):
    return s.pack('<IHH',t,v,len(fields))+b''.join(s.pack('<IBBHQ',i,w,0,0,len(p))+p for i,w,p in sorted(fields))
position=obj(3,1,[(i,4,s.pack('<f',v)) for i,v in enumerate([1.5,-2,3.25],1)])
items=[obj(4,1,[(1,1,s.pack('<Q',i)),(2,5,text(n))]) for i,n in [(7,'Sword'),(8,'Potion')]]
inventory=u32(len(items))+b''.join(s.pack('<Q',len(x))+x for x in items)
for version in (1,2):
    fields=[(1,1,s.pack('<Q',42)),(3,2 if version==1 else 3,s.pack('<i' if version==1 else '<q',100)),(4,6,position),(5,7,inventory)]
    if version==2: fields.append((2,5,text('Ada')))
    root=obj(1,1,[(1,5,text('Meadow')),(2,6,obj(2,version,fields))])
    data=b'CHRNLITE'+s.pack('<HHIQ',1,0,1,len(root))+root
    (OUTPUT / f'scene_v{version}.bin').write_bytes(data)
(OUTPUT / 'bad_magic.bin').write_bytes(b'BADMAGIC'+data[8:])
(OUTPUT / 'truncated.bin').write_bytes(data[:-3])

# V3 uses a new Stats object; keep the four historical fixtures above intact.
stats=obj(5,1,[(1,3,s.pack('<q',100)),(2,3,s.pack('<q',100))])
player=obj(2,3,[(1,1,s.pack('<Q',42)),(2,5,text('Ada')),(4,6,position),(5,7,inventory),(6,6,stats)])
root=obj(1,1,[(1,5,text('Meadow')),(2,6,player)])
v3=b'CHRNLITE'+s.pack('<HHIQ',1,0,1,len(root))+root
(OUTPUT / 'scene_v3.bin').write_bytes(v3)
