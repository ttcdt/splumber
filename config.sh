#!/bin/sh

# Configuration shell script

TARGET=splumber

# gets program version
VERSION=`cut -f2 -d\" VERSION`

# default installation prefix
PREFIX=/usr/local

# store command line args for configuring the libraries
CONF_ARGS="$*"

MINGW32_PREFIX=x86_64-w64-mingw32

# parse arguments
while [ $# -gt 0 ] ; do

    case $1 in
    --help)         CONFIG_HELP=1 ;;

    --mingw32-prefix=*)     MINGW32_PREFIX=`echo $1 | sed -e 's/--mingw32-prefix=//'`
                            ;;

    --mingw32)          CC=${MINGW32_PREFIX}-gcc
                        WINDRES=${MINGW32_PREFIX}-windres
                        AR=${MINGW32_PREFIX}-ar
                        LD=${MINGW32_PREFIX}-ld
                        CPP=${MINGW32_PREFIX}-g++
                        ;;

    --prefix)       PREFIX=$2 ; shift ;;
    --prefix=*)     PREFIX=`echo $1 | sed -e 's/--prefix=//'` ;;
    esac

    shift
done

if [ "$CONFIG_HELP" = "1" ] ; then

    echo "Available options:"
    echo "--prefix=PREFIX       Installation prefix ($PREFIX)."
    echo "--mingw32             Build using the mingw32 compiler."

    echo
    echo "Environment variables:"
    echo "CC                    C Compiler."
    echo "CFLAGS                Compile flags (i.e., -O3)."

    exit 1
fi

echo "Configuring Space Plumber..."

echo "/* automatically created by config.sh - do not modify */" > config.h
echo "# automatically created by config.sh - do not modify" > makefile.opts
> config.ldflags
> config.cflags
> .config.log

# set compiler
if [ "$CC" = "" ] ; then
    CC=cc
    # if CC is unset, try if gcc is available
    which gcc > /dev/null

    if [ $? = 0 ] ; then
        CC=gcc
    fi
fi

if [ "$LD" = "" ] ; then
    LD=ld
fi

if [ "$TAR" = "" ] ; then
    TAR=tar
fi

echo "CC=$CC" >> makefile.opts
echo "LD=$LD" >> makefile.opts
echo "TAR=$TAR" >> makefile.opts

# set cflags
if [ "$CFLAGS" = "" ] ; then
    CFLAGS="-g -Wall"
fi

echo "CFLAGS=$CFLAGS" >> makefile.opts

# Add CFLAGS to CC
CC="$CC $CFLAGS"

# add version
cat VERSION >> config.h

# add installation prefix
#echo "#define CONFOPT_PREFIX \"$PREFIX\"" >> config.h

#########################################################

# qdgdf
echo -n "Looking for QDGDF... "

for QDGDF in ./qdgdf ../qdgdf NOTFOUND ; do
    if [ -d $QDGDF ] && [ -f $QDGDF/qdgdf_video.h ] ; then
        break
    fi
done

if [ "$QDGDF" != "NOTFOUND" ] ; then
    echo "-I$QDGDF" >> config.cflags
    echo "-L$QDGDF -lqdgdf" >> config.ldflags
    echo "OK ($QDGDF)"
else
    echo "No"
    exit 1
fi

if [ ! -f $QDGDF/Makefile ] ; then
    (echo ; cd $QDGDF ; ./config.sh $CONF_ARGS ) || exit 1
fi

grep CONFOPT $QDGDF/config.h >> config.h

cat ${QDGDF}/config.ldflags >> config.ldflags

# add all lines except 1-3 from QDGDF's config.h
#sed -e '1,3d;' < ${QDGDF}/config.h >> config.h

# if qdgdf includes the DirectDraw driver,
# a win32 executable is assumed
grep CONFOPT_DDRAW ${QDGDF}/config.h >/dev/null && TARGET=splumber.exe

#########################################################

# final setup

echo "QDGDF=$QDGDF" >> makefile.opts
echo "TARGET=$TARGET" >> makefile.opts
echo "VERSION=$VERSION" >> makefile.opts
echo "PREFIX=\$(DESTDIR)$PREFIX" >> makefile.opts
echo >> makefile.opts

cat makefile.opts makefile.in makefile.depend > Makefile

#########################################################

echo "Type 'make' to build Space Plumber."

# cleanup
rm -f .tmp.c .tmp.o

exit 0
