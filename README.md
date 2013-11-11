
#videoconverter.js

This is a project that converts the popular [FFmpeg]() library into JavaScript, using [Emscripten](https://github.com/kripken/emscripten).

## Usage instructions

These instructions are still See the `test/` directory for a basic usage example.  Call `ffmpeg_run` with an `Module` object as seen [here](test/index.html).  **Note**: this should be done in a worker normally to prevent browser hangs.

## External Library Support

This isn't yet compiled with any other static library support (like zlib, x264, libvpx, etc.  It should be possible to do though.

## Instructions to build yourself

First, make sure you have Emscripten set up:

    git clone git@github.com:kripken/emscripten.git

Depending on your system may need to also get the SDK to make sure Emscripten will work.  The have [documentation on their site](https://github.com/kripken/emscripten/wiki/Tutorial) about getting this to work.  Here are the basic configuration options we ended up using.  Most of the configuration options are actually disabling features that either don't make sense, or are there to save space.  The only other flag that I needed right off the bat was --enable-pic.


    git clone git@github.com:bgrins/videoconverter.js.git