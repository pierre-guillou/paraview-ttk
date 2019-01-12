""" utility to convert a spec A,B,or C metadata file to D """

from __future__ import print_function
import sys
import cinema_python.database.file_store as file_store
import cinema_python.adaptors.explorers as explorers


def to_D(fname, save=False):
    """
    convert an A, B, or C store meta directory into a D format one
    """
    cs = file_store.FileStore(fname)
    cs.load()

    results = []

    class printDescriptor(explorers.Explorer):
        def __init__(self, *args):
            super(printDescriptor, self).__init__(*args)

        def execute(self, desc):
            # a line for the identity and filename of everything in the store
            line = []
            for p in sorted(self.parameters):
                if p in desc:
                    line.append(str(desc[p]))
                else:
                    # use Not Applicable for don't cares i.e. dependencies
                    line.append("NA")
                line.append(",")
            line.append(cs._get_filename(desc))
            results.append(line)

    # make the header line
    hline = []
    for k in sorted(cs.parameter_list):
        hline.append("{0},".format(k))
    hline.append("FILE")
    results.append(hline)

    # iterate
    e = printDescriptor(cs, cs.parameter_list, [])
    e.explore()

    return results


def to_string(results):
    """
    Utility to convert from list of lists to a string.
    """
    res = ''
    for line in results:
        sline = ''.join(line)
        res += sline + "\n"
    return res


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python toCinemaD.py ABorC.json")
        quit()

    print(to_string(to_D(sys.argv[1])))
