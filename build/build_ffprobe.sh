
echo "Beginning Build:"

cd ffmpeg

emconfigure ./configure --cc="emcc" --prefix=$(pwd)/../dist --extra-cflags="-I$(pwd)/../dist/include"  --enable-cross-compile --target-os=none --arch=x86_32 --cpu=generic \
    --disable-ffplay --disable-ffmpeg --disable-ffserver --disable-asm --disable-doc --disable-devices --disable-pthreads --disable-w32threads --disable-network \
    --disable-hwaccels --disable-parsers --disable-bsfs --disable-debug --disable-protocols --disable-indevs --disable-outdevs --enable-protocol=file \
    --enable-nonfree --enable-gpl

make clean
make
cp ffprobe ffprobe.bc
emcc -s OUTLINING_LIMIT=100000 -s VERBOSE=1 -s TOTAL_MEMORY=33554432 -O0 -v ffprobe.bc -o ../ffprobe.js --pre-js ../ffmpeg_pre.js --post-js ../ffmpeg_post.js
cd ../

echo "Finished Build"