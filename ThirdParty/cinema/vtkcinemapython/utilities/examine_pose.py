import cinema_python.database.file_store as file_store
import sys


def pose_to_vtk(fname):
    """
    converts the view transformation matrices into a
    vtk polydata to visualize it
    """

    if fname[-5:] == ".json":
        cs = file_store.FileStore(fname)
        cs.load()
        poses = cs.get_parameter('pose')['values']
    else:
        poses = []
        f = open(fname, 'r')
        for line in f:
            asarray = eval(line)
            poses.append(asarray)

    coords = []
    coords.append([0, 0, 0])
    cells = []

    for pose_id in range(0, len(poses)):
        up = poses[pose_id][0]
        rt = [x*2 for x in poses[pose_id][1]]
        dn = [x*3 for x in poses[pose_id][2]]
        coords.append(up)
        coords.append(rt)
        coords.append(dn)
        cells.append([0, 1+(pose_id*3+0),  # line from origin to up and back
                      0, 1+(pose_id*3+1),   # origin to rt and back
                      0, 1+(pose_id*3+2)])  # origin to dn and back

    print "# vtk DataFile Version 2.0"
    print "Poses from ", fname
    print "ASCII"
    print "DATASET POLYDATA"
    print "POINTS", len(coords), "double"
    for c in coords:
        print c[0], c[1], c[2]
    print "LINES", len(cells), len(cells)*7
    for c in cells:
        print 6, c[0], c[1], c[2], c[3], c[4], c[5]
    print "CELL_DATA", len(cells)
    print "SCALARS poseid int 1"
    print "LOOKUP_TABLE default"
    for c in range(0, len(cells)):
        print c

if len(sys.argv) != 2:
    print "Usage: python examine_pose.py filename"
    exit(0)

pose_to_vtk(sys.argv[1])
