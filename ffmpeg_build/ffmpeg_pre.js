
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
