# Current build uses emscripten at commit df11c6f1fd1636a355b83a1c48b3a890596e6a32
echo "Beginning Build:"

rm -r dist
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
## Try some of these options: ./configure --target=js1-none-clang_emscripten --disable-examples --disable-docs --disable-multithread --disable-runtime-cpu-detect --disable-optimizations --disable-vp8-decoder --disable-vp9-decoder --extra-cflags="-O2"
make clean
emconfigure ./configure --prefix=$(pwd)/../dist --disable-examples --disable-docs \
  --disable-runtime-cpu-detect --disable-multithread --disable-optimizations \
  --target=generic-gnu
sed -i.bak -e 's/ARFLAGS = -crs$(if $(quiet),,v)/ARFLAGS = crs$(if $(quiet),,v)/' ./libs-generic-gnu.mk
emmake make
emmake make install
cd ..

# x264-snapshot-20160910-2245
cd x264
make clean
patch --forward ./configure ../fix_x264_configure.patch
emconfigure ./configure --disable-thread --disable-asm \
            --host=i686-pc-linux-gnu \
            --disable-cli --enable-static --disable-gpl --prefix=$(pwd)/../dist
emmake make
emmake make install
cd ..

cd ffmpeg

make clean
emconfigure ./configure --cc="emcc" --prefix=$(pwd)/../dist --extra-cflags="-I$(pwd)/../dist/include -v" --enable-cross-compile --target-os=none --arch=x86_32 --cpu=generic \
    --disable-ffplay --disable-ffprobe --disable-ffserver --disable-asm --disable-doc --disable-devices --disable-pthreads --disable-w32threads --disable-network \
    --disable-hwaccels --disable-parsers --disable-bsfs --disable-debug --disable-protocols --disable-indevs --disable-outdevs --enable-protocol=file \
    --enable-libvpx --enable-gpl --extra-libs="$(pwd)/../dist/lib/libx264.a $(pwd)/../dist/lib/libvpx.a"

# Because there doesn't appear to be a way to tell configure that arc4random isn't actually there
sed -i.bak -e 's/#define HAVE_ARC4RANDOM 1/#define HAVE_ARC4RANDOM 0/' ./config.h
sed -i.bak -e 's/HAVE_ARC4RANDOM=yes/HAVE_ARC4RANDOM=no/' ./config.mak

# If we --enable-libx264 there is an error.  Instead just act like it is there, extra-libs seems to work.
sed -i '' 's/define CONFIG_LIBX264 0/define CONFIG_LIBX264 1/' config.h
sed -i '' 's/define CONFIG_LIBX264_ENCODER 0/define CONFIG_LIBX264_ENCODER 1/' config.h
sed -i '' 's/define CONFIG_LIBX264RGB_ENCODER 0/define CONFIG_LIBX264RGB_ENCODER 1/' config.h
sed -i '' 's/define CONFIG_H264_PARSER 0/define CONFIG_H264_PARSER 1/' config.h

sed -i '' 's/\!CONFIG_LIBX264=yes/CONFIG_LIBX264=yes/' config.mak
sed -i '' 's/\!CONFIG_LIBX264_ENCODER=yes/CONFIG_LIBX264_ENCODER=yes/' config.mak
sed -i '' 's/\!CONFIG_LIBX264RGB_ENCODER=yes/CONFIG_LIBX264RGB_ENCODER=yes/' config.mak
sed -i '' 's/\!CONFIG_H264_PARSER=yes/CONFIG_H264_PARSER=yes/' config.mak

make
make install

cd ..

rm dist/*.bc

# cp dist/lib/libogg.a dist/libogg.bc
# cp dist/lib/libvorbis.a dist/libvorbis.bc

cp dist/lib/libvpx.a dist/libvpx.bc
cp dist/lib/libz.a dist/libz.bc
cp dist/lib/libx264.a dist/libx264.bc
cp ffmpeg/ffmpeg dist/ffmpeg.bc

cd dist
emcc -s VERBOSE=1 -s TOTAL_MEMORY=33554432 -O2 -v ffmpeg.bc libx264.bc  libvpx.bc libz.bc -o ../ffmpeg-all-codecs.js --pre-js ../ffmpeg_pre.js --post-js ../ffmpeg_post.js  -s WASM=1 -s "BINARYEN_METHOD='native-wasm'" -s ALLOW_MEMORY_GROWTH=1
cd ..

cp ffmpeg-all-codecs.js* ../demo
cp ffmpeg-all-codecs.wasm* ../demo

echo "Finished Build"
