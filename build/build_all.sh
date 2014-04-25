# Current build uses emscripten at commit df11c6f1fd1636a355b83a1c48b3a890596e6a32
echo "Beginning Build:"

mkdir -p dist

# cd libogg
# make clean
# emconfigure ./configure --prefix=$(pwd)/../dist
# emmake make
# emmake make install
# cd ..

# cd libvorbis
# make clean
# emconfigure ./configure --disable-oggtest --prefix=$(pwd)/../dist
# emmake make
# emmake make install
# cd ..

cd zlib
make clean
emconfigure ./configure --prefix=$(pwd)/../dist --64
emmake make
emmake make install
cd ..

cd libvpx
make clean
emconfigure ./configure --prefix=$(pwd)/../dist --disable-examples --disable-docs --disable-multithread --disable-optimizations --target=generic-gnu
emmake make
emmake make install
cd ..

cd ffmpeg

emconfigure ./configure --cc="emcc" --prefix=$(pwd)/../dist --extra-cflags="-I$(pwd)/../dist/include"  --enable-cross-compile --target-os=none --arch=x86_64 --cpu=generic \
    --disable-ffplay --disable-ffprobe --disable-ffserver --disable-asm --disable-doc --disable-devices --disable-pthreads --disable-w32threads --disable-network \
    --disable-hwaccels --disable-parsers --disable-bsfs --disable-debug --disable-protocols --disable-indevs --disable-outdevs --enable-protocol=file \
    --enable-libvpx

make clean
make
make install

cd ..

rm dist/*.bc

# cp dist/lib/libogg.a dist/libogg.bc
# cp dist/lib/libvorbis.a dist/libvorbis.bc

cp dist/lib/libvpx.a dist/libvpx.bc
cp dist/lib/libz.a dist/libz.bc
cp ffmpeg/ffmpeg dist/ffmpeg.bc

cd dist
emcc -s OUTLINING_LIMIT=100000 -s VERBOSE=1 -s TOTAL_MEMORY=33554432 -O2 -v ffmpeg.bc libvpx.bc libz.bc -o ../ffmpeg_all.js --pre-js ../ffmpeg_pre.js --post-js ../ffmpeg_post.js
cd ..


echo "Finished Build"
