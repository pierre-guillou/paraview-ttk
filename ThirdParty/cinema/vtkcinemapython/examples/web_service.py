"""
Example script showing how to provide a database viewing service over the web.
Besides cinema, this depends on bottle.py (see bottlepy.org)
To try it:
python web_service.py -fn /location/of/an/info.json
browser to localhost:8080/cinemaviewer
"""

from bottle import response, route, run
import cinema_python.database.file_store as file_store
import cinema_python.images.compositor as compositor
import cinema_python.images.lookup_tables as luts
import cinema_python.images.querymaker_specb as qmsb
import json
import numpy
import PIL
import StringIO
import sys

fname = None
for i in range(0, len(sys.argv)-1):
    if sys.argv[i] == "-fn":
        fname = sys.argv[i+1]
if fname is None:
    print ("Usage:", sys.argv[0], "-fn cinemastore/info.json")
    sys.exit(0)

cs = file_store.FileStore(fname)
cs.load()


@route('/speclevel')
def speclevel():
    """
    Entry point for web page to see what type of store we serve.
    """
    if cs.get_version_major() == 1:
        return "C"
    return "A"


@route('/cameramodel')
def cameramodel():
    """
    Entry point to ask what the camera model is, and thus what
    interactors to make. todo: we are not yet doing anything with this
    in the example below.
    """
    return cs.get_camera_model()


@route('/parameters')
def parameters():
    """
    Entry point to get the list of parameters are present in the store.
    """
    return cs.parameter_list


def __index_query_to_values(query):
    """
    Translate indexes in query to values.
    """
    # print ("QSTR", query)
    q = json.loads(query)
    transq = {}
    for k, v in q.iteritems():
        p = cs.get_parameter(k)['values']
        transq[k] = p[v[0]]
    # print ("TRANSQ ", transq)
    return transq


@route('/get/<query>')
def get(query):
    """
    Entry point for page to request a specific result from the store.
    query comes in a a set of key:index pairs and this returns images.
    todo: cinema can store more than just images, for example depth
    and value rasters and potentially other things, but this function
    blindly returns pngs. That's fine for specA, but if we make the
    client smart enough to do its own compositing, we'll need to fix
    that for example.
    """
    transq = __index_query_to_values(query)

    # get the result out of the database
    # todo: fail gracefully on bad query where raster is None
    document = cs.get(transq)
    raster = document.data

    # convert the result into a string of byte to return to browser
    imageslice = numpy.flipud(raster)
    pimg = PIL.Image.fromarray(imageslice)
    output = StringIO.StringIO()

    # encode as PNG in memory
    format = 'PNG'
    pimg.save(output, format)
    contents = output.getvalue()
    # let client know that we're returning an image
    response.set_header('Content-type', 'image/png')
    return contents


def __index_query_to_values_C(query):
    """
    Differs from __index_query_to_values_A in that we expect to to
    to pass in multiple items for some keys, not just one.
    """
    # print ("QSTR", query)
    q = json.loads(query)
    transq = {}
    for k, v in q.iteritems():
        p = cs.get_parameter(k)['values']
        if k == 'pose':
            transq[k] = p[v[0]]
        else:
            vals = []
            for r in range(0, len(v)):
                vals.append(p[v[r]])
            transq[k] = vals
    # print ("TRANSQ ", transq)
    return transq


@route('/getcomposite/<query>')
def getcomposite(query):
    """
    Entry point for rendered results from a specB/C store.
    """
    transq = __index_query_to_values_C(query)

    # turn the set of choices into a stack or raster producing queries
    qm = qmsb.QueryMaker_SpecB()
    qm.setStore(cs)
    res = qm.translateQuery(transq)

    # setup the deferred renderer to put the rasters together into a final
    # image
    compo = compositor.Compositor_SpecB()
    compo.enableLighting(True)
    compo.set_background_color([0, 0, 0])

    # make up a color transfer function
    # todo: let user choose which one
    # todo: let user make new ones
    cmaps = []
    luts.add_rainbow(cmaps)
    lut = cmaps[0]
    glut = luts.LookupTable()
    glut.name = "Rainbow"
    glut.colorSpace = "RGB"
    glut.ingest(lut['RGBPoints'])
    lstruct = {}
    # more correct to look for the one param with annotation 'layer' but
    # whatever we'll just get 'vis' since paraview names it that.
    objects = cs.get_parameter('vis')['values']
    for v in objects:
        # assign color choices to each potential object in the scene
        lstruct[v] = {'colorLut': glut, 'geometryColor': [255, 255, 255]}
    compo.setColorDefinitions(lstruct)

    # tell the compositor to make the final image
    pimg = compo.render(res)

    # translate the numpy result into a png to send to browser
    im = PIL.Image.fromarray(pimg)
    output = StringIO.StringIO()
    format = 'PNG'
    im.save(output, format)

    # return it
    contents = output.getvalue()
    response.set_header('Content-type', 'image/png')
    return contents


@route('/cinemaviewer')
def cinemaviewer():
    """
    Main entry point that end users care about. Point a browser at this and
    it will create a page that lets you see and interact with a cinema store.
    """

    # todo: the html and javascript and eventually css should be in
    # in their own files. But for now, just encoding the page as text.
    return ("""
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Sample of Web delivery out of cinema_python.</title>
  <link rel="stylesheet"
        href="//code.jquery.com/ui/1.12.1/themes/base/jquery-ui.css">
  <script src="https://code.jquery.com/jquery-1.12.4.js"></script>
  <script src="https://code.jquery.com/ui/1.12.1/jquery-ui.js"></script>
  <script>
  // what we learn about the store
  var level = "A";
  var model = "static";
  var parameters;

  // holds the query that we make.
  var query = {};

  onload = function () {
    // when page loads, first figure out what the store has
    // spec type, so we can make appropriate requests
    $.get("http://localhost:8080/speclevel",
          function(data) {
            level = data;
          });

    // camera model so we can set up interactors
    // todo: this is unused so far
    $.get("http://localhost:8080/cameramodel",
          function(data) {
            model = data;
          });

    // this of parameters
    $.getJSON("http://localhost:8080/parameters", {format:"json"}).done(
      function(data) {
        //we now know what the parameters are, make GUI and run
        parameters = data;
        update_UI();
      }
    );
  };

  update_UI = function() {
    // make up GUI corresponding to parameters int the store
    // at the end (and whenever a widget changes, we call render to show
    // the result.

    var gui = document.getElementById('gui');
    for (var key in parameters) {
      var br = document.createElement("br");
      gui.appendChild(br);

      query[key] = [0]; // todo: use default instead of 0
      if (('role' in parameters[key]) &&
          (parameters[key]['role'] == 'field')) {
         // color components need special treatment
         var label = document.createTextNode(key);
         gui.appendChild(label);

         br = document.createElement("br");
         gui.appendChild(br);

         var numitems = parameters[key]['values'].length;
         var types = parameters[key]['types']
         var first = -1;
         for (var idx = 0; idx < numitems; idx++) {
           if (types[idx] != "depth" && types[idx] != "luminance") {
             // for color components, have to skip over depth and luminance
             // which user can't see directly

             if (first == -1)
               {
                 first = idx;
               }
             var btn = document.createElement('button');
             btn.innerHTML = parameters[key]['values'][idx];
             btn.id = key+"_"+idx;
             gui.appendChild(btn);
             $(btn).click( function () {
               var key = this.id.split("_")[0];
               var idx = parseInt(this.id.split("_")[1]);
               query[key] = [idx];
               render();
             });
           }

           //todo: add a colorlut chooser
         }
         query[key] = [first];
         continue;
      }

      var innerDiv = document.createElement('div');
      // label
      var label = document.createTextNode(key);
      gui.appendChild(label);

      var innerDiv = document.createElement('div');
      innerDiv.id = key;
      gui.appendChild(innerDiv);
      var numitems = parameters[key]['values'].length;

      if (parameters[key]['type'] == 'option') {
        // combobox
        for (var idx = 0; idx < numitems; idx++) {
          var btn = document.createElement('button');
          btn.innerHTML = parameters[key]['values'][idx];
          btn.id = key+"_"+idx;
          gui.appendChild(btn);
          // todo: make button look down when on
          $(btn).click( function () {
            var key = this.id.split("_")[0];
            var idx = parseInt(this.id.split("_")[1]);
            var location = query[key].indexOf(idx);
            if (location == -1) {
               query[key].push(idx);
            } else {
               query[key].splice(location, 1);
            }
            render();
          });
        }
      } else {
        // slider
        // todo: display chosen value as text
        var nextSlider = $(innerDiv).slider({
          max : numitems-1,
          change: function(event, ui) {
            var key = $(ui.handle).parent().attr('id');
            var idx = ui.value;
            query[key] = [idx];
            // call render whenever released to update view
            render();
          }
        });

        // play button
        var btn = document.createElement('button');
        btn.innerHTML = "play";
        btn.id = "play_"+key;
        gui.appendChild(btn)
        $(btn).click( function () {
          var key = this.id.split("_")[1];
          var numitems = parameters[key]['values'].length;
          var now = query[key][0];
          if (now == numitems-1) {
            now = -1;
          }
          if (now < numitems-1) {
            now = now + 1;
            query[key] = [now];
            render();
            //call self to continue
            //todo: update slider appropriately
            //todo: rather than fixed timeout of 130msec, call(_this)
            //as soon as and only when render() refreshes image
            if (now < numitems-1) {
              var fn = arguments.callee;
              var _this = this;
              setTimeout(function(){fn.call(_this);}, 100);
            }
          }
        });
      }

    }
    // call render on first draw
    render();
  };

  render = function() {
    // ask the server (view the get or getcomposite entry point) for the
    // data corresponding to our query, and show it in the view_window image
    //
    // todo: cache results and reuse rather than hit server every time.
    var q_str1 = "http://localhost:8080/get/";
    if (level == "C") {
      q_str1 = "http://localhost:8080/getcomposite/";
    }
    var q_str2 = JSON.stringify(query);
    var elem = document.getElementById('view_window');
    elem.src = q_str1+q_str2;
  };
  </script>
</head>
<body>

<div id="gui"></div>
<br>
<img src="" id="view_window" >
</div>

</body>
</html>
""")


run(host='localhost', port=8080, debug=False)
