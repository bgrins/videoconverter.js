
echo "Beginning Build:"

cd ffmpeg

emconfigure ./configure --cc="emcc" --disable-ffplay --disable-ffprobe --disable-ffserver --disable-asm --enable-pic --enable-static --disable-shared --disable-doc --disable-devices --disable-pthreads --disable-w32threads --disable-network --enable-small --disable-hwaccels --disable-parsers --disable-bsfs --disable-debug --disable-altivec --disable-amd3dnow --disable-amd3dnowext --disable-mmx --disable-mmxext --disable-sse --disable-sse2 --disable-sse3 --disable-ssse3 --disable-sse4 --disable-sse42 --disable-avx --disable-fma4 --disable-armv5te --disable-armv6 --disable-armv6t2 --disable-vfp --disable-neon --disable-vis --disable-inline-asm --disable-yasm
make clean;
emmake make;
emcc -s VERBOSE=1 -s ALLOW_MEMORY_GROWTH=1 -v libavutil/*.o libavcodec/*.o libavformat/*.o libavdevice/*.o libswresample/*.o libavfilter/*.o libswscale/*.o *.o -o ../ffmpeg.js --pre-js ../ffmpeg_pre.js --post-js ../ffmpeg_post.js

cd ../

echo "Finished Build"