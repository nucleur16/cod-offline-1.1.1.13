"""Read-only C1 actor targeting inputs. PROCESS_VM_READ only; no input/debugger.
Prints actual actorInfo validity/team and matching centity type/bounds/flags,
beside predicted/snapshot crosshair flags. Layout is C1 1.1.1.13 only.
"""
import argparse,ctypes,json,struct,time
def main():
    p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--samples',type=int,default=1);p.add_argument('--interval',type=float,default=.25);a=p.parse_args()
    k=ctypes.WinDLL('kernel32',use_last_error=True)
    k.OpenProcess.argtypes=[ctypes.c_uint32,ctypes.c_int,ctypes.c_uint32];k.OpenProcess.restype=ctypes.c_void_p
    k.ReadProcessMemory.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
    k.CloseHandle.argtypes=[ctypes.c_void_p]
    h=k.OpenProcess(0x10,False,a.pid)
    if not h:raise ctypes.WinError(ctypes.get_last_error())
    def read(va,n):
        b=ctypes.create_string_buffer(n);got=ctypes.c_size_t()
        return b.raw if k.ReadProcessMemory(h,va,b,n,ctypes.byref(got)) and got.value==n else None
    def u32(va):
        b=read(va,4);return struct.unpack('<I',b)[0] if b else None
    def ptr(index,key):
        i=u32(index);v=u32(0x16d3b58+4*i) if i is not None and i<1024 else None
        return v^key if v is not None else None
    try:
        for sample in range(a.samples):
            cg=ptr(0x6c3d2ac,0x490b400c);ents=ptr(0x6c3d2b4,0x189bfead)
            out={'sample':sample,'cg':cg,'ents':ents,'actors':[]}
            if cg and ents:
                out.update(time=u32(cg+0xb0350),flags=u32(cg+0x344),target=u32(cg+0xbf04c),settle=[u32(cg+0xbf050),u32(cg+0xbf054)])
                client=u32(cg+0x140);out['viewer_team']=u32(cg+0x15ae24+client*0xb50+0x4c) if client is not None and client<19 else None
                snap=u32(cg+0x35e0);out['snapshot_flags']=u32(snap+0x344) if snap else None
                for slot in range(32):
                    info=cg+0x168514+slot*0x29c;valid=u32(info)
                    if not valid:continue
                    ent=u32(info+0xc);team=u32(info+0x14)
                    record={'slot':slot,'valid':valid,'ent':ent,'team':team}
                    if ent is not None and ent<2048:
                        cent=ents+ent*0x228;nextstate=cent+0xe4
                        record.update(current_type=struct.unpack('<H',read(cent+2,2))[0],next_type=u32(nextstate+4),flags=u32(nextstate+8),solid=u32(nextstate+0xa8),slot_tree=u32(nextstate+0x58),weapon=u32(nextstate+0xd4))
                    out['actors'].append(record)
            print(json.dumps(out),flush=True)
            if sample+1<a.samples:time.sleep(a.interval)
    finally:k.CloseHandle(h)
if __name__=='__main__':main()
