find_package(PythonInterp REQUIRED)

add_test(NAME testBasic COMMAND ${PYTHON_EXECUTABLE} ${CinemaPython_SOURCE_DIR}/tests/test.py)
set_tests_properties(testBasic PROPERTIES WORKING_DIRECTORY ${CinemaPython_SOURCE_DIR}/tests/)
set_tests_properties(testBasic PROPERTIES ENVIRONMENT "PYTHONPATH=${CinemaPython_SOURCE_DIR}:$ENV{PYTHONPATH}")

add_test(NAME testVTK COMMAND ${PYTHON_EXECUTABLE} ${CinemaPython_SOURCE_DIR}/tests/test_vtk.py)
set_tests_properties(testVTK PROPERTIES WORKING_DIRECTORY ${CinemaPython_SOURCE_DIR}/tests/)
set_tests_properties(testVTK PROPERTIES ENVIRONMENT "PYTHONPATH=${CinemaPython_SOURCE_DIR}:$ENV{PYTHONPATH}")

add_test(NAME testPV COMMAND ${PYTHON_EXECUTABLE} ${CinemaPython_SOURCE_DIR}/tests/test_paraview.py)
set_tests_properties(testPV PROPERTIES WORKING_DIRECTORY ${CinemaPython_SOURCE_DIR}/tests/)
set_tests_properties(testPV PROPERTIES ENVIRONMENT "PYTHONPATH=${CinemaPython_SOURCE_DIR}:$ENV{PYTHONPATH}")
