importScripts('ffmpeg.js');

var now = Date.now;
var startTime;

function print(text) {
  postMessage({
    'type' : 'stdout',
    'data' : text
  });
  console.log('ffmpeg print: ' + text);
}

function onReturn(data) {
  postMessage({
    'type' : 'done',
    'data' : data,
    'time' : now() - startTime
  });
}

onmessage = function(event) {

  var message = event.data;

  if (message.type === "command") {

    var Module = {
      print: print,
      printErr: print,
      'return': onReturn,
      files: message.files || [],
      arguments: message.arguments || [],
      TOTAL_MEMORY: 268435456
      // Can play around with this option - must be a power of 2
      // TOTAL_MEMORY: 268435456
    };

    postMessage({
      'type' : 'start',
      'data' : Module.arguments.join(" ")
    });

    postMessage({
      'type' : 'stdout',
      'data' : 'Received command: ' +
                Module.arguments.join(" ") +
                ((Module.TOTAL_MEMORY) ? ".  Processing with " + Module.TOTAL_MEMORY + " bits." : "")
    });

    startTime = now();
    ffmpeg_run(Module);
  }
};

postMessage({
  'type' : 'ready'
});
