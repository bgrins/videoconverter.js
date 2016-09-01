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

make -j4
make install


cd ..

cd dist

rm *.bc

cp lib/libz.a libz.bc
cp ../ffmpeg/ffmpeg ffmpeg.bc

emcc -s OUTLINING_LIMIT=100000 -v -s TOTAL_MEMORY=33554432 -O2 ffmpeg.bc -o ../ffmpeg.js --pre-js ../ffmpeg_pre.js --post-js ../ffmpeg_post.js

cd ..

cp ffmpeg.js* ../demo


echo "Finished Build"
