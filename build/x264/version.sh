#!/bin/sh
[ -n "$1" ] && cd $1

git_version() {
trap 'rm -f config.git-hash' EXIT
git rev-list HEAD | sort > config.git-hash
LOCALVER=`wc -l config.git-hash | awk '{print $1}'`
if [ $LOCALVER \> 1 ] ; then
    VER=`git rev-list origin/master | sort | join config.git-hash - | wc -l | awk '{print $1}'`
    VER_DIFF=$(($LOCALVER-$VER))
    echo "#define X264_REV $VER"
    echo "#define X264_REV_DIFF $VER_DIFF"
    if [ $VER_DIFF != 0 ] ; then
        VER="$VER+$VER_DIFF"
    fi
    if git status | grep -q "modified:" ; then
        VER="${VER}M"
    fi
    VER="$VER $(git rev-list HEAD -n 1 | cut -c 1-7)"
    VERSION=" r$VER"
fi
}

VER="x"
VERSION=""
[ -d .git ] && (type git >/dev/null 2>&1) && git_version
echo "#define X264_VERSION \"$VERSION\""
API=`grep '#define X264_BUILD' < x264.h | sed -e 's/.* \([1-9][0-9]*\).*/\1/'`
echo "#define X264_POINTVER \"0.$API.$VER\""
