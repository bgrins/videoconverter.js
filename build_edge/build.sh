#!/bin/sh
# Current build uses emscripten 1.31.0 (commit 01d531a5fabc58d9f83a628df5c5b43ecc5950d5)
echo "Beginning Build:"

PREFIX=$(pwd)/dist

clean_prefix() {
  echo clean_prefix
  rm -r $PREFIX
  mkdir -p $PREFIX
}

build_zlib() {
  echo build_zlib
  cd zlib
  emconfigure ./configure --prefix=$PREFIX --64 --static
  emmake make CFLAGS="-O3" SHELL="/bin/bash -x"
  emmake make install
  cd ..
}

build_libvpx() {
  echo build_libvpx
  cd libvpx
  ## Try some of these options: ./configure --target=js1-none-clang_emscripten --disable-examples --disable-docs --disable-multithread --disable-runtime-cpu-detect --disable-optimizations --disable-vp8-decoder --disable-vp9-decoder --extra-cflags="-O2"

  emconfigure ./configure --prefix=$PREFIX --disable-examples --disable-docs \
    --disable-runtime-cpu-detect --disable-multithread \
    --target=generic-gnu --extra-cflags=-O3

  #make clean
  emmake make -j4 SHELL="/bin/bash -x"
  emmake make install
  cd ..
}

build_x264() {
  echo build_x264
  # x264
  cd x264

  # x264 has .c files with same names, which causes linking problem if built as static library, so --enable-shared.
  emconfigure ./configure --disable-thread --disable-asm --disable-opencl \
    --host=i686-pc-linux-gnu \
    --disable-cli --enable-shared --disable-gpl --prefix=$PREFIX

  #make clean
  emmake make -j4 SHELL="/bin/bash -x"
  emmake make install
  cd ..
}

build_openh264() {
  echo build_openh264
  # openh264
  cd openh264
  # use mips to avoid compiling ASM codes
  emmake make -j4 ARCH=mips CFLAGS_OPT=-O3
  # use shared library becuase openh264 has object files with same name
  emmake make ARCH=mips PREFIX=$PREFIX install-headers install-shared
  cd ..
}

build_lame() {
  echo build_lame
  cd lame
  emconfigure ./configure CFLAGS=-O3 --prefix=$PREFIX --enable-shared=no --disable-gtktest --disable-decoder --disable-cpml --without-iconv
  emmake make -j4
  emmake make install
  cd ..
}

build_opus() {
  echo build_opus
  cd opus
  ./autogen.sh
  emconfigure ./configure CFLAGS=-O3 --prefix=$PREFIX --enable-shared=no --disable-asm --disable-rtcd \
  # --enable-fixed-point --enable-intrinsics

  emmake make -j4
  emmake make install
  cd ..
}

config_ffmpeg_full() {
  echo config_ffmpeg_full
  cd ffmpeg

  # ffmpeg's configure doesn't correctly set CPPFLAGS="-D_POSIX_C_SOURCE=200112 -D_XOPEN_SOURCE=600" when built by emcc, so we add them.
  emconfigure ./configure --cc="emcc" --prefix=$PREFIX \
    --pkg-config=../pkg_config \
    --optflags="-O3" --extra-cflags="-I$PREFIX/include -v -D_POSIX_C_SOURCE=200112 -D_XOPEN_SOURCE=600" \
    --extra-ldflags="-L$PREFIX/lib/" \
    --enable-cross-compile --target-os=none --arch=x86_32 --cpu=generic --disable-debug --disable-runtime-cpudetect \
    --disable-ffplay --disable-ffprobe --disable-ffserver --disable-asm --disable-doc --disable-pthreads --disable-w32threads --disable-network --disable-iconv --disable-xlib \
    --enable-gpl --enable-libvpx --enable-libx264 \
    --enable-libmp3lame --enable-libopus --enable-libopenh264 \
    --enable-protocol=file

  cd ..
}

config_ffmpeg_custom() {
  echo config_ffmpeg_custom
  cd ffmpeg

  # ffmpeg's configure doesn't correctly set CPPFLAGS="-D_POSIX_C_SOURCE=200112 -D_XOPEN_SOURCE=600" when built by emcc, so we add them.
  emconfigure ./configure --cc="emcc" --prefix=$PREFIX \
    --pkg-config=../pkg_config \
    --optflags="-O3" --extra-cflags="-I$PREFIX/include -v -D_POSIX_C_SOURCE=200112 -D_XOPEN_SOURCE=600" \
    --extra-ldflags="-L$PREFIX/lib/" \
    --enable-cross-compile --target-os=none --arch=x86_32 --cpu=generic --disable-debug --disable-runtime-cpudetect \
    --disable-ffplay --disable-ffprobe --disable-ffserver --disable-asm --disable-doc --disable-pthreads --disable-w32threads --disable-network --disable-iconv --disable-xlib \
    --enable-gpl --enable-libvpx --enable-libx264 \
    --enable-libmp3lame --enable-libopus --enable-libopenh264 \
    --disable-everything --enable-protocol=file \
    --enable-encoder=libopus --enable-encoder=libmp3lame --enable-encoder=aac --enable-encoder=libvpx_vp8 --enable-encoder=libvpx_vp9 --enable-encoder=libx264 --enable-encoder=libopenh264 --enable-encoder=vorbis \
    --enable-demuxer=aac --enable-demuxer=ac3 --enable-demuxer=ogg --enable-demuxer=pcm_alaw --enable-demuxer=pcm_f32be --enable-demuxer=pcm_f32le --enable-demuxer=pcm_f64be --enable-demuxer=pcm_f64le --enable-demuxer=pcm_mulaw --enable-demuxer=pcm_s16be --enable-demuxer=pcm_s16le --enable-demuxer=avi --enable-demuxer=pcm_s24be --enable-demuxer=avisynth --enable-demuxer=pcm_s24le --enable-demuxer=pcm_s32be --enable-demuxer=pcm_s32le --enable-demuxer=pcm_s8 --enable-demuxer=pcm_u16be --enable-demuxer=pcm_u16le --enable-demuxer=pcm_u24be --enable-demuxer=pcm_u24le --enable-demuxer=pcm_u32be --enable-demuxer=pcm_u32le --enable-demuxer=pcm_u8 --enable-demuxer=rm --enable-demuxer=m4v --enable-demuxer=matroska --enable-demuxer=mjpeg --enable-demuxer=mp3 --enable-demuxer=flac --enable-demuxer=mpegps --enable-demuxer=flv --enable-demuxer=mpegts --enable-demuxer=mpegtsraw --enable-demuxer=mpegvideo --enable-demuxer=vc1t --enable-demuxer=vc1 \
    --enable-muxer=mp4 --enable-muxer=oga --enable-muxer=ogg --enable-muxer=webm \
    --enable-filter=adelay --enable-filter=aresample --enable-filter=pad --enable-filter=aformat --enable-filter=rotate --enable-filter=scale --enable-filter=format --enable-filter=setdar --enable-filter=setsar --enable-filter=ashowinfo --enable-filter=showinfo --enable-filter=ass --enable-filter=atrim --enable-filter=join --enable-filter=trim --enable-filter=vflip --enable-filter=concat --enable-filter=copy --enable-filter=volume --enable-filter=crop --enable-filter=volumedetect --enable-filter=cropdetect \
    --enable-decoder=libvpx_vp8 --enable-decoder=libvpx_vp9 --enable-decoder=vorbis \
    --enable-decoder=zlib \
  #--enable-decoder=aac --enable-decoder=aac_latm --enable-decoder=h263 --enable-decoder=ac3 --enable-decoder=h263i --enable-decoder=ac3_fixed --enable-decoder=h263p --enable-decoder=h264 --enable-decoder=h264_crystalhd --enable-decoder=h264_vda --enable-decoder=h264_vdpau --enable-decoder=cook --enable-decoder=mjpeg --enable-decoder=mjpegb --enable-decoder=mp2 --enable-decoder=mp2float --enable-decoder=mp3 --enable-decoder=mp3adu --enable-decoder=pcm_u32be --enable-decoder=mp3adufloat --enable-decoder=pcm_u32le --enable-decoder=mp3float --enable-decoder=pcm_u8 --enable-decoder=mp3on4 --enable-decoder=pcm_zork --enable-decoder=mp3on4float --enable-decoder=mpeg1_vdpau --enable-decoder=mpeg1video --enable-decoder=mpeg2_crystalhd --enable-decoder=mpeg2video --enable-decoder=mpeg4 --enable-decoder=mpeg4_crystalhd --enable-decoder=mpeg4_vdpau --enable-decoder=mpeg_vdpau --enable-decoder=mpeg_xvmc --enable-decoder=mpegvideo --enable-decoder=msmpeg4_crystalhd --enable-decoder=vc1 --enable-decoder=msmpeg4v1 --enable-decoder=vc1_crystalhd --enable-decoder=msmpeg4v2 --enable-decoder=vc1_vdpau --enable-decoder=msmpeg4v3 --enable-decoder=ra_144 --enable-decoder=vc1image --enable-decoder=ra_288 --enable-decoder=ralf --enable-decoder=rv10 --enable-decoder=vp6 --enable-decoder=rv20 --enable-decoder=vp6a --enable-decoder=rv30 --enable-decoder=vp6f --enable-decoder=rv40 --enable-decoder=opus --enable-decoder=pcm_alaw --enable-decoder=pcm_bluray --enable-decoder=sipr --enable-decoder=pcm_dvd --enable-decoder=wmalossless --enable-decoder=pcm_f32be --enable-decoder=wmapro --enable-decoder=pcm_f32le --enable-decoder=wmav1 --enable-decoder=pcm_f64be --enable-decoder=wmav2 --enable-decoder=pcm_f64le --enable-decoder=wmavoice --enable-decoder=pcm_lxf --enable-decoder=wmv1 --enable-decoder=pcm_mulaw --enable-decoder=wmv2 --enable-decoder=pcm_s16be --enable-decoder=wmv3 --enable-decoder=pcm_s16be_planar --enable-decoder=wmv3_crystalhd --enable-decoder=pcm_s16le --enable-decoder=wmv3_vdpau --enable-decoder=pcm_s16le_planar --enable-decoder=wmv3image --enable-decoder=pcm_s24be --enable-decoder=pcm_s24daud --enable-decoder=pcm_s24le --enable-decoder=pcm_s24le_planar --enable-decoder=pcm_s32be --enable-decoder=pcm_s32le --enable-decoder=pcm_s32le_planar --enable-decoder=pcm_s8 --enable-decoder=pcm_s8_planar --enable-decoder=pcm_u16be --enable-decoder=pcm_u16le --enable-decoder=pcm_u24be --enable-decoder=pcm_u24le

  cd ..
}

make_ffmpeg() {
  echo make_ffmpeg
  cd ffmpeg

  # make clean
  emmake make -j4
  emmake make install

  cd ..
}

build_js() {
  echo build_js
  cp $PREFIX/bin/ffmpeg $PREFIX/ffmpeg.bc

  emcc -s OUTLINING_LIMIT=100000 -s VERBOSE=1 -s TOTAL_MEMORY=33554432 -O3 -v $PREFIX/ffmpeg.bc $PREFIX/lib/libx264.so $PREFIX/lib/libopenh264.so -o ffmpeg.js --pre-js ffmpeg_pre.js --post-js ffmpeg_post.js
}

install_js() {
  echo install_js
  cp -a ffmpeg.js ffmpeg.js.mem ../demo/
}

clean_prefix
build_zlib
build_libvpx
build_x264
build_openh264
build_lame
build_opus

# select one of the following ffmpeg configure.
# config_ffmpeg_full
config_ffmpeg_custom

make_ffmpeg
build_js
install_js

echo "Finished Build"
