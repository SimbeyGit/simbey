"""Author the QuadooPlugin package's transparent 20px icon.

Uses the palette's existing ink/blue/gold colors and supersampled drawing style.
Run manually with Pillow; the designer will consume the PNG, not this script.
"""
from pathlib import Path
from PIL import Image, ImageDraw

SCALE = 4
INK = '#485b6c'
BLUE = '#377ab2'
PALE = '#eaf4fc'
GOLD = '#f8d778'

image = Image.new('RGBA', (20 * SCALE, 20 * SCALE))
draw = ImageDraw.Draw(image)


def box(bounds, fill, outline=INK, radius=0):
    bounds = tuple(value * SCALE for value in bounds)
    if radius:
        draw.rounded_rectangle(bounds, radius=radius * SCALE, fill=fill,
                               outline=outline, width=SCALE)
    else:
        draw.rectangle(bounds, fill=fill, outline=outline, width=SCALE)


# A blue replaceable module with two gold connector pins and a small socket.
box((12, 6, 17, 8), GOLD)
box((12, 12, 17, 14), GOLD)
box((2, 3, 13, 17), PALE, radius=2)
box((3, 4, 6, 16), BLUE, BLUE, radius=1)
box((8, 7, 11, 13), BLUE, radius=1)
draw.line(((9 * SCALE, 8 * SCALE), (9 * SCALE, 11 * SCALE)),
          fill='#ffffff', width=SCALE)

image = image.resize((20, 20), Image.Resampling.LANCZOS)
destination = Path(__file__).resolve().with_name('QuadooPlugin.png')
image.save(destination)
print(destination)
