
test ("Ping", function() {
  ok(true, "Pong");
});

asyncTest("Basic worker is initialized", basicWorkerTest("../demo/worker.js"));
asyncTest("All codecs worker is initialized", basicWorkerTest("../demo/worker-asm.js"));

function basicWorkerTest(src) {
  return function( assert ) {
    expect( 3 );
    initWorker({
      src: "../demo/worker-asm.js",
      ready: function(worker) {
        ok (true, "Worker is ready");
        worker.postMessage({
          type: "command",
          arguments: ["-help"]
        });
      },
      start: function(e) {
        ok(true, "Start has been called");
      },
      done: function(e) {
        ok(true, "Done has been called");
        QUnit.start();
      }
    });
  }
}

function initWorker(opts) {
  var src = opts.src;
  var worker = new Worker(src);
  var onready = opts.ready || function() {};
  var onstart = opts.start || function() {};
  var ondone = opts.done || function() {};
  var onstdout = opts.onstdout || function() {};
  worker.onmessage = function (event) {
    var message = event.data;
    if (message.type == "ready") {
      onready(worker, message);
    } else if (message.type == "stdout") {
      onstdout(worker, message);
    } else if (message.type == "start") {
      onstart(worker, message);
    } else if (message.type == "done") {
      ondone(worker, message);
    }
  };
}
