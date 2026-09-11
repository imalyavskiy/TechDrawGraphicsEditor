from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
scale = 4
size = 256
canvas = Image.new("RGBA", (size * scale, size * scale), (0, 0, 0, 0))
draw = ImageDraw.Draw(canvas)

def box(values):
    return tuple(int(value * scale) for value in values)

draw.rounded_rectangle(box((12, 12, 244, 244)), radius=42 * scale, fill=(42, 105, 168, 255), outline=(25, 65, 105, 255), width=7 * scale)
draw.ellipse(box((116, 116, 140, 140)), fill=(240, 247, 255, 255))
for target in ((30, 35), (226, 35), (28, 221), (228, 221), (128, 24), (128, 232)):
    draw.line((128 * scale, 128 * scale, target[0] * scale, target[1] * scale), fill=(184, 216, 244, 180), width=5 * scale)

body = [(67, 181), (166, 82), (190, 106), (91, 205)]
draw.polygon([box(point) for point in body], fill=(249, 154, 36, 255), outline=(28, 44, 61, 255), width=5 * scale)
draw.polygon([box(point) for point in ((166, 82), (183, 65), (207, 89), (190, 106))], fill=(233, 239, 244, 255), outline=(28, 44, 61, 255), width=5 * scale)
draw.polygon([box(point) for point in ((183, 65), (212, 60), (207, 89))], fill=(28, 44, 61, 255))
draw.polygon([box(point) for point in ((67, 181), (91, 205), (55, 217))], fill=(247, 220, 174, 255), outline=(28, 44, 61, 255), width=5 * scale)
draw.polygon([box(point) for point in ((55, 217), (67, 181), (72, 200))], fill=(28, 44, 61, 255))
draw.line((82 * scale, 190 * scale, 179 * scale, 93 * scale), fill=(255, 202, 91, 255), width=5 * scale)

image = canvas.resize((size, size), Image.Resampling.LANCZOS)
image.save(root / "resources" / "drawing.png")
image.save(root / "resources" / "drawing.ico", format="ICO", sizes=[(16, 16), (20, 20), (24, 24), (32, 32), (40, 40), (48, 48), (64, 64), (96, 96), (128, 128), (256, 256)])
