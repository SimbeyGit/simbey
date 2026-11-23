"""Author the 20px palette artwork and XP-compatible DIB ICOs.

Run manually with Pillow when changing artwork. Normal C++ builds use the checked
in ICOs and generated designer data; they do not need Python or Pillow.
"""
from pathlib import Path
import struct
import sys
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
SCALE = 4
INK = '#485b6c'
BLUE = '#377ab2'
PALE = '#eaf4fc'
WHITE = '#ffffff'
GRAY = '#a6b2bd'
GREEN = '#47946e'


def artwork(name):
    image = Image.new('RGBA', (80, 80))
    draw = ImageDraw.Draw(image)

    def rect(box, fill=WHITE, outline=INK):
        draw.rectangle(tuple(x * SCALE for x in box), fill=fill, outline=outline, width=SCALE)

    def line(points, fill=INK, width=1):
        draw.line([(x * SCALE, y * SCALE) for x, y in points], fill=fill, width=width * SCALE)

    def ellipse(box, fill=WHITE, outline=INK):
        draw.ellipse(tuple(x * SCALE for x in box), fill=fill, outline=outline, width=SCALE)

    def text_rows(x=6, y=5, count=4, bullets=False):
        for row in range(count):
            if bullets:
                rect((3, y + row * 3, 4, y + row * 3 + 1), BLUE, BLUE)
            line([(x, y + row * 3), (15, y + row * 3)], GRAY)

    if name == 'Calendar':
        rect((1, 3, 18, 18), WHITE)
        rect((2, 4, 17, 7), '#c85d53', '#c85d53')
        line([(5, 2), (5, 5)], INK)
        line([(14, 2), (14, 5)], INK)
        for y in (10, 13, 16):
            for x in (4, 7, 10, 13, 16):
                rect((x, y, x + 1, y + 1), GRAY, GRAY)
        rect((9, 12, 11.5, 14.5), BLUE, BLUE)
    elif name == 'DateTimePicker':
        rect((1, 3, 18, 18), WHITE)
        rect((2, 4, 17, 7), BLUE, BLUE)
        line([(5, 2), (5, 5)], INK)
        line([(14, 2), (14, 5)], INK)
        for y in (10, 13, 16):
            for x in (4, 8, 12):
                rect((x, y, x + 1, y + 1), BLUE, BLUE)
        ellipse((11, 10, 19, 18), PALE, INK)
        line([(15, 12), (15, 14), (17, 15)], BLUE)
    elif name == 'StatusBar':
        rect((1, 3, 19, 17), PALE)
        rect((2, 12, 18, 16), '#dce6ef', GRAY)
        line([(10, 12), (10, 16)], GRAY)
        line([(4, 14), (8, 14)], BLUE)
        line([(14, 16), (17, 13)], INK)
        line([(16, 16), (17, 15)], INK)
    elif name == 'ImageList':
        rect((1, 2, 15, 14), PALE)
        rect((4, 5, 18, 18), WHITE)
        for column, color in enumerate(('#508dc7', '#66b187', '#e8bd56')):
            x = 6 + column * 4
            rect((x, 8, x + 2, 12), color, color)
        line([(6, 15), (15, 15)], GRAY)
    elif name in ('MainMenu', 'PopupMenu', 'MenuItem'):
        rect((1, 3, 19, 17), PALE)
        rect((2, 4, 18, 7), WHITE, GRAY)
        line([(4, 5), (7, 5)], BLUE)
        line([(10, 5), (13, 5)], INK)
        if name != 'MainMenu':
            rect((4, 7, 16, 17), WHITE, GRAY)
            rect((5, 9, 15, 11), BLUE, BLUE)
            line([(7, 13), (13, 13)], INK)
            if name == 'MenuItem':
                line([(2, 9), (3, 10), (5, 8)], BLUE)
    elif name == 'Pointer':
        draw.polygon([(x*SCALE, y*SCALE) for x,y in [(4,2),(4,16),(8,12),(11,18),(14,16),(11,11),(17,11)]], fill=WHITE, outline=INK, width=SCALE)
    elif name in ('Form', 'Frame', 'GenericControl'):
        rect((2,3,18,17), PALE)
        rect((3,4,17,7), BLUE, BLUE)
        if name == 'Frame':
            rect((5,10,15,14), WHITE, GRAY)
        else:
            rect((12,12,16,15), WHITE)
    elif name == 'Button':
        rect((2,5,18,15), '#edf0f2')
        line([(3,6),(17,6)], WHITE)
        line([(7,9),(12,9)], BLUE)
        line([(7,11),(12,11)], BLUE)
    elif name == 'ImageButton':
        rect((1,4,19,16), '#edf0f2')
        line([(2,5),(18,5)], WHITE)
        rect((3,7,10,13), PALE, BLUE)
        draw.polygon([(4*SCALE,12*SCALE),(6*SCALE,9*SCALE),(8*SCALE,11*SCALE),(9*SCALE,10*SCALE),(9*SCALE,12*SCALE)], fill=GREEN)
        ellipse((4,8,5,9), '#efbd54', '#efbd54')
        line([(12,9),(16,9)], BLUE)
        line([(12,11),(15,11)], BLUE)
    elif name == 'CheckBox':
        rect((3,3,17,17))
        line([(6,10),(9,13),(15,6)], BLUE, 2)
    elif name == 'RadioButton':
        ellipse((3,3,17,17))
        ellipse((7,7,13,13), BLUE, BLUE)
    elif name in ('TextBox', 'ComboBox'):
        rect((1,5,19,15))
        line([(4,9),(9,9)], INK)
        line([(4,11),(8,11)], INK)
        if name == 'ComboBox':
            rect((13,6,18,14), PALE, GRAY)
            draw.polygon([(14*SCALE,9*SCALE),(17*SCALE,9*SCALE),(15.5*SCALE,12*SCALE)], fill=INK)
        else:
            line([(11,7),(11,13)], BLUE)
    elif name == 'ColorSwatch':
        for x,color in [(1,'#90bdbc'),(7,'#88c3d2'),(13,'#5e85ad')]:
            draw.rounded_rectangle((x*SCALE,5*SCALE,(x+6)*SCALE,15*SCALE),radius=2*SCALE,fill=color)
        draw.rounded_rectangle((7*SCALE,3*SCALE,13*SCALE,17*SCALE),radius=2*SCALE,fill='#88c3d2',outline='#6395a4',width=SCALE)
    elif name == 'Image':
        rect((1,3,19,17), PALE)
        ellipse((12,5,16,9), '#f5c65b', '#d2a544')
        draw.polygon([(2*SCALE,16*SCALE),(7*SCALE,8*SCALE),(12*SCALE,16*SCALE)], fill=GREEN)
        draw.polygon([(8*SCALE,16*SCALE),(13*SCALE,10*SCALE),(18*SCALE,16*SCALE)], fill=BLUE)
    elif name == 'Label':
        line([(3,16),(10,3),(17,16)], INK, 2)
        line([(6,11),(14,11)], INK, 2)
    elif name in ('ListBox', 'Memo'):
        rect((2,2,18,18))
        if name == 'ListBox':
            rect((3,8,17,11), PALE, PALE)
        text_rows(count=4, bullets=name == 'ListBox')
        if name == 'Memo':
            line([(16,4),(16,16)], GRAY)
            rect((15,7,17,11), PALE, BLUE)
    elif name in ('Panel', 'GroupBox', 'ScrollingPanel'):
        rect((2,4,18,17), '#f2f5f7')
        if name == 'GroupBox':
            rect((5,3,13,5), WHITE, WHITE)
            line([(6,4),(11,4)], INK)
        elif name == 'Panel':
            line([(4,7),(4,15),(16,15)], GRAY)
        else:
            rect((15,5,17,16), WHITE, GRAY)
            rect((3,14,14,16), WHITE, GRAY)
            rect((15,8,17,11), PALE, BLUE)
            rect((7,14,10,16), PALE, BLUE)
    elif name in ('TabControl', 'TabPage'):
        rect((2,6,18,17), PALE if name == 'TabControl' else WHITE)
        rect((2,3,9,7), WHITE)
        line([(3,7),(8,7)], WHITE)
        if name == 'TabControl':
            rect((10,4,16,6), '#cbdceb', BLUE)
            rect((5,10,14,14), WHITE, GRAY)
    elif name == 'OpenFile':
        draw.polygon([(2*SCALE,6*SCALE),(2*SCALE,17*SCALE),(17*SCALE,17*SCALE),(18*SCALE,7*SCALE),(10*SCALE,7*SCALE),(8*SCALE,4*SCALE),(3*SCALE,4*SCALE)], fill='#f8d778', outline='#90723a', width=SCALE)
        draw.polygon([(2*SCALE,17*SCALE),(5*SCALE,9*SCALE),(19*SCALE,9*SCALE),(16*SCALE,17*SCALE)], fill='#ffe6a1', outline='#90723a', width=SCALE)
    elif name == 'SaveFile':
        rect((3,2,17,18), BLUE)
        rect((6,3,14,8))
        rect((6,11,14,17))
        line([(12,4),(12,6)], BLUE, 2)
        line([(8,13),(12,13)], GRAY)
        line([(8,15),(12,15)], GRAY)
    elif name == 'ColorDialog':
        rect((2,2,18,18), PALE)
        rect((3,3,17,5), BLUE, BLUE)
        for row, colors in enumerate((('#ee7068','#e8bd56','#66b187'),('#50bac6','#508dc7','#ae78bb'))):
            for column, color in enumerate(colors):
                rect((4+column*4,7+row*5,7+column*4,11+row*5),color,color)
    elif name == 'Timer':
        ellipse((2,2,18,18))
        line([(10,5),(10,10),(14,12)], BLUE, 2)
        for x,y in [(10,3),(17,10),(10,17),(3,10)]:
            ellipse((x-.5,y-.5,x+.5,y+.5), GRAY, GRAY)
    elif name == 'ListView':
        rect((1, 2, 19, 18), WHITE)
        rect((2, 3, 18, 6), '#c8dff1', BLUE)
        rect((2, 11, 18, 13), '#d9eafa', '#d9eafa')
        for x in (8, 14):
            line([(x, 3), (x, 17)], GRAY)
        for y in (9, 13, 16):
            line([(3, y), (6, y)], BLUE)
            line([(10, y), (12, y)], INK)
            line([(16, y), (17, y)], GRAY)
    elif name == 'Canvas':
        rect((1, 2, 19, 18), WHITE, INK)
        ellipse((4, 5, 11, 12), '#a9d8f5', BLUE)
        line([(3, 15), (9, 10), (13, 14), (17, 6)], GREEN, 2)
        line([(12, 4), (17, 9)], '#d17c32', 2)
    elif name == 'ProgressBar':
        rect((1, 6, 19, 14), WHITE, INK)
        for x in (3, 6, 9, 12):
            rect((x, 8, x + 2, 12), GREEN, GREEN)
    elif name == 'TreeView':
        rect((1,1,19,19), '#ffffff', GRAY)
        line([(5,5),(5,14),(10,14)], BLUE)
        line([(5,9),(10,9)], BLUE)
        rect((3,3,7,7), '#f2d273', INK)
        rect((10,7,16,11), '#d9e8f6', BLUE)
        rect((10,12,16,16), '#d9e8f6', BLUE)
    elif name in ('GenericComponent','ExternalService'):
        draw.polygon([(10*SCALE,2*SCALE),(18*SCALE,6*SCALE),(18*SCALE,14*SCALE),(10*SCALE,18*SCALE),(2*SCALE,14*SCALE),(2*SCALE,6*SCALE)], fill='#eadff5' if name == 'ExternalService' else PALE, outline=INK, width=SCALE)
        line([(2,6),(10,10),(18,6)], BLUE)
        line([(10,10),(10,18)], BLUE)
        if name == 'ExternalService':
            ellipse((7,5,13,11), '#a263ba', '#73448b')
    elif name == 'ExternalMeter':
        rect((2,3,18,17), '#e4f3e9', GREEN)
        rect((5,11,7,14), GREEN, GREEN)
        rect((9,8,11,14), GREEN, GREEN)
        rect((13,5,15,14), GREEN, GREEN)
    elif name == 'Unavailable':
        draw.polygon([(10*SCALE,2*SCALE),(19*SCALE,17*SCALE),(1*SCALE,17*SCALE)], fill='#ffe39b', outline='#92712e', width=SCALE)
        line([(10,7),(10,12)], INK, 2)
        ellipse((9,14,11,16), INK, INK)
    else:
        raise ValueError(name)
    return image.resize((20,20), Image.Resampling.LANCZOS)


def dib(image):
    pixels = image.load()
    color = b''.join(bytes((b,g,r,a)) for y in range(19,-1,-1) for x in range(20) for r,g,b,a in [pixels[x,y]])
    mask = b''
    for y in range(19,-1,-1):
        bits = sum((pixels[x,y][3] == 0) << (31-x) for x in range(20))
        mask += struct.pack('>I', bits)
    return struct.pack('<IiiHHIIiiII',40,20,40,1,32,0,len(color),0,0,0,0) + color + mask


names = ['Calendar','DateTimePicker','Canvas','StatusBar','ProgressBar','ImageButton','ListView','TreeView','ImageList','MainMenu','PopupMenu','MenuItem','ColorDialog','ColorSwatch','Form','Frame','Button','CheckBox','ComboBox','GroupBox','Image','Label','ListBox','Memo','Panel','RadioButton','ScrollingPanel','TabControl','TabPage','TextBox','OpenFile','SaveFile','Timer','Pointer','GenericControl','GenericComponent','Unavailable','ExternalMeter','ExternalService']
only = sys.argv[1] if len(sys.argv) > 1 else None
if only is not None and only not in names:
    raise ValueError(only)
images = {}
for name in names:
    image = images[name] = artwork(name)
    data = dib(image)
    header = struct.pack('<HHHBBBBHHII',0,1,1,20,20,0,0,1,32,len(data),22)
    if only is None or name == only:
        (ROOT / (name + '.ico')).write_bytes(header + data)
        if name in ('ListView', 'ImageButton', 'ProgressBar', 'StatusBar', 'Canvas', 'DateTimePicker', 'Calendar'):
            image.save(ROOT / (name + '.png'))

generated = ['// Generated by Icons/GenerateIcons.py. No host EXE resources required.', '#pragma once']
for name in ('Pointer','GenericControl','GenericComponent','Unavailable'):
    data = dib(images[name])
    generated.append('static const BYTE Icon%s[] =\n{' % name)
    generated.extend('\t' + ','.join('0x%02x' % b for b in data[n:n+24]) + ',' for n in range(0,len(data),24))
    generated.append('};')
if only is None or only in ('Pointer','GenericControl','GenericComponent','Unavailable'):
    (ROOT.parent / 'Designer' / 'DesignerIconData.h').write_text('\n'.join(generated) + '\n')

atlas = Image.new('RGB',(480, len(names)*28 + 8),'#f1f3f5')
draw = ImageDraw.Draw(atlas)
for n,name in enumerate(names):
    y = n*28+4
    for x,color in [(4,'#ffffff'),(34,'#235b91'),(64,'#303840')]:
        draw.rectangle((x,y,x+23,y+23),fill=color)
        atlas.paste(images[name],(x+2,y+2),images[name])
    draw.text((100,y+5),name,fill='#25364a')
atlas.save(ROOT / 'PalettePreview.png')
print('Wrote %s20x20 DIB artwork and preview.' % ((only + ' ') if only else (str(len(names)) + ' ')))
