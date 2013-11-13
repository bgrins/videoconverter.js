importScripts('../ffmpeg_build/ffmpeg.js');

self.addEventListener('message', function(e) {
  var results = run("big_buck_bunny.webm", e.data,
    ["-i", "big_buck_bunny.webm", "-vf", "showinfo,scale=w=-1:h=-1", "-strict", "experimental", "-v", "verbose", "output.gif"]);

  postMessage({
    'type' : 'done'
  });
});

function run(filename, data, args) {
    var mod = {
      print: function (text) {
        postMessage({
          'type' : 'stdout',
          'data' : text
        });
      },
      printErr: function (text) {
        postMessage({
          'type' : 'stdout',
          'data' : text
        });
      },
      'arguments': args,
      'fileData': data,
      'fileName': filename
    };

    var result = ffmpeg_run(mod);
    var buffers = [];
    if (result && result.object && result.object.contents) {
      for (var i in result.object.contents) {
        if (result.object.contents.hasOwnProperty(i)) {
          buffers.push(new Uint8Array(result.object.contents[i].contents).buffer);
        }
      }
    }
    return buffers;
}