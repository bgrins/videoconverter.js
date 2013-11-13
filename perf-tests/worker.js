importScripts('localhost:8000/ffmpeg.js');

self.addEventListener('message', function(e) {
  run("big_buck_bunny.webm", e.data, "vf showinfo,scale=w=-1:h=-1 -strict experimental -v verbose output.gif");
});

function run(filename, data, args) {  
    var mod = {
      print: function (text) {
        self.postMessage(text);
      },
      printErr: function (text) {
        self.postMessage(text);
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