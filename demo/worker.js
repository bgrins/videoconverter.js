importScripts('../build/ffmpeg.js');

var now = Date.now;

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

onmessage = function(event) {

  var message = event.data;
  var args = message.arguments || [];

  if (message.type === "command") {

    postMessage({
      'type' : 'start',
    });

    postMessage({
      'type' : 'stdout',
      'data' : 'Received command: ' + args.join(" ")
    });

    var outputFilePath = args[args.length - 1];
    if (args.length > 2 && outputFilePath && outputFilePath.indexOf(".") > -1) {
      args[args.length - 1] = "output/" + outputFilePath;
    }

    var Module = {
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
      }
    };
    Module.files = message.files;
    Module['arguments'] = args;

    var time = now();
    var result = ffmpeg_run(Module);

    var totalTime = now() - time;
    postMessage({
      'type' : 'stdout',
      'data' : 'Finished processing (took ' + totalTime + 'ms)'
    });

    postMessage({
      'type' : 'done',
      'data' : getAllBuffers(result)
    });
  }
};

postMessage({
  'type' : 'ready'
});
