#videoconverter.js

videoconverter.js is a library that allows you to convert and manipulate videos inside of your web browser.

This is acheived by converting the popular [FFmpeg](http://ffmpeg.org/) library into JavaScript, using [Emscripten](https://github.com/kripken/emscripten).

It was originally conceived for a project called [http://nodeknockout.com/teams/devcomo](Video Funhouse) in Node Knockout 2013.

## How big is the JavaScript file?

It is 24MB.  For the original demo, it was around 50MB.

## Can I Use It?

Sure, as long as you follow any relevant [FFmpeg license](http://www.ffmpeg.org/legal.html) terms.  You can also read a copy of the [LICENSE](LICENSE) file - the [ffmpeg.js](ffmpeg_build/ffmpeg.js) file is built under the LGPL terms described on the FFmpeg site above.

The usage instructions are still in development.  See the [test/](test/index.html) directory for a very basic usage example.  We are working on bringing a more robust sample app into the repository.

Call `ffmpeg_run` with an `Module` object as seen [here](test/index.html).  **Note**: this should be done in a worker normally to prevent browser hangs.

## Instructions to build yourself

Want to build the ffmpeg.js file for yourself?  First, make sure you have Emscripten set up:

    git clone git@github.com:kripken/emscripten.git

Depending on your system may need to also get the SDK to make sure Emscripten will work.  The have [documentation on their site](https://github.com/kripken/emscripten/wiki/Tutorial) about getting this to work.

Once this is all set up and `emcc` is on your path, you should be able to run:

    git clone git@github.com:bgrins/videoconverter.js.git
    cd videoconverter.js/ffmpeg_build
    ./build_lgpl.sh


## Potential Uses

### Video Editing / Conversion

This is what we are doing with http://devcomo.2013.nodeknockout.com/.  Obviously, this could be expanded and optimized.  Quite likely to bump up against performance bottlenecks - I [wrote about some of the issues we bumped into](http://www.briangrinstead.com/blog/video-funhouse) if you are interested in more information.

### Benchmarking

We are beginning to build a [benchmark](jsperf_test/) to compare different browser performances.  It would be interesting to compare performance versus native as well.

### External Library Support

This isn't yet compiled with any other static library support (like zlib, x264, libvpx, etc.  It should be possible to do though.
