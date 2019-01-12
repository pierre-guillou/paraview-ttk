""" paraview level tests of cinema """

import unittest
import paraview.simple as pv
import cinema_python.database.store as store
import cinema_python.database.file_store as file_store
import cinema_python.adaptors.explorers as explorers
import cinema_python.adaptors.paraview.pv_explorers as pv_explorers
import compare_helper as ch


class TestBasicFileStore(unittest.TestCase):

    def clean_up(self, cs, fname):
        pass

    def test_slice(self):
        pv.Connect()  # using a dedicated server state for each test
        print "\nTEST_SLICE"

        # set up some processing task
        view_proxy = pv.CreateRenderView()
        view_proxy.OrientationAxesVisibility = 0
        s = pv.Sphere()
        sliceFilt = pv.Slice(
            SliceType="Plane", Input=s, SliceOffsetValues=[0.0])
        sliceFilt.SliceType.Normal = [0, 1, 0]
        sliceRep = pv.Show(sliceFilt)

        # make or open a cinema data store to put results in
        fname = "/tmp/test_pv_slice/info.json"
        cs = file_store.FileStore(fname)
        cs.add_metadata({'type': 'parametric-image-stack'})
        cs.add_metadata({'store_type': 'FS'})
        cs.add_metadata({'version': '0.0'})
        cs.filename_pattern = "{phi}_{theta}_{offset}_{color}_slice.png"
        cs.add_parameter(
            "phi", store.make_parameter('phi', [90, 120, 140]))
        cs.add_parameter(
            "theta", store.make_parameter('theta', [-90, -30, 30, 90]))
        cs.add_parameter(
            "offset",
            store.make_parameter('offset', [-.4, -.2, 0, .2, .4]))
        cs.add_parameter(
            "color",
            store.make_parameter(
                'color', ['yellow', 'cyan', "purple"], typechoice='list'))

        colorChoice = pv_explorers.ColorList()
        colorChoice.AddSolidColor('yellow', [1, 1, 0])
        colorChoice.AddSolidColor('cyan', [0, 1, 1])
        colorChoice.AddSolidColor('purple', [1, 0, 1])

        # associate control points with parameters of the data store
        cam = pv_explorers.Camera([0, 0, 0], [0, 1, 0], 10.0, view_proxy)
        filt = pv_explorers.Slice("offset", sliceFilt)
        col = pv_explorers.Color("color", colorChoice, sliceRep)

        params = ["phi", "theta", "offset", "color"]
        e = pv_explorers.ImageExplorer(
            cs, params, [cam, filt, col], view_proxy)
        # run through all parameter combinations and put data into the store
        e.explore()

        # Reproduce an entry and compare vs. loaded

        # First set the parameters to reproduce
        cam.execute(store.Document({'theta': -30, 'phi': 120}))
        filt.execute(store.Document({'offset': -.4}))
        col.execute(store.Document({'color': 'cyan'}))
        imageslice = ch.pvRenderToArray(view_proxy)

        # Now load the corresponding entry
        cs2 = file_store.FileStore(fname)
        cs2.load()
        docs = []
        for doc in cs2.find(
                {'theta': -30, 'phi': 120, 'offset': -.4, 'color': 'cyan'}):
            docs.append(doc.data)

        # print "gen entry: \n",
        #        imageslice, "\n",
        #        imageslice.shape,"\n",
        #        "loaded: \n",
        #        docs[0], "\n",
        #        docs[0].shape
        # compare the two
        l2error = ch.compare_l2(imageslice, docs[0])
        ncc = ch.compare_ncc(imageslice, docs[0])
        self.assertTrue((l2error < 1.0) and (ncc > 0.99))
        pv.Disconnect()  # using a dedicated server state for each test

    def test_contour(self):
        pv.Connect()  # using a dedicated server state for each test
        print "\nTEST_CONTOUR"

        # set up some processing task
        view_proxy = pv.CreateRenderView()
        view_proxy.OrientationAxesVisibility = 0
        view_proxy.ViewSize = [1024, 768]
        s = pv.Wavelet()
        contour = pv.Contour(Input=s, ContourBy='RTData', ComputeScalars=1)
        sliceRep = pv.Show(contour)

        # make or open a cinema data store to put results in
        fname = "/tmp/test_pv_contour/info.json"
        cs = file_store.FileStore(fname)
        cs.add_metadata({'type': 'parametric-image-stack'})
        cs.add_metadata({'store_type': 'FS'})
        cs.add_metadata({'version': '0.0'})
        cs.filename_pattern = "{phi}_{theta}_{contour}_{color}_contour.png"
        cs.add_parameter(
            "phi", store.make_parameter('phi', [90, 120, 140]))
        cs.add_parameter(
            "theta", store.make_parameter('theta', [-90, -30, 30, 90]))
        cs.add_parameter(
            "contour",
            store.make_parameter('contour', [50, 100, 150, 200]))
        cs.add_parameter(
            "color",
            store.make_parameter(
                'color', ['white', 'RTData_1'], typechoice='list'))

        # associate control points with parameters of the data store
        cam = pv_explorers.Camera(
            [0, 0, 0], [0, 1, 0], 75.0, view_proxy)
        filt = pv_explorers.Contour("contour", contour)

        colorChoice = pv_explorers.ColorList()
        colorChoice.AddSolidColor('white', [1, 1, 1])
        colorChoice.AddLUT('POINTS', 'RTData_1', 'X')
        col = pv_explorers.Color("color", colorChoice, sliceRep)

        params = ["phi", "theta", "contour", "color"]
        e = pv_explorers.ImageExplorer(
            cs, params, [cam, filt, col], view_proxy)

        # run through all parameter combinations and put data into the store
        e.explore()

        # Reproduce an entry and compare vs. loaded

        # First set the parameters to reproduce
        cam.execute(store.Document({'theta': 30, 'phi': 140}))
        filt.execute(store.Document({'contour': 100}))
        col.execute(store.Document({'color': 'RTData_1'}))

        imageslice = ch.pvRenderToArray(view_proxy)

        # Now load the corresponding
        cs2 = file_store.FileStore(fname)
        cs2.load()
        docs = []
        for doc in cs2.find(
                {'theta': 30, 'phi': 140,
                 'contour': 100, 'color': 'RTData_1'}):
            docs.append(doc.data)

        # compare the two
        l2error = ch.compare_l2(imageslice, docs[0])
        ncc = ch.compare_ncc(imageslice, docs[0])
        success = (l2error < 1.0) and (ncc > 0.99)
        if not success:
            print "\n l2-error = ", l2error, " ; ncc = ", ncc, "\n"
        self.assertTrue(success)
        pv.Disconnect()  # using a dedicated server state for each test

    def test_composite(self):
        pv.Connect()  # get a new context like a normal script would
        print "\nTEST_COMPOSITE"

        # set up some processing task
        view_proxy = pv.CreateRenderView()
        view_proxy.OrientationAxesVisibility = 0
        s = pv.Wavelet()
        contour = pv.Contour(Input=s, ContourBy='RTData', ComputeScalars=1)
        sliceRep = pv.Show(contour)

        # make or open a cinema data store to put results in
        fname = "/tmp/test_pv_composite/info.json"
        cs = file_store.FileStore(fname)
        cs.add_metadata({'type': 'composite-image-stack'})
        cs.add_metadata({'store_type': 'FS'})
        cs.add_metadata({'version': '0.1'})

        cs.filename_pattern = "results.png"
        cs.add_parameter(
            "phi", store.make_parameter('phi', [90, 120, 140]))
        cs.add_parameter(
            "theta", store.make_parameter('theta', [-90, -30, 30, 90]))
        cs.add_layer(
            "vis", store.make_parameter("vis", ['contour']))
        contours = [50, 100, 150, 200]
        cs.add_control("isoval",
                       store.make_parameter('isoval', contours))
        cs.assign_parameter_dependence("isoval", "vis", ['contour'])
        cs.add_field("color",
                     store.make_field('color',
                                      {'white': 'rgb',
                                       'depth': 'depth',
                                       'lum': 'luminance',
                                       'RTData_1': 'lut'},),
                     "isoval", contours)

        # associate control points with parameters of the data store
        cam = pv_explorers.Camera([0, 0, 0], [0, 1, 0], 75.0, view_proxy)
        showcontour = pv_explorers.SourceProxyInLayer("contour",
                                                      sliceRep, contour)
        layertrack = explorers.Layer("vis", [showcontour])
        filt = pv_explorers.Contour("isoval", contour)

        # additional specification necessary for the color field
        colorChoice = pv_explorers.ColorList()
        colorChoice.AddSolidColor('white', [1, 1, 1])
        colorChoice.AddLUT('POINTS', 'RTData_1', 'X')
        colorChoice.AddDepth('depth')
        colorChoice.AddLuminance('lum')

        col = pv_explorers.Color("color", colorChoice, sliceRep)

        paramNames = ["phi", "theta", "vis", "isoval", "color"]
        trackList = [cam, layertrack, filt, col]
        e = pv_explorers.ImageExplorer(cs,
                                       paramNames, trackList, view_proxy)

        # run through all parameter combinations and put data into the store
        e.explore()

        # Reproduce an entry and compare vs. loaded
        # First set the parameters to reproduce
        cam.execute(store.Document({'theta': 30, 'phi': 140}))
        filt.execute(store.Document({'isoval': 100}))
        col.execute(store.Document({'color': 'RTData_1'}))
        imageslice = ch.pvRenderToArray(view_proxy)

        # Now load the corresponding
        cs2 = file_store.FileStore(fname)
        cs2.load()
        docs = []
        for doc in cs2.find({'theta': 30, 'phi': 140,
                             'isoval': 100, 'color': 'RTData_1'}):
            docs.append(doc.data)

        # compare the two
        l2error = ch.compare_l2(imageslice, docs[0])
        ncc = ch.compare_ncc(imageslice, docs[0])
        success = (l2error < 1.0) and (ncc > 0.99)

        if not success:
            print "\n l2-error = ", l2error, " ; ncc = ", ncc, "\n"

        self.assertTrue(success)
        pv.Disconnect()  # using a dedicated server state for each test

if __name__ == '__main__':
    unittest.main()
