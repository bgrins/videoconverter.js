# Current build uses emscripten 1.32.0 (commit b3efd9328f940034e1cab45af23bf29541e0d8ff)

echo "Beginning Build:"

rm -r dist
mkdir -p dist

cd zlib
make clean

# --static should be enforced because current emcc can't handle .lo files
emconfigure ./configure --prefix=$(pwd)/../dist --64 --static
emmake make -j2 CFLAGS="-O3"
emmake make install
cd ..

cd ffmpeg

#--enable-small

make clean

# ffmpeg's configure doesn't correctly set CPPFLAGS="-D_POSIX_C_SOURCE=200112 -D_XOPEN_SOURCE=600" when built by emcc, so we add them.
emconfigure ./configure --cc="emcc" --prefix=$(pwd)/../dist --enable-cross-compile --target-os=none --arch=x86_32 --cpu=generic \
    --optflags="-O3" --extra-cflags="-D_POSIX_C_SOURCE=200112 -D_XOPEN_SOURCE=600" \
    --disable-ffplay --disable-ffprobe --disable-ffserver --disable-asm --disable-doc --disable-devices --disable-pthreads --disable-w32threads --disable-network \
    --disable-hwaccels --disable-parsers --disable-bsfs --disable-debug --disable-protocols --disable-indevs --disable-outdevs --enable-protocol=file

emmake make -j2
emmake make install


cd ..

cd dist

rm *.bc

cp lib/libz.a dist/libz.bc
cp ../ffmpeg/ffmpeg ffmpeg.bc

emcc -s OUTLINING_LIMIT=100000 -s VERBOSE=1 -s TOTAL_MEMORY=33554432 -O3 -v ffmpeg.bc -o ../ffmpeg.js --pre-js ../ffmpeg_pre.js --post-js ../ffmpeg_post.js

cd ..


echo "Finished Build"
