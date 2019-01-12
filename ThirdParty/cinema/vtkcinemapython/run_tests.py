import glob
import subprocess

tests = glob.glob("./tests/*.py")
for test in tests:
    print test
    print subprocess.check_output(["python", test])
