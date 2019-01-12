"""
Example script that shows one object from a cinema spec B/C store.
"""

import cinema_python.database.file_store as file_store
import cinema_python.images.querymaker_specb as qmsb
import cinema_python.images.compositor as compositor
import cinema_python.images.lookup_tables as luts
import PIL.Image
import sys

# todo:
# fail gracefully, or use specA translator, if not a specB/C store
# use pv_explorer to make something predictable


def show_something(fname):
    cs = file_store.FileStore(fname)
    cs.load()

    defobject = cs.get_parameter('vis')['default']
    print "LOOKING AT OBJECT ", defobject

    # now find parameters that are needed for this object and choose values
    indep, field, dep = cs.parameters_for_object(defobject)

    # the object itself
    request = {}
    request['vis'] = set([defobject])

    # independent parameters (time, camera etc)
    for x in indep:
        defval = cs.get_parameter(x)['default']
        try:
            request[x] = set([defval])
        except TypeError:
            # happens with pose's which are unhashable lists
            request[x] = defval

    # dependent parameters, filter settings etc
    for x in dep:
        defval = cs.get_parameter(x)['default']
        request[x] = set([defval])

    # an array to color by
    defval = cs.get_parameter(field)['default']
    request[field] = set([defval])

    print "DEFAULT SETTINGS ARE: "
    print "{"
    for k in list(request):
        print " '{}': {},".format(k, request[k])
    print "}"

    # now hand that over to the maker so it can make up a series
    # of queries that return the required rasters that go into
    # the object we've selected
    qm = qmsb.QueryMaker_SpecB()
    qm.setStore(cs)
    res = qm.translateQuery(request)

    print "RASTERS ARE KEPT IN", res

    # now setup the deferred renderer
    compo = compositor.Compositor_SpecB()
    compo.enableLighting(True)
    compo.set_background_color([0, 0, 0])

    # make up a color transfer function
    cmaps = []
    luts.add_rainbow(cmaps)
    lut = cmaps[0]
    glut = luts.LookupTable()
    glut.name = "Rainbow"
    glut.colorSpace = "RGB"
    glut.ingest(lut['RGBPoints'])
    lstruct = {
        defobject: {'colorLut': glut, 'geometryColor': [255, 255, 255]}}
    compo.setColorDefinitions(lstruct)
    print "COLOR TRANSFER FUNCTION TO USE", lstruct

    # ask it to render
    image = compo.render(res)

    # now we should have an RGB image in numpy, show it on screen
    im = PIL.Image.fromarray(image)
    im.show()
    print "If you see an empty window, most likely default values are off"

if len(sys.argv) != 2:
    print "Usage: python demo.py /path/to/info.json"
    exit(0)

show_something(sys.argv[1])
