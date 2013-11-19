/*
Most files in FFmpeg are under the GNU Lesser General Public License version 2.1
or later (LGPL v2.1+). Read the file COPYING.LGPLv2.1 for details. Some other
files have MIT/X11/BSD-style licenses. In combination the LGPL v2.1+ applies to
FFmpeg.

The source code used to build this file can be obtained at https://github.com/bgrins/videoconverter.js,
and in zip form at https://github.com/bgrins/videoconverter.js/archive/master.zip
*/

function ffmpeg_run(opts) {
  var isNode = typeof(exports) !== 'undefined';
  if (!isNode) {
    var Module = {
      'outputDirectory': 'output'
    };
    for (var i in opts) {
      Module[i] = opts[i];
    }
    var outputFilePath = Module['arguments'][Module['arguments'].length - 1];
    if (Module['arguments'].length > 2 && outputFilePath && outputFilePath.indexOf(".") > -1) {
      Module['arguments'][Module['arguments'].length - 1] = "output/" + outputFilePath;
    }
    Module['preRun'] = function() {
      FS.createFolder('/', Module['outputDirectory'], true, true);
      /* fileData / fileName is deprecated - please use file.name and file.data instead */
      if (Module['fileData']) {
        FS.createDataFile('/', Module['fileName'], Module['fileData'], true, true);
      }
      if (Module['files']) {
        Module['files'].forEach(function(file) {
          FS.createDataFile('/', file.name, file.data, true, true);
        });
      }
    };
    Module['postRun'] = function() {
      var handle = FS.analyzePath(Module['outputDirectory']);
      Module['return'] = getAllBuffers(handle);
    };
    function getAllBuffers(result) {
      var buffers = [];
      if (result && result.object && result.object.contents) {
        for (var i in result.object.contents) {
          if (result.object.contents.hasOwnProperty(i)) {
            buffers.push({
              name: i,
              data: new Uint8Array(result.object.contents[i].contents).buffer
            });
          }
        }
      }
      return buffers;
    }
  }