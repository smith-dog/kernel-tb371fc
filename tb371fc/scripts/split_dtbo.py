import struct
d = open('/mnt/d/work/code-work/project/tb371fc-kernel/out/dtbo_a.img','rb').read()
fields = struct.unpack('>8I', d[:32])
magic, tsize, hdr, esize, ecount, eoff, psize = fields[:7]
print(hex(magic), tsize, hdr, esize, ecount, eoff, psize)
for i in range(ecount):
    off = eoff + i*esize
    sz, offdt = struct.unpack('>II', d[off:off+8])
    ident = struct.unpack('>I', d[off+8:off+12])[0]
    print(i, 'size', sz, 'off', offdt, 'id', hex(ident))
    open('/mnt/d/work/code-work/project/tb371fc-kernel/out/dtbo-dts/dtbo_%d.dtb' % i, 'wb').write(d[offdt:offdt+sz])
