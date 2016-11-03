# Current build uses emscripten 1.32.0 (commit b3efd9328f940034e1cab45af23bf29541e0d8ff)
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
# --static should be enforced because current emcc can't handle .lo files
emconfigure ./configure --prefix=$(pwd)/../dist --64 --static
emmake make -j2 CFLAGS="-O3" SHELL="/bin/bash -x"
emmake make install

cd ..

cd libvpx
## Try some of these options: ./configure --target=js1-none-clang_emscripten --disable-examples --disable-docs --disable-multithread --disable-runtime-cpu-detect --disable-optimizations --disable-vp8-decoder --disable-vp9-decoder --extra-cflags="-O2"

make clean

emconfigure ./configure --prefix=$(pwd)/../dist --disable-examples --disable-docs \
  --disable-runtime-cpu-detect --disable-multithread --disable-optimizations \
  --target=generic-gnu --extra-cflags="-O3"

emmake make -j2 SHELL="/bin/bash -x"
emmake make install

cd ..

# x264-snapshot-20140501-2245
cd x264
make clean

# x264 has .c files with same names, which causes linking problem if built as static library, so --enable-shared.
emconfigure ./configure --disable-thread --disable-asm \
  --host=i686-pc-linux-gnu \
  --disable-cli --enable-shared --disable-gpl --prefix=$(pwd)/../dist

emmake make -j2 SHELL="/bin/bash -x"
emmake make install

cd ..

cd ffmpeg

make clean

# ffmpeg's configure doesn't correctly set CPPFLAGS="-D_POSIX_C_SOURCE=200112 -D_XOPEN_SOURCE=600" when built by emcc, so we add them.
emconfigure ./configure --cc="emcc" --prefix=$(pwd)/../dist \
    --optflags="-O3" --extra-cflags="-I$(pwd)/../dist/include -v -D_POSIX_C_SOURCE=200112 -D_XOPEN_SOURCE=600" \
    --enable-cross-compile --target-os=none --arch=x86_32 --cpu=generic \
    --disable-ffplay --disable-ffprobe --disable-ffserver --disable-asm --disable-doc --disable-devices --disable-pthreads --disable-w32threads --disable-network \
    --disable-hwaccels --disable-parsers --disable-bsfs --disable-debug --disable-protocols --disable-indevs --disable-outdevs --enable-protocol=file \
    --enable-libvpx --enable-libx264 --enable-gpl \
    --extra-ldflags="-L$(pwd)/../dist/lib/ -lx264 -lvpx -lz"

emmake make -j2
emmake make install

cd ..

rm dist/*.bc

# cp dist/lib/libogg.a dist/libogg.bc
# cp dist/lib/libvorbis.a dist/libvorbis.bc

cp ffmpeg/ffmpeg dist/ffmpeg.bc

cd dist

emcc -s OUTLINING_LIMIT=100000 -s VERBOSE=1 -s TOTAL_MEMORY=33554432 -O3 -v ffmpeg.bc lib/libx264.so -o ../ffmpeg-all-codecs.js --pre-js ../ffmpeg_pre.js --post-js ../ffmpeg_post.js

cd ..

cp ffmpeg-all-codecs.js ../demo/ffmpeg.js
cp ffmpeg-all-codecs.js.mem ../demo/ffmpeg.js.mem

echo "Finished Build"
