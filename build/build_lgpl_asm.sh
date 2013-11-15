echo "Beginning Build:"

cd ffmpeg

emconfigure ./configure --cc="emcc" --target-os=none --cpu=generic --arch=x86_64 --disable-ffplay --disable-ffprobe --disable-ffserver --disable-asm --enable-pic --disable-doc --disable-devices --disable-pthreads --disable-w32threads --disable-network --enable-small --disable-hwaccels --disable-parsers --disable-bsfs --disable-debug --disable-zlib
make clean;
emmake make;
emcc -O2 -s VERBOSE=1 -s TOTAL_MEMORY=33554432 -v libavutil/*.o libavcodec/*.o libavformat/*.o libavdevice/*.o libswresample/*.o libavfilter/*.o libswscale/*.o *.o -o ../ffmpeg_asm.js --pre-js ../ffmpeg_pre.js --post-js ../ffmpeg_post.js

cd ../

echo "Finished Build"
