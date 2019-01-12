""" unit level tests of cinema base functionality """

import unittest
import cinema_python.database.store as store
import cinema_python.database.file_store as file_store
import cinema_python.adaptors.explorers as explorers


class TestBasicFileStore(unittest.TestCase):
    def clean_up(self, cs, fname):
        import os
        for doc in cs.find():
            os.remove(cs._get_filename(doc.descriptor))
        os.remove(fname+"/info.json")
        os.removedirs(fname)

    def test_basic(self):
        """ test verifies that we can create, save, and load a store
        and search and exercise it getting consistent results"""

        print "*"*80
        print "testing filestore persistance and I/O functioning"

        thetas = [0, 10, 20, 30, 40]
        phis = [0, 10, 20]

        fname = "./test.json"
        cs = file_store.FileStore(fname)
        cs.filename_pattern = "data_{theta}_{phi}.txt"
        cs.add_parameter("theta", store.make_parameter('theta', thetas))
        cs.add_parameter("phi", store.make_parameter('phi', phis))

        s = set()

        for t in thetas:
            for p in phis:
                doc = store.Document({'theta': t, 'phi': p})
                doc.data = str(doc.descriptor)
                s.add((t, p))
                cs.insert(doc)

        try:
            cs.save()

            s2 = set()

            cs2 = file_store.FileStore(fname)
            # Test load
            cs2.load()
            for doc in cs2.find():
                s2.add(tuple(doc.descriptor.values()))

            self.assertEqual(s, s2)

            # Test search
            docs = cs2.find({'theta': 0})
            import ast
            for doc in docs:
                vals1 = [int(x) for x in doc.descriptor.values()]
                vals2 = ast.literal_eval(doc.data).values()
                self.assertEqual(vals1, vals2)
        except:
            self.clean_up(cs, fname)
            raise
        else:
            self.clean_up(cs, fname)

    def test_explorer(self):

        """
        verifies that we cover exactly the set of settings that we expect
        from within the parameter space.
        """
        print "*"*80
        print "testing paramter/value space"

        params = ["time", "layer", "slice_field", "back_color"]

        cs = store.Store()
        cs.add_parameter("time",
                         store.make_parameter("time", [0, 1, 2]))
        cs.add_parameter("layer",
                         store.make_parameter
                         ("layer", ['outline', 'slice', 'background']))
        cs.add_parameter("slice_field", store.make_parameter
                         ("slice_field",
                          ['solid_red', 'temperature', 'pressure']))
        cs.add_parameter("back_color", store.make_parameter
                         ("back_color", ['grey0', 'grey49']))

        class printDescriptor(explorers.Explorer):
            def __init__(self, *args):
                super(printDescriptor, self).__init__(*args)
                self.Descriptions = []

            def execute(self, desc):
                print desc
                self.Descriptions.append(desc)

        print "NO DEPENDENCIES"
        # we should hit all combinations of the values for
        # time, layer, slice_field and back_color
        e = printDescriptor(cs, params, [])
        e.explore()
        self.assertEqual(len(e.Descriptions), 3*3*3*2)

        for desc in e.Descriptions:
            self.assertTrue('slice_field' in desc)
            self.assertTrue('back_color' in desc)

        print "NO DEPENDENCIES AND FIXED TIME"
        # similar to above, except that the time parameter is fixed to one
        # particular setting
        e = printDescriptor(cs, params, [])
        e.explore({'time': 3})

        self.assertEqual(len(e.Descriptions), 3*3*2)

        for desc in e.Descriptions:
            self.assertTrue('slice_field' in desc)
            self.assertTrue('back_color' in desc)

        print "WITH DEPENDENCIES"
        # Now we should hit only the subset constrained by the dependency
        # relationships. For example, if layer is not equal to slice we don't
        # vary over slice_field.
        cs.assign_parameter_dependence('slice_field', 'layer', ['slice'])
        cs.assign_parameter_dependence('back_color', 'layer', ['background'])

        ground_truth = {'outline': (False, False),
                        'slice': (True, False),
                        'background': (False, True)}

        e = printDescriptor(cs, params, [])
        e.explore()

        self.assertEqual(len(e.Descriptions), 6*3)

        # Check dependencies
        for desc in e.Descriptions:
            layer = desc['layer']
            match = ('slice_field' in desc, 'back_color' in desc)
            # print layer
            self.assertEqual(match, ground_truth[layer])

        print "WITH DEPENDENCIES AND FIXED TIME"
        # similar to above, except with a fixed value for time
        e = printDescriptor(cs, params, [])
        e.explore({'time': 3})

        self.assertEqual(len(e.Descriptions), 6)

        # Check dependencies
        for desc in e.Descriptions:
            layer = desc['layer']
            match = ('slice_field' in desc, 'back_color' in desc)
            # print layer
            self.assertEqual(match, ground_truth[layer])

    def test_layers_fields(self):
        """
        verifies that layer controls work as expected. In otherwords to layers
        enforce the presentation of just one item at a time from within the
        set of values.
        """

        print "*"*80
        print "testing layers and fields"
        settings = []

        params = ["time", "layer", "component"]

        cs = store.Store()
        cs.add_parameter("time", store.make_parameter("time", ["0"]))
        cs.add_layer("layer", store.make_parameter
                     ("layer", ['outline', 'slice', 'background']))
        cs.add_field("component", store.make_parameter
                     ("component", ['z', 'RGB']), "layer", 'slice')

        def showme(self):
            settings[-1].append([self.name, True])

        def hideme(self):
            settings[-1].append([self.name, False])

        outline_control = explorers.LayerControl("outline", showme, hideme)
        slice_control = explorers.LayerControl("slice", showme, hideme)
        background_control = explorers.LayerControl("background",
                                                    showme, hideme)

        field_control1 = explorers.LayerControl("z", showme, hideme)
        field_control2 = explorers.LayerControl("RGB", showme, hideme)

        layertrack = explorers.Layer(
            "layer", [outline_control, slice_control, background_control])
        fieldtrack = explorers.Layer(
            "component", [field_control1, field_control2])

        class printDescriptor(explorers.Explorer):
            def __init__(self, *args):
                super(printDescriptor, self).__init__(*args)

            def execute(self, desc):
                print desc
                settings.append([desc])
                super(printDescriptor, self).execute(desc)

        e = printDescriptor(cs, params, [layertrack, fieldtrack])
        e.explore()

        ground_truth = {
            'outline': {'outline': True, 'slice': False, 'background': False},
            'slice': {'outline': False, 'slice': True, 'background': False},
            'background':
            {'outline': False, 'slice': False, 'background': True}}

        slice_gt = [{'z': True, 'RGB': False}, {'z': False, 'RGB': True}]
        # look at what the explorer hit and make sure we did not hit spaces
        # outside of ground truth
        for setting in settings:
            layer = setting[0]['layer']
            gt = ground_truth[layer]
            slice_settings = {}
            for s in setting[1:]:
                if s[0] not in ('z', 'RGB'):
                    self.assertEqual(s[1], gt[s[0]])
                else:
                    slice_settings[s[0]] = s[1]
            if layer == 'slice':
                self.assertTrue(slice_settings in slice_gt)

if __name__ == '__main__':
    unittest.main()
