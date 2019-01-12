""" set of routines that evaluate raster results for the purposes of
regression testing them. In particular this will do a soft comparison
of two images to see that they look 'close' """

from numpy import linalg as la
from scipy.signal import correlate2d as c2d

import vtk
from vtk.numpy_interface import dataset_adapter as dsa
import numpy


def rgb2grey(image):
    image_lum = image.sum(2) / 3.0
    return image_lum


def normalizeForCC(image):
    image_n = (image - image.mean()) / image.std()
    return image_n


def compare_ncc(image1, image2):
    image1_n = normalizeForCC(rgb2grey(image1))
    image2_n = normalizeForCC(rgb2grey(image2))

    # take the mean in case there are suble differences in size
    return float((c2d(image1_n, image2_n, 'valid') / image1_n.size).mean())


def compare_l2(image1, image2):
    return float(abs(la.norm(image1) - la.norm(image2)))


def vtkRenderToArray(rw, mode="color"):
    # Render & capture image
    rw.Render()

    w2i = vtk.vtkWindowToImageFilter()
    w2i.SetInput(rw)

    if mode is "color":
        w2i.SetInputBufferTypeToRGB()
    elif mode is "Z":
        w2i.SetInputBufferTypeToZBuffer()

    w2i.Update()

    image = w2i.GetOutput()

    npview = dsa.WrapDataObject(image)
    idata = npview.PointData[0]
    ext = image.GetExtent()
    width = ext[1] - ext[0] + 1
    height = ext[3] - ext[2] + 1
    if image.GetNumberOfScalarComponents() == 1:
        imageslice = idata.reshape(height, width)
    else:
        imageslice = idata.reshape(
            height, width, image.GetNumberOfScalarComponents())
    return imageslice


def pvRenderToArray(view_proxy):
    image = view_proxy.CaptureWindow(1)
    npview = dsa.WrapDataObject(image)
    idata = npview.PointData[0]
    ext = image.GetExtent()
    width = ext[3] - ext[2] + 1
    height = ext[1] - ext[0] + 1

    if image.GetNumberOfScalarComponents() == 1:
        imageslice = numpy.flipud(idata.reshape(width, height, 3))
    else:
        imageslice = numpy.flipud(idata.reshape(
            width, height, image.GetNumberOfScalarComponents()))

    return imageslice
