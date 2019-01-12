.. CinemaPython documentation master file, created by
   sphinx-quickstart on Mon Jun  1 16:54:14 2015.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.
   * Manually updated on April 21, 2016.

User documentation for Cinema's reference Python Library
========================================================

The cinema_python package provides a set of APIs that provide a standard way to write, read, search and process Cinema databases of various specs and formats.

Requirements
------------

* Python 2.x
* numpy
* cmake (required only to run unit tests)
* scipy (required only to run unit tests)
* PIL (default resource for exporting/importing compressed rasters)
* VTK (alternative to PIL for exporting/importing compressed rasters)
* OpenEXR (optional, for exporting/importing compressed rasters)
* ParaView (optional, to generate stores from ParaView/Catalyst)

Basic Usage
-----------

If you have dependencies setup, you can use this example to test your setup by generating a small Cinema database.::

    $ python examples/simple_vtk.py

This should generate a number of png files and one json files that looks
similar to this::

    {
      "metadata": null,
      "name_pattern": "{phi}\/{theta}\/{contour}.png",
      "arguments": {
        "contour": {
          "label": "contour",
          "type": "range",
          "values": [
            160,
            200
          ],
          "default": 160
        },
        "phi": {
          "label": "phi",
          "type": "range",
          "values": [
            0,
            40,
            80,
            120,
            160
          ],
          "default": 0
        },
        "theta": {
          "label": "theta",
          "type": "range",
          "values": [
            -180,
            -140,
            -100,
            -60,
            -20,
            20,
            60,
            100,
            140,
            180
          ],
          "default": -180
        }
      },
      "associations": null
    }

For details about the format of Cinema databases, see the `Cinema specs repo <https://gitlab.kitware.com/cinema/specs>`_. You can view the generated database using the `Python Qt viewer <https://gitlab.kitware.com/cinema/qt-viewer>`_ or the
`basic Web viewer <https://gitlab.kitware.com/cinema/basic-web-viewer>`_.

Besides generating data, you can also use the Cinema library to search and analyze the collected data, as well as view the contents graphically. The examples directory :ref:`examples` has the getting started scripts for all three purposes.


API Documentation
------------------

.. toctree::
   :maxdepth: 4

   cinema_python
   database
   images
   adaptors
   examples


Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

