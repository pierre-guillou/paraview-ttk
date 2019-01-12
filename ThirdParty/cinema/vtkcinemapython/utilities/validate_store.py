#!/usr/bin/python
import sys
import json

import cinema_python.database.file_store as file_store
import cinema_python.database.vti_store as vti_store
from cinema_python.adaptors import explorers
from cinema_python.database import raster_wrangler

# parse arguments if any
argv = sys.argv
if len(argv) > 1:
    # try to open up a store
    with open(sys.argv[1], mode="rb") as file:
        try:
            info_json = json.load(file)
        except IOError as e:
            print e
            sys.exit(1)

    try:
        if info_json["metadata"]["store_type"] == "SFS":
            cs = vti_store.VTIFileStore(sys.argv[1])
        else:
            raise TypeError

    except(TypeError, KeyError):
        cs = file_store.FileStore(sys.argv[1])

    cs.load()

    class validateStore(explorers.Explorer):
        def __init__(self, *args):
            super(validateStore, self).__init__(*args)
            self.isValid = True
            self.raster_wrangler = raster_wrangler.RasterWrangler()

        def execute(self, desc):
            try:
                self.raster_wrangler.assertvalidimage(cs._get_filename(desc))
            except IOError as e:
                print e
                self.isValid = False
                pass

    e = validateStore(cs, [], [])
    e.explore()

    if not e.isValid:
        print "Store", sys.argv[1], "is invalid."
        sys.exit(1)

    print "Store", sys.argv[1], "is valid."

    sys.exit()

else:
    print "python valid_store_checker <path_to_json_descriptor>"
    sys.exit(1)
