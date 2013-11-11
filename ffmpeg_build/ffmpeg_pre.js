/*
Most files in FFmpeg are under the GNU Lesser General Public License version 2.1
or later (LGPL v2.1+). Read the file COPYING.LGPLv2.1 for details. Some other
files have MIT/X11/BSD-style licenses. In combination the LGPL v2.1+ applies to
FFmpeg.

The source code used to build this file can be obtained at https://github.com/bgrins/videoconverter.js,
and in zip form at https://github.com/bgrins/videoconverter.js/archive/master.zip
*/

function ffmpeg_run(opts) {
  var Module = {
    'outputDirectory': 'output'
  };

  for (var i in opts) {
    Module[i] = opts[i];
  }

  Module['preRun'] = function() {
    FS.createFolder('/', Module['outputDirectory'], true, true);
    if (Module['fileData']) {
      FS.createDataFile('/', Module['fileName'], Module['fileData'], true, true);
    }
  };

  Module['postRun'] = function() {
    var handle = FS.analyzePath(Module['outputDirectory']);
    Module['return'] = handle;
  };
