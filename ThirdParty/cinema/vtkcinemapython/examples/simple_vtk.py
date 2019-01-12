"""
Example script that creates a Cinema store using VTK.
The parameters are phi, theta (camera positions) and
contour. This script will create a contour.json file
as well as a bunch of images named based on the
filename_patter = {phi}/{theta}/{contour}.png
where the {} scope denotes variables that are replaced
by parameter values for each image.
You can view the output of this script using the
Cinema QtViewer using something like this:
>> python ~/Work/Cinema/qt-viewer/Cinema.py ./contour.json
"""

import cinema_python.adaptors.vtk.vtk_explorers as vtk_explorers
import cinema_python.database.store as store
import cinema_python.database.file_store as file_store
import vtk

# set up the visualization

rw = vtk.vtkRenderWindow()
rw.SetSize(800, 800)
r = vtk.vtkRenderer()
rw.AddRenderer(r)

s = vtk.vtkRTAnalyticSource()
s.SetWholeExtent(-50, 50, -50, 50, -50, 50)

cf = vtk.vtkContourFilter()
cf.SetInputConnection(s.GetOutputPort())
cf.SetInputArrayToProcess(
    0, 0, 0, "vtkDataObject::FIELD_ASSOCIATION_POINTS", "RTData")
cf.SetNumberOfContours(1)
cf.SetValue(0, 200)
cf.ComputeScalarsOn()
m = vtk.vtkPolyDataMapper()
m.SetInputConnection(cf.GetOutputPort())
a = vtk.vtkActor()
a.SetMapper(m)
r.AddActor(a)

rw.Render()
r.ResetCamera()

# Create a new Cinema store
cs = file_store.FileStore("./contour.json")
cs.filename_pattern = "{phi}/{theta}/{contour}.png"

# These are the parameters that we will have in the store
cs.add_parameter("phi", store.make_parameter('phi', range(0, 200, 40)))
cs.add_parameter(
    "theta", store.make_parameter('theta', range(-180, 200, 40)))
cs.add_parameter("contour", store.make_parameter('contour', [160, 200]))

# These objects are responsible of change VTK parameters during exploration
con = vtk_explorers.Contour('contour', cf, 'SetValue')
cam = vtk_explorers.Camera([0, 0, 0], [0, 1, 0], 300.0, r.GetActiveCamera())

# Let's create the store
e = vtk_explorers.ImageExplorer(
    cs, ['contour', 'phi', 'theta'], [cam, con], rw)
e.explore()

# Now let's load from the store

cs2 = file_store.FileStore("./contour.json")
cs2.load()

docs = []
for doc in cs2.find({'theta': -180, 'phi': 0, 'contour': 160}):
    print doc
    docs.append(doc.data)

print doc
