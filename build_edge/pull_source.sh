#!/bin/sh

DEPTH=5

#param url, dir
pull_source() {
  echo "Pulling $1 to $2"
  if [ -d $2 ]
  then
    cd $2
    git pull
    diff=`git diff`
    if [ ${#diff} -gt 1 ]
    then
      echo "$diff" > ../$2.patch
      echo "$2.patch saved."
    fi
    cd ..
  else
    git clone --depth $DEPTH $1 $2
    # apply patch to the source code
    if [ -f $2.patch ]
    then
      patch -p1 -d $2 < $2.patch && echo "$2.patch applied."
    fi
  fi
}

pull_source https://github.com/FFmpeg/FFmpeg.git ffmpeg
pull_source git://git.opus-codec.org/opus.git opus
pull_source https://github.com/rbrito/lame.git lame
pull_source https://github.com/webmproject/libvpx.git libvpx
pull_source git://git.videolan.org/x264.git x264
pull_source https://github.com/cisco/openh264.git openh264
pull_source https://github.com/madler/zlib.git zlib
