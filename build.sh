HOST_PATH=/home/khangaroo/github/buildroot/output/gcw0/host
PATH=$HOST_PATH/bin:$HOST_PATH/mipsel-gcw0-linux-uclibc/sysroot/usr/bin:$PATH

premake5 gmake --no-asoundlib
make -j${nproc} -C build config=release CC=mipsel-linux-gcc CXX=mipsel-linux-g++ AR=mipsel-linux-ar th06