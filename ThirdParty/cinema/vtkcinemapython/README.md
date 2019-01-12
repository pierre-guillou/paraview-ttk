# Introduction

Extreme scale scientific simulations are leading a charge to exascale
computation, and data analytics runs the risk of being a bottleneck to
scientific discovery. Due to power and I/O constraints, we expect _in situ_
visualization and analysis will be a critical component of these workflows.
Options for extreme scale data analysis are often presented as a stark contrast:
write large files to disk for interactive, exploratory analysis, or perform in
situ analysis to save detailed data about phenomena that a scientists knows
about in advance. We present a novel framework for a third option – a highly
interactive, image-based approach that promotes exploration of simulation
results, and is easily accessed through extensions to widely used open source
tools. This _in situ_ approach supports interactive exploration of a wide range of
results, while still significantly reducing data movement and storage.

More information about the overall design of Cinema is available in the paper,
An Image-based Approach to Extreme Scale In Situ Visualization and Analysis,
which is available at the following link:
[https://datascience.lanl.gov/data/papers/SC14.pdf](https://datascience.lanl.gov/data/papers/SC14.pdf).

This repository contains a set of Python modules that make it easy to read
and write Cinema databases of various specs and formats. Our goal is to grow
this API as new functionality is added to Cinema by the community.

# Requirements

* Python 2.x
* numpy
* PIL (not required for output if VTK is available)
* VTK (optional)
* ParaView (optional)
* scipy (only required for unit tests)
* OpenEXR (optional)

# Contributing

See [CONTRIBUTING.md][CONTRIBUTING.md] for instructions to contribute.

[CONTRIBUTING.md]: CONTRIBUTING.md

# Installation of OpenEXR

OpenEXR 1.2.0 is supported as a database image format (currently only for depth images). This version of OpenEXR is readily available as a python egg and as a regular python module.
The module can be installed as a [python egg][OpenEXREgg] or as a [regular module][OpenEXRModule].

[OpenEXRModule]: https://pypi.python.org/pypi/OpenEX://pypi.python.org/pypi/OpenEXR
[OpenEXREgg]: http://excamera.com/sphinx/articles-openexr.html#openexrpython

# Basic Usage

If you have numpy, PIL and VTK installed, you can use one of the tests
to generate a simple Cinema database as follows.

```python
>>> from cinema_python import tests
>>> tests.test_vtk_clip("./info.json")
```
This should generate a number of png files and one json files that looks
similar to this:

```json
{"associations": {}, "arguments": {"theta": {"default": -180, "values": [-180, -140, -100, -60, -20, 20, 60, 100, 140, 180], "type": "range", "label": "theta"}, "phi": {"default": 0, "values": [0, 40, 80, 120, 160], "type": "range", "label": "phi"}, "offset": {"default": 0, "values": [0, 0.2, 0.4, 0.6, 0.8, 1.0], "type": "range", "label": "offset"}}, "name_pattern": "{phi}_{theta}_{offset}_slice.png", "metadata": {"type": "parametric-image-stack"}}
```

For details about the Cinema spec used, see the [Cinema specs repo](https://gitlab.kitware.com/cinema/specs).

You can view the generated database using the [Python Qt viewer](https://gitlab.kitware.com/cinema/qt-viewer) or the
[basic Web viewer](https://gitlab.kitware.com/cinema/basic-web-viewer).

Here is a simplified version of the code from `test_vtk_clip`.

```python
import explorers
import vtk_explorers
import vtk

# Setup a VTK pipeline
s = vtk.vtkSphereSource()

plane = vtk.vtkPlane()
plane.SetOrigin(0, 0, 0)
plane.SetNormal(-1, -1, 0)

clip = vtk.vtkClipPolyData()
clip.SetInputConnection(s.GetOutputPort())
clip.SetClipFunction(plane)
clip.GenerateClipScalarsOn()
clip.GenerateClippedOutputOn()
clip.SetValue(0)

m = vtk.vtkPolyDataMapper()
m.SetInputConnection(clip.GetOutputPort())

rw = vtk.vtkRenderWindow()
r = vtk.vtkRenderer()
rw.AddRenderer(r)

a = vtk.vtkActor()
a.SetMapper(m)
r.AddActor(a)

# make or open a cinema data store to put results in
cs = FileStore(fname)
# The png files created will be named using this pattern
cs.filename_pattern = "{phi}_{theta}_{offset}_slice.png"
# We add 3 parameters: 2 camera angles & the slice offset
cs.add_parameter("phi", make_parameter('phi', range(0, 200, 40)))
cs.add_parameter("theta", make_parameter('theta', range(-180,200,40)))
cs.add_parameter("offset", make_parameter('offset', [0,.2,.4,.6,.8,1.0]))

# This explorer will change the camera parameters (phi and theta) during generation
cam = vtk_explorers.Camera([0,0,0], [0,1,0], 3.0, r.GetActiveCamera()) #phi,theta implied
# Changes the slice offset
g = vtk_explorers.Clip('offset', clip)
# Combines all explorers to generate the images
e = vtk_explorers.ImageExplorer(cs, ['offset','phi', 'theta'], [cam, g], rw)

# run through all parameter combinations and put data into the store
e.explore()
```

The cinema module is also the recommended path for working with cinema stores that have been generated. The following code demonstrates the API for opening a store, searching for elements within it, and doing some trivial analysis of them.

```python
from cinema_python import cinema_store
import PIL.Image
import sys

def demonstrate_analyze(fname):
    """
    this demonstrates traversing an existing cinema store and doing some analysi
    (in this case just printing the contents) on each item"""

    cs = cinema_store.FileStore(fname)
    cs.load()

    print "PARAMETERS ARE"
    for parameter in cs.parameter_list:
        print parameter
        print cs.get_parameter(parameter)['values']

    print "ONE PARAMETER'S FIRST VALUE IS"
    param = cs.parameter_list.keys()[0]
    val = cs.get_parameter(param)['values'][0]
    print val

    print "HISTOGRAMS OF MATCHING RECORDS FOR", param, "=", val, "ARE"
    for doc in cs.find({param:val}):
        print doc.descriptor
        image = PIL.Image.fromarray(doc.data)
        print image.histogram()

if len(sys.argv) != 2:
  print "Usage: python demo.py /path/to/info.json"
  exit(0)

demonstrate_analyze(sys.argv[1])
```


