import sys
import PIL.Image
import PIL.ImImagePlugin
import numpy as np
import zlib

if len(sys.argv) < 2:
    print "supply filename of a raster file"
    exit(0)

rescale = False

if sys.argv[1] == "-R":
    rescale = True

zlib_width = 100
zlib_height = 100
acnt = 0
while acnt < len(sys.argv):
    if sys.argv[acnt] == "-zw":
        acnt = acnt + 1
        zlib_width = int(sys.argv[acnt])
    if sys.argv[acnt] == "-zh":
        acnt = acnt + 1
        zlib_height = int(sys.argv[acnt])
    acnt = acnt + 1

fname = sys.argv[-1]

index = fname.rfind('.')
ext = fname[index:]
if ext == ".npz":
    file = open(fname, mode='r')
    tz = np.load(file)  # like a tar
    imageslice = tz[tz.files[0]]
    tz.close()
    file.close()
    imageslice = np.flipud(imageslice)
    if rescale:
        mm = [imageslice.min(), imageslice.max()]
        print "WAS:", imageslice.shape, mm[0], mm[1]
        r = mm[1]-mm[0]
        if r != 0:
            imageslice = np.multiply(np.divide(np.subtract(
                imageslice, mm[0]), r), 255)
        else:
            imageslice = np.multiply(np.subtract(imageslice, mm[0]), 255)
    im = PIL.Image.fromarray(imageslice)
elif ext == ".Z":
    with open(fname, mode='r') as file:
        compresseddata = file.read()
        flatarr = np.fromstring(zlib.decompress(compresseddata),
                                np.float32)
        im = PIL.Image.fromarray(flatarr.reshape((zlib_width, zlib_height)))
else:
    im = PIL.Image.open(fname)
asnumpy = np.array(im, np.float32)
print asnumpy.shape, asnumpy.min(), asnumpy.max()
im.show()
