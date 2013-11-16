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

    var Module = {
      print: print,
      printErr: print,
      files: message.files || [],
      arguments: message.arguments || [],
      noInitialRun: true
    };

    postMessage({
      'type' : 'start',
      'data': Module.arguments.join(" ")
    });

    postMessage({
      'type' : 'stdout',
      'data' : 'Received command: ' + Module.arguments.join(" ")
    });

    var time = now();
    ffmpeg_run(Module);
    var timeBeforeRun = now() - time;

    Module.callMain(Module['arguments']);

    var totalTime = now() - time - timeBeforeRun;
    postMessage({
      'type' : 'stdout',
      'data' : 'Finished processing (took ' + timeBeforeRun + ' / ' + totalTime + 'ms)'
    });

    postMessage({
      'type' : 'done',
      'data' : Module['return'],
      'time' : totalTime,
      'timeBeforeRun' : timeBeforeRun
    });

Module.HEAP8 = null;
Module.HEAP16 = null;
Module.HEAP32 = null;
Module.HEAPF32 = null;
Module.HEAPF64 = null;
Module.HEAPU8 = null;
Module.HEAPU16 = null;
Module.HEAPU32 = null;

    //Module['exit']();
    Module = undefined;
  }
};

postMessage({
  'type' : 'ready'
});
