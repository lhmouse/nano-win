#!/bin/bash -e

_build="$(gcc -v 2>&1 | sed -En 's/^Target: (.*)$/\1/;T;p')"
_host="${1:-x86_64}-w64-mingw32"
_pwd="$(cygpath -am $(pwd) || readlink -f $(pwd))"
_nproc="$(nproc)"
_prefix="$(cygpath -au $(pwd) || readlink -f $(pwd))/pkg_${_host}"

export CPPFLAGS="-D__USE_MINGW_ANSI_STDIO -I\"${_prefix}/include\""
export CFLAGS="-O2 -g3 -flto"
export LDFLAGS="-L\"${_prefix}/lib/\" -static -flto"
export LIBS="-lshlwapi -lbcrypt"

export PKG_CONFIG=false  # always fails
export NCURSESW_CFLAGS="-I\"${_prefix}/include/ncursesw\" -DNCURSES_STATIC"
export NCURSESW_LIBS="-lncursesw"

wget -c "https://invisible-mirror.net/archives/ncurses/ncurses-6.4.tar.gz"
tar -xzvf ncurses-6.4.tar.gz
patch -p1 < ncurses-6.4.patch
./autogen.sh

mkdir -p "${_pwd}/build_${_host}"
pushd "${_pwd}/build_${_host}"

mkdir -p "ncurses"
pushd "ncurses"
../../ncurses-6.4/configure  \
  --build="${_build}" --host="${_host}" --prefix="${_prefix}"  \
  --enable-{widec,sp-funcs,termcap,term-driver,interop}  \
  --disable-{shared,database,rpath,home-terminfo,db-install,getcap}  \
  --without-{ada,cxx-binding,manpages,pthread,debug,tests,libtool,progs}
make -j"${_nproc}"
make install
popd

mkdir -p "nano"
pushd "nano"
touch roll-a-release.sh  # enable use of `git describe`
../../configure  \
  --build="${_build}" --host="${_host}" --prefix="${_prefix}"  \
  --enable-{color,utf8,nanorc} --disable-{nls,speller,threads,rpath}
make -j"${_nproc}"
make install-strip
popd
