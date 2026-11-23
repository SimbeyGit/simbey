"""Author the picker form and its XP-compatible 16/32px icon.

Run manually with Pillow after changing the layout/artwork. Normal VC9 builds
use the checked-in JSON and ICO. VistaColors.cpp is read ONLY by this manual
authoring script; the resulting form contains every swatch at design time.
"""
from pathlib import Path
import colorsys
import base64
import json
import re
import struct
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
SCALE = 4
image = Image.new('RGBA', (32*SCALE, 32*SCALE))
draw = ImageDraw.Draw(image)
draw.rounded_rectangle((2*SCALE, 2*SCALE, 30*SCALE, 30*SCALE), radius=5*SCALE,
                       fill='#f1f5f9', outline='#485b6c', width=SCALE)
for row, colors in enumerate((('#ef6c63', '#f5c65b', '#72b68c'),
                             ('#58b9c8', '#538dc3', '#aa79b9'))):
    for column, color in enumerate(colors):
        x, y = 6 + column*7, 7 + row*10
        draw.rounded_rectangle((x*SCALE, y*SCALE, (x+5)*SCALE, (y+7)*SCALE),
                               radius=SCALE, fill=color)

entries, payloads = [], []
offset = 6 + 2*16
for side in (16, 32):
    icon = image.resize((side, side), Image.Resampling.LANCZOS)
    pixels = icon.load()
    color = b''.join(bytes((b,g,r,a)) for y in range(side-1,-1,-1)
                     for x in range(side) for r,g,b,a in [pixels[x,y]])
    stride = ((side + 31)//32)*4
    mask = b''.join(sum((pixels[x,y][3] == 0) << (stride*8-1-x)
                       for x in range(side)).to_bytes(stride, 'big')
                    for y in range(side-1,-1,-1))
    dib = struct.pack('<IiiHHIIiiII',40,side,side*2,1,32,0,len(color),0,0,0,0) + color + mask
    entries.append(struct.pack('<BBBBHHII',side,side,0,0,1,32,len(dib),offset))
    payloads.append(dib)
    offset += len(dib)
ico = struct.pack('<HHH',0,1,2) + b''.join(entries+payloads)
(ROOT.parent.parent / 'Icons' / 'ColorPicker.ico').write_bytes(ico)

font = {'Name':'Tahoma', 'Size':8, 'Bold':False, 'Italic':False}
label_font = dict(font, Size=7)
heading_font = dict(font, Size=9, Bold=True)

def control(kind, name, x, y, width, height, **properties):
    return {'type':kind, 'name':name, 'properties':dict(
        Left=x, Top=y, Width=width, Height=height, Font=font, **properties)}

def label(name, text, x, y, width, height=20):
    return control('Label', name, x, y, width, height, Caption=text)

# Author fixed 18x18 swatches. Neutral shades first (light to dark), then hue,
# with lighter colors first within each hue. Retain every named table entry,
# including aliases, so there is no hidden runtime dependency on VistaColors.
source = (ROOT.parents[3] / 'shared/library/VistaColors.cpp').read_text()
entries = re.findall(r'\{"([^"\n]+)"[^\n]*RGB\(0x([0-9A-Fa-f]+),0x([0-9A-Fa-f]+),0x([0-9A-Fa-f]+)\)', source)
assert entries
entries = [(name,tuple(int(v,16) for v in rgb)) for name,*rgb in entries]
def hue_key(entry):
    h,s,v = colorsys.rgb_to_hsv(*(v/255 for v in entry[1]))
    return (0, -v, 0, entry[0]) if s < 0.08 else (1,h,-v,s,entry[0])
entries.sort(key=hue_key)
columns = 21
palette = control('Panel','VistaPalette',16,38,columns*22-4,((len(entries)+columns-1)//columns)*22-4)
palette['children'] = []
raw_colors = []
for n,(name,(r,g,b)) in enumerate(entries):
    raw = r | g<<8 | b<<16
    raw_colors.append(raw)
    swatch = control('ColorSwatch','Vista'+str(n),(n%columns)*22,(n//columns)*22,18,18,
                     Mode='Fixed',Selected=0,Colors=[raw])
    swatch['events'] = {'OnClick':'ChooseSwatch'}
    palette['children'].append(swatch)
assert len(entries) <= columns*7
body = [palette, label('VistaHeading','Vista Colors',16,12,458),
        label('CurrentHeading','Current color (click to customize)',510,12,378),
        control('ColorSwatch','CurrentColor',510,34,378,80,Mode='Editable',Colors=[0]),
        control('TextBox','HexColor',510,124,210,24,TabOrder=0,Text='#000000'),
        label('ColorName','RGB: 0, 0, 0',510,154,378),
        label('HexHint','#RRGGBB or 0x800000nn (system color)',510,180,378)]
body[3]['events'] = {'OnClick':'ChooseSwatch'}
body[4]['events'] = {'OnChange':'EditHex'}
names = re.findall(r'L"([^"]*)"', (ROOT.parent / 'ColorNames.h').read_text().split('};')[0])
groups = [
    ('DesktopWorkspace','Desktop & Workspace',[1,12,0]),
    ('WindowsText','Windows & Text',[5,8,17,26]),
    ('Buttons3D','Buttons & 3D',[15,18,22,20,16,21]),
    ('Menus','Menus',[4,7,30,29]),
    ('CaptionsBorders','Captions & Borders',[2,9,27,3,19,28,10,11,6]),
    ('SelectionInfo','Selection & Info',[13,14,24,23]),
]
for row,(name,caption,colors) in enumerate(groups):
    y = 202+row*60
    heading = label(name+'Heading',caption,16,y,888)
    heading['properties']['Font'] = heading_font
    body.append(heading)
    swatch = control('ColorSwatch',name,16,y+20,888,32,Mode='Fixed',
                     Colors=[-2147483648+i for i in colors],Labels=[names[i][6:] for i in colors])
    swatch['properties']['Font'] = label_font
    swatch['events'] = {'OnClick':'ChooseSwatch'}
    body.append(swatch)
footer = control('Panel','Footer',0,564,920,48,Align='Bottom')
footer['children'] = [
    control('Button','AcceptColor',374,10,80,26,Caption='OK',Default=True,ModalResult=1,TabOrder=0,Anchors=2),
    control('Button','CancelColor',466,10,80,26,Caption='Cancel',Cancel=True,ModalResult=2,TabOrder=1,Anchors=2),
]
content = control('ScrollingPanel','ColorChoices',0,0,920,564,Align='Client',AutoScroll=True)
content['children'] = body
form = {'schemaVersion':1,'type':'Form','name':'ColorPicker',
        'properties':{'Caption':'Choose Color','Width':920,'Height':612,
                      'BorderStyle':'Single','BorderIcons':9,'Font':font,
                      'Icon':{'format':'ico','encoding':'base64','data':base64.b64encode(ico).decode('ascii')}},
        'events':{'OnCloseQuery':'CloseQuery','OnFocus':'FocusHex'},'children':[footer,content]}
(ROOT/'ColorPicker.qform.json').write_text(json.dumps(form,indent=2)+'\n',encoding='utf-8')
# Test fixtures are authored from the same asset but are never linked to production.
fixture = '// Generated by QuadooForm/Dialogs/GenerateAssets.py.\n#pragma once\n'
fixture += 'static const COLORREF PickerVistaColors[] = {\n'
fixture += ',\n'.join('\t'+','.join('0x%06x'%v for v in raw_colors[i:i+16]) for i in range(0,len(raw_colors),16))+'\n};\n'
fixture += 'static const struct { PCWSTR name; UINT count; UINT colors[9]; INT y; } PickerGroups[] = {\n'
fixture += ',\n'.join('\t{L"%s",%u,{%s},%u}'%(name,len(colors),','.join(map(str,colors)),222+row*60) for row,(name,_,colors) in enumerate(groups))+'\n};\n'
(ROOT.parents[1]/'DesignerTests/ColorPickerData.h').write_text(fixture)
print('Wrote',len(entries),'Vista swatches and',len(groups),'system strips into the form resource.')

