importScripts('../build/ffmpeg.js');

self.addEventListener('message', function(e) {
  postMessage({
    'type' : 'start'
  });

  var results = run("big_buck_bunny.webm", e.data,
    ["-i", "big_buck_bunny.webm", "-vf", "showinfo,scale=w=-1:h=-1", "-strict", "experimental", "-v", "verbose", "output.mov"]);

  postMessage({
    'type' : 'end'
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
      'files': [{
        data: data,
        name: filename
      }]
    };

    var result = ffmpeg_run(mod);
    return result;
}