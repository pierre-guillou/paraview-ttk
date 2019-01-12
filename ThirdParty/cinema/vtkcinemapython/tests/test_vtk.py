""" vtk level tests of cinema """

import unittest
import numpy as np
import cinema_python.database.store as store
import cinema_python.database.file_store as file_store
import cinema_python.adaptors.explorers as explorers
import cinema_python.adaptors.vtk.vtk_explorers as vtk_explorers

try:
    import cinema_python.images.OexrHelper as exr
except ImportError:
    print "Could not import OpenEXR, some tests will be disabled."

import vtk
import compare_helper as ch
import PIL as pil


class TestImageFileStore(unittest.TestCase):
    def clean_up(self, cs, fname):
        import os
        for doc in cs.find():
            os.remove(cs._get_filename(doc.descriptor))
        os.remove(fname)

    def test_basic(self):
        print "\nTEST BASIC"

        # a VTK program
        rw = vtk.vtkRenderWindow()
        rw.SetSize(1024, 768)
        r = vtk.vtkRenderer()
        rw.AddRenderer(r)

        s = vtk.vtkRTAnalyticSource()
        s.SetWholeExtent(-25, 25, -25, 25, -25, 25)

        cf = vtk.vtkContourFilter()
        cf.SetInputConnection(s.GetOutputPort())
        cf.SetInputArrayToProcess(0, 0, 0,
                                  "vtkDataObject::FIELD_ASSOCIATION_POINTS",
                                  "RTData")
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

        # Create a Cinema store
        fname = "/tmp/test_vtk_basic/info.json"
        cs = file_store.FileStore(fname)
        cs.add_metadata({'type': 'parametric-image-stack'})
        cs.add_metadata({'store_type': 'FS'})
        cs.add_metadata({'version': '0.0'})
        cs.filename_pattern = "{phi}_{theta}_{contour}.png"

        # These are the parameters that will vary in the store
        cs.add_parameter("phi", store.make_parameter(
            'phi', range(0, 200, 80)))
        cs.add_parameter("theta", store.make_parameter(
            'theta', range(-180, 200, 80)))
        cs.add_parameter("contour", store.make_parameter(
            'contour', [160, 200]))

        # These objects respond to changes in parameters during exploration
        con = vtk_explorers.Contour('contour', cf, 'SetValue')
        cam = vtk_explorers.Camera([0, 0, 0], [0, 1, 0], 150.0,
                                   r.GetActiveCamera())  # phi,theta implied

        # Runs through all the combinations and saves each result
        e = vtk_explorers.ImageExplorer(cs,
                                        ['contour', 'phi', 'theta'],
                                        [cam, con], rw)
        # Go.
        e.explore()

        # Manually reproduce the first entry in the store
        # First set the camera to {'theta': -180, 'phi': 0}
        doc = store.Document({'theta': -180, 'phi': 0,
                              'contour': 160})
        con.execute(doc)
        cam.execute(doc)
        imageslice = ch.vtkRenderToArray(rw)

        # Load the first entry from the store
        cs2 = file_store.FileStore(fname)
        cs2.load()
        docs = []
        for doc in cs2.find({'theta': -180, 'phi': 0, 'contour': 160}):
            docs.append(doc.data)

        # compare the two
        l2error = ch.compare_l2(imageslice, docs[0])
        ncc = ch.compare_ncc(imageslice, docs[0])
        success = (l2error < 1.0) and (ncc > 0.99)

        if not success:
            print "\n l2error: ", l2error, " ; ncc = ", ncc, "\n"

        self.assertTrue(success)
        # self.clean_up(cs, "./contour.json")

    def test_clip(self):
        print "\nTEST CLIP"
        # set up some processing task
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
        rw.SetSize(300, 200)
        r = vtk.vtkRenderer()
        rw.AddRenderer(r)
        a = vtk.vtkActor()
        a.SetMapper(m)
        r.AddActor(a)

        # make or open a cinema data store to put results in
        fname = "/tmp/test_vtk_clip/info.json"
        cs = file_store.FileStore(fname)
        cs.add_metadata({'type': 'parametric-image-stack'})
        cs.add_metadata({'store_type': 'FS'})
        cs.add_metadata({'version': '0.0'})
        cs.filename_pattern = "{phi}_{theta}_{offset}_slice.png"
        cs.add_parameter("phi", store.make_parameter(
            'phi', range(0, 200, 80)))
        cs.add_parameter("theta", store.make_parameter(
            'theta', range(-180, 200, 80)))
        cs.add_parameter("offset", store.make_parameter(
            'offset', [0, .2, .4]))

        # associate control points with parameters of the data store
        cam = vtk_explorers.Camera([0, 0, 0], [0, 1, 0], 3.0,
                                   r.GetActiveCamera())  # phi,theta implied
        g = vtk_explorers.Clip('offset', clip)
        e = vtk_explorers.ImageExplorer(cs,
                                        ['offset', 'phi', 'theta'],
                                        [cam, g], rw)

        # run through all parameter combinations and put data into the store
        rw.Render()
        e.explore()

        # Now let's reproduce an entry in the store
        doc = store.Document({'theta': -100, 'phi': 80, 'offset': .2})
        g.execute(doc)
        cam.execute(doc)
        imageslice = ch.vtkRenderToArray(rw)

        # Now load the same entry from the store
        cs2 = file_store.FileStore(fname)
        cs2.load()
        docs = []
        for doc in cs2.find({'theta': -100, 'phi': 80, 'offset': .2}):
            docs.append(doc.data)

        # compare the two
        l2error = ch.compare_l2(imageslice, docs[0])
        ncc = ch.compare_ncc(imageslice, docs[0])
        self.assertTrue((l2error < 1.0) and (ncc > 0.99))

        # self.clean_up(cs, "./info.json")

    def test_contour(self):
        print "\nTEST CONTOUR"
        # set up some processing task
        s = vtk.vtkRTAnalyticSource()
        s.SetWholeExtent(-50, 50, -50, 50, -50, 50)
        cf = vtk.vtkContourFilter()
        cf.SetInputConnection(s.GetOutputPort())
        cf.SetInputArrayToProcess(
            0, 0, 0,
            "vtkDataObject::FIELD_ASSOCIATION_POINTS", "RTData")
        cf.SetNumberOfContours(1)
        cf.SetValue(0, 100)

        m = vtk.vtkPolyDataMapper()
        m.SetInputConnection(cf.GetOutputPort())

        rw = vtk.vtkRenderWindow()
        r = vtk.vtkRenderer()
        rw.AddRenderer(r)

        a = vtk.vtkActor()
        a.SetMapper(m)
        r.AddActor(a)

        rw.Render()
        r.ResetCamera()

        # make or open a cinema data store to put results in

        fname = "/tmp/test_vtk_contour/info.json"
        cs = file_store.FileStore(fname)
        cs.add_metadata({'type': 'parametric-image-stack'})
        cs.add_metadata({'store_type': 'FS'})
        cs.add_metadata({'version': '0.0'})
        cs.filename_pattern = "{contour}_{color}.png"
        cs.add_parameter(
            "contour",
            store.make_parameter(
                'contour',
                [0, 25, 50, 75, 100, 125, 150, 175, 200, 225, 250]))
        cs.add_parameter(
            "color",
            store.make_parameter('color', ['white', 'red']))

        colorChoice = vtk_explorers.ColorList()
        colorChoice.AddSolidColor('white', [1, 1, 1])
        colorChoice.AddSolidColor('red', [1, 0, 0])

        # associate control points with parameters of the data store
        g = vtk_explorers.Contour('contour', cf, 'SetValue')
        c = vtk_explorers.Color('color', colorChoice, a)
        e = vtk_explorers.ImageExplorer(cs, ['contour', 'color'], [g, c], rw)

        # run through all parameter combinations and put data into the store
        e.explore()

        # Now let's reproduce an entry in the store

        # First set the parameters to {'contour': 75} and {'color': 'white'}
        g.execute(store.Document({'contour': 75}))
        c.execute(store.Document({'color': 'white'}))
        imageslice = ch.vtkRenderToArray(rw)

        # Now load the same entry from the store
        cs2 = file_store.FileStore(fname)
        cs2.load()
        docs = []
        for doc in cs2.find({'contour': 75, 'color': 'white'}):
            docs.append(doc.data)

        # compare the two
        l2error = ch.compare_l2(imageslice, docs[0])
        ncc = ch.compare_ncc(imageslice, docs[0])
        self.assertTrue((l2error < 1.0) and (ncc > 0.99))
        # self.clean_up(cs, "./info.json")

    def test_composite(self):
        print "\nTEST VTK LAYERS"

        # set up some processing task
        s = vtk.vtkRTAnalyticSource()
        s.SetWholeExtent(-50, 50, -50, 50, -50, 50)

        rw = vtk.vtkRenderWindow()
        r = vtk.vtkRenderer()
        rw.AddRenderer(r)

        ac1 = vtk.vtkArrayCalculator()
        ac1.SetInputConnection(s.GetOutputPort())
        ac1.SetAttributeModeToUsePointData()
        ac1.AddCoordinateVectorVariable("coords", 0, 1, 2)
        ac1.SetResultArrayName("Coords")
        ac1.SetFunction("coords")

        ac2 = vtk.vtkArrayCalculator()
        ac2.SetInputConnection(ac1.GetOutputPort())
        ac2.SetAttributeModeToUsePointData()
        ac2.AddCoordinateVectorVariable("coords", 0, 1, 2)
        ac2.SetResultArrayName("radii")
        ac2.SetFunction("mag(coords)")

        cf = vtk.vtkContourFilter()
        cf.SetInputConnection(ac2.GetOutputPort())
        cf.SetInputArrayToProcess(
            0, 0, 0, "vtkDataObject::FIELD_ASSOCIATION_POINTS", "radii")
        cf.SetNumberOfContours(1)
        cf.ComputeScalarsOff()
        cf.SetValue(0, 40)

        m = vtk.vtkPolyDataMapper()
        m.SetInputConnection(cf.GetOutputPort())
        a = vtk.vtkActor()
        a.SetMapper(m)
        r.AddActor(a)

        rw.Render()
        r.ResetCamera()

        # make a cinema data store by defining the things that vary
        fname = "/tmp/test_vtk_composite/info.json"
        cs = file_store.FileStore(fname)
        cs.add_metadata({'type': 'composite-image-stack'})
        cs.add_metadata({'store_type': 'FS'})
        cs.add_metadata({'version': '0.1'})
        cs.filename_pattern = "{phi}/{theta}/{vis}.png"
        cs.add_parameter(
            "phi",
            store.make_parameter('phi', range(0, 200, 50)))
        cs.add_parameter(
            "theta",
            store.make_parameter('theta', range(-180, 200, 45)))
        cs.add_layer("vis",
                     store.make_parameter("vis", ['contour']))
        contours = [15, 30, 55, 70, 85]
        cs.add_control("isoval",
                       store.make_parameter('isoval', contours))
        cs.assign_parameter_dependence("isoval", "vis", ['contour'])
        cs.add_field("color",
                     store.make_field('color',
                                      {'white': 'rgb',
                                       'red': 'rgb',
                                       'depth': 'depth',
                                       'lum': 'luminance',
                                       'RTData': 'value',
                                       'point_X': 'value',
                                       'point_Y': 'value',
                                       'point_Z': 'value'}),
                     "isoval", contours)

        # associate control points with parameters of the data store
        cam = vtk_explorers.Camera(
            [0, 0, 0], [0, 1, 0], 300.0, r.GetActiveCamera())
        showcontour = vtk_explorers.ActorInLayer('contour', a)
        layertrack = explorers.Layer('vis', [showcontour])
        controltrack = vtk_explorers.Contour('isoval', cf, 'SetValue')
        # additional specification necessary for the color field
        colorChoice = vtk_explorers.ColorList()
        colorChoice.AddSolidColor('white', [1, 1, 1])
        colorChoice.AddSolidColor('red', [1, 0, 0])
        colorChoice.AddDepth('depth')
        colorChoice.AddLuminance('lum')
        colorChoice.AddValueRender(
            'RTData',
            vtk.VTK_SCALAR_MODE_USE_POINT_FIELD_DATA, 'RTData', 0, [0, 250])
        colorChoice.AddValueRender(
            'point_X',
            vtk.VTK_SCALAR_MODE_USE_POINT_FIELD_DATA, 'Coords', 0, [-50, 50])
        colorChoice.AddValueRender(
            'point_Y',
            vtk.VTK_SCALAR_MODE_USE_POINT_FIELD_DATA, 'Coords', 1, [-50, 50])
        colorChoice.AddValueRender(
            'point_Z',
            vtk.VTK_SCALAR_MODE_USE_POINT_FIELD_DATA, 'Coords', 2, [-50, 50])
        colortrack = vtk_explorers.Color('color', colorChoice, a)

        paramNames = ['phi', 'theta', 'vis', 'isoval', 'color']
        trackList = [cam, layertrack, controltrack, colortrack]
        e = vtk_explorers.ImageExplorer(cs, paramNames, trackList, rw)
        colortrack.imageExplorer = e
        e.explore()

        cs.save()
        # todo: test is assumed to pass if it runs to completion
        # that is necessary but not necessarily sufficient

    def test_exr_export_z(self):
        print "\nTEST EXR"
        try:
            exr
        except NameError:
            print "OpenEXR test disabled: No OpenEXR available."
            return

        # generate a test image
        rw = vtk.vtkRenderWindow()
        rw.SetSize(1920, 1080)  # test HD resolution to compare sizes
        r = vtk.vtkRenderer()
        rw.AddRenderer(r)

        s = vtk.vtkRTAnalyticSource()
        s.SetWholeExtent(-25, 25, -25, 25, -25, 25)

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

        # Render & capture z-buffer
        image = ch.vtkRenderToArray(rw, "Z")
        del rw

        # save as im
        pil.Image.fromarray(image).save("/tmp/test_exr.im")
        loaded_im = np.array(pil.Image.open("/tmp/test_exr.im"))

        # save as exr
        exr.save_depth(image, "/tmp/test_exr.exr")

        # load saved file and compare
        loaded_exr = exr.load_depth("/tmp/test_exr.exr")
        self.assertTrue(np.all(image == loaded_exr) and
                        np.all(loaded_exr == loaded_im))


if __name__ == '__main__':
    unittest.main()
