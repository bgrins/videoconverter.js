importScripts('../build/ffmpeg_asm.js');

var now = Date.now;

function print(text) {
  postMessage({
    'type' : 'stdout',
    'data' : text
  });
}

onmessage = function(event) {

  var message = event.data;

  if (message.type === "command") {

    postMessage({
      'type' : 'start',
    });

    var Module = {
      print: print,
      printErr: print,
      files: message.files || [],
      arguments: message.arguments || [],
      TOTAL_MEMORY: 67108864
    };

    postMessage({
      'type' : 'stdout',
      'data' : 'Received command: ' + Module.arguments.join(" ")
    });

    var time = now();
    var result = ffmpeg_run(Module);

    var totalTime = now() - time;
    postMessage({
      'type' : 'stdout',
      'data' : 'Finished processing (took ' + totalTime + 'ms)'
    });

    postMessage({
      'type' : 'done',
      'data' : result
    });
  }
};

postMessage({
  'type' : 'ready'
});
