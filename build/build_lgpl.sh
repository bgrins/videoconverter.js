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

CPPFLAGS="-D_XOPEN_SOURCE=600" emconfigure ./configure --cc="emcc" --prefix=$(pwd)/../dist --enable-cross-compile --target-os=none --arch=x86_32 --cpu=generic \
    --disable-ffplay --disable-ffprobe --disable-ffserver --disable-asm --disable-doc --disable-devices --disable-pthreads --disable-w32threads --disable-network \
    --disable-hwaccels --disable-parsers --disable-bsfs --disable-debug --disable-protocols --disable-indevs --disable-outdevs --enable-protocol=file

# Because there doesn't appear to be a way to tell configure that arc4random isn't actually there
sed -i.bak -e 's/#define HAVE_ARC4RANDOM 1/#define HAVE_ARC4RANDOM 0/' ./config.h
sed -i.bak -e 's/HAVE_ARC4RANDOM=yes/HAVE_ARC4RANDOM=no/' ./config.mak

make -j4
make install


cd ..

cd dist

rm *.bc

cp lib/libz.a libz.bc
cp ../ffmpeg/ffmpeg ffmpeg.bc

emcc -v -s TOTAL_MEMORY=33554432 -Os ffmpeg.bc -o ../ffmpeg.js --pre-js ../ffmpeg_pre.js --post-js ../ffmpeg_post.js -s WASM=1 -s "BINARYEN_METHOD='native-wasm'" -s ALLOW_MEMORY_GROWTH=1

cd ..

cp ffmpeg.js* ../demo
cp ffmpeg.wasm* ../demo

echo "Finished Build"
