from pathlib import Path
import struct, zlib, binascii

root = Path(__file__).resolve().parents[3]
def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', binascii.crc32(kind + data) & 0xffffffff)
def png(w, h, depth, color, rows, interlace=0, extra=b''):
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, depth, color, 0, 0, interlace)) + extra + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')
fixtures = {}
fixtures['RGBA'] = png(3, 2, 8, 6, b'\0' + bytes([255,0,0,255,0,255,0,128,255,0,255,0]) + b'\0' + bytes([0,0,255,255,255,255,255,255,0,0,0,0]))
fixtures['Palette'] = png(2, 2, 1, 3, b'\0\x40\0\x80', extra=chunk(b'PLTE', bytes([255,0,0,0,0,255]))+chunk(b'tRNS', bytes([255,0])))
fixtures['Gray'] = png(3, 1, 8, 0, bytes([0,0,127,255]))
fixtures['GrayAlpha16'] = png(2, 1, 16, 4, b'\0'+struct.pack('>HHHH',65535,32768,0,65535))
fixtures['RGB16'] = png(1, 1, 16, 2, b'\0'+struct.pack('>HHH',65535,32768,0))
def adam(x,y): return bytes([x*40,y*80,255-x*20,0 if x==4 else 128 if (x+y)%2 else 255])
rows = b''
for x,y,dx,dy in [(0,0,8,8),(4,0,8,8),(0,4,4,8),(2,0,4,4),(0,2,2,4),(1,0,2,2),(0,1,1,2)]:
    if x < 5 and y < 3:
        for sy in range(y,3,dy): rows += b'\0' + b''.join(adam(sx,sy) for sx in range(x,5,dx))
fixtures['Adam7'] = png(5,3,8,6,rows,1)
fixtures['Large'] = png(512,256,8,6,(b'\0'+bytes([255,0,0,255])*256+bytes([0,0,255,0])*256)*256)
fixtures['AlphaEdge'] = png(300,1,8,6,b'\0'+bytes([255,0,0,255])+bytes([0,0,255,0])*299)
fixtures['Transparent'] = png(2,2,8,6,(b'\0'+bytes([255,0,255,0])*2)*2)
fixtures['ShortRows'] = png(5,5,8,6,b'\0')
fixtures['LongRows'] = png(1,1,8,6,bytes(5000))
fixtures['TooWide'] = png(4097,1,8,6,b'\0')
fixtures['TooManyPixels'] = png(4096,4096,8,6,b'\0')
text = '// PNG fixtures generated with Python struct/zlib, independently of lodepng.\n#pragma once\n'
for name, data in fixtures.items():
    text += '\nstatic const BYTE Png' + name + '[] =\n{\n'
    for n in range(0,len(data),16): text += '\t'+','.join('0x%02x'%b for b in data[n:n+16])+',\n'
    text += '};\n'
(root/'platform/QuadooForm/DesignerTests/PngFixtures.h').write_text(text)
(root/'target').mkdir(exist_ok=True)
(root/'target/DesignerPNGImport-QA.png').write_bytes(fixtures['Large'])
(root/'target/DesignerPNGImport-invalid-QA.png').write_bytes(fixtures['ShortRows'])
