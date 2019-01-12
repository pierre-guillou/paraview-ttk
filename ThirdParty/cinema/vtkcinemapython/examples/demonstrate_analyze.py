"""
Example script that analyzes a store. First it loads it, then asks what the
variable parameters are, picks one of them, uses that to search in
the database while doing some simple processing on the matching documents.
"""

import cinema_python.database.file_store as file_store
import PIL.Image
import sys


def demonstrate_analyze(fname):
    cs = file_store.FileStore(fname)
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
    for doc in cs.find({param: val}):
        print doc.descriptor
        image = PIL.Image.fromarray(doc.data)
        print image.histogram()

if len(sys.argv) != 2:
    print "Usage: python demo.py /path/to/info.json"
    exit(0)

demonstrate_analyze(sys.argv[1])
