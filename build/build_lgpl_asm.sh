# Current build uses emscripten at commit df11c6f1fd1636a355b83a1c48b3a890596e6a32

echo "Beginning Build:"

rm -r dist
mkdir -p dist

cd zlib
make clean
emconfigure ./configure --prefix=$(pwd)/../dist --64
emmake make
emmake make install
cd ..

cd ffmpeg

#--enable-small

make clean
emconfigure ./configure --cc="emcc" --prefix=$(pwd)/../dist --enable-cross-compile --target-os=none --arch=x86_32 --cpu=generic \
    --disable-ffplay --disable-ffprobe --disable-ffserver --disable-asm --disable-doc --disable-devices --disable-pthreads --disable-w32threads --disable-network \
    --disable-hwaccels --disable-parsers --disable-bsfs --disable-debug --disable-protocols --disable-indevs --disable-outdevs --enable-protocol=file \

make
make install


cd ..

cd dist

rm *.bc

cp lib/libz.a dist/libz.bc
cp ../ffmpeg/ffmpeg ffmpeg.bc

emcc -s OUTLINING_LIMIT=100000 -s VERBOSE=1 -s TOTAL_MEMORY=33554432 -O2 -v ffmpeg.bc -o ../ffmpeg.js --pre-js ../ffmpeg_pre.js --post-js ../ffmpeg_post.js

cd ..


echo "Finished Build"
