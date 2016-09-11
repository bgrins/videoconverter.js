importScripts('../build/ffmpeg.js');
importScripts('./lodash.js');
importScripts('./benchmark.js');

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

    var time = now();

    Module['returnCallback'] = function(result) {
      var totalTime = now() - time;
      
      postMessage({
        'type' : 'stdout',
        'data' : 'Finished processing (took ' + totalTime + 'ms)'
      });
      
      postMessage({
        'type' : 'done',
        'data' : result,
        'time' : totalTime
      });
    }

    var result = ffmpeg_run(Module);

  }

  if (message.type === "benchmark") {

    var Module = {};

    postMessage({
      'type' : 'start',
      'data' : message.arguments.join(" ")
    });
    print("Starting benchmark")

    var suite = new Benchmark.Suite
    suite.add('ffmpeg', {
      'defer': true,
      'setup': function() {
        Module = {
          print: function(data){},
          printErr: function(data){},
          files: message.files || [],
          arguments: message.arguments.slice() || [],
          TOTAL_MEMORY: 268435456
          // Can play around with this option - must be a power of 2
          // TOTAL_MEMORY: 268435456
        };
        print(".");
      },
      'fn': function(deferred) {
        Module['returnCallback'] = function(result) {
          deferred.resolve();
        }
        ffmpeg_run(Module);
      }
    }).on('complete', function() {
      var results = this.filter('fastest')[0];
      print("Samples:" + results.stats.sample.length)
      print("Mean:" + results.stats.mean)
      print("Variance:" + results.stats.variance)
      print("Stddev:" + Math.sqrt(results.stats.variance))
      postMessage({
        'type' : 'done',
        'data' : [],
        'time' : 0
      });
    }).run({'async': true});
  }
};

postMessage({
  'type' : 'ready'
});
