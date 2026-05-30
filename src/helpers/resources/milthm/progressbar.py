from PIL import Image

im = Image.new("RGBA", (512, 2))

for x in range(im.width):
    for y in range(im.height):
        p = x / (im.width - 1)
        a = 1.0 - (1.0 - p) ** 2.2
        im.putpixel((x, y), (255, 255, 255, int(a * 255)))

im.save("progressbar.png")
