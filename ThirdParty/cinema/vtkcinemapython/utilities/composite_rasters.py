import sys
import PIL.Image
import PIL.ImImagePlugin
import numpy as np

if len(sys.argv) != 5:
    print("usage: depth_raster_obj1 value_raster_obj1" +
          "depth_raster_obj2 value_raster_obj2 ")
    exit(0)


def get_ir(fname):
    index = fname.rfind('.')
    ext = fname[index:]
    if ext == ".npz":
        file = open(fname, mode='r')
        tz = np.load(file)
        imageslice = tz[tz.files[0]]
        tz.close()
        file.close()
        imageslice = np.flipud(imageslice)
        return imageslice
    else:
        im = PIL.Image.open(fname)
        asnumpy = np.array(im)
        return asnumpy


o1_dr = get_ir(sys.argv[1])
o1_wide = np.dstack((o1_dr[:], o1_dr[:], o1_dr[:]))
o1_vr = get_ir(sys.argv[2])
o2_dr = get_ir(sys.argv[3])
o2_wide = np.dstack((o2_dr[:], o2_dr[:], o2_dr[:]))
o2_vr = get_ir(sys.argv[4])

res = np.where(o1_wide < o2_wide, o1_vr, o2_vr)
im = PIL.Image.fromarray(res)
im.show()
