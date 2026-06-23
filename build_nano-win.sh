#!/bin/bash -e

_top=$(cygpath -am "${PWD}" || realpath "${PWD}")
_nproc=$(nproc)
_ncurses_ver="6.6"

wget -c "https://invisible-island.net/archives/ncurses/ncurses-${_ncurses_ver}.tar.gz"
tar -xzvf "ncurses-${_ncurses_ver}.tar.gz"
./autogen.sh

function do_build()
{
  source shell $1 || true

  local _build="$(gcc -v 2>&1 | sed -En 's/^Target: (.*)$/\1/;T;p')"
  local _host="$2"
  local _prefix="$(cygpath -au ${_top} || realpath ${_top})/pkg_${_host}"

  export CPPFLAGS="-D__USE_MINGW_ANSI_STDIO -I\"${_prefix}/include\" -pipe"
  export CFLAGS="-O2 -g -masm=att"
  export LDFLAGS="-L\"${_prefix}/lib/\" -static"
  export LIBS="-lshlwapi"

  export PKG_CONFIG=true
  export NCURSESW_CFLAGS="-I\"${_prefix}/include/ncursesw\" -DNCURSES_STATIC"
  export NCURSESW_LIBS="-lncursesw"

  mkdir -p "${_top}/build_${_host}"
  pushd "${_top}/build_${_host}"

  mkdir -p "ncurses"
  pushd "ncurses"
  ../../ncurses-${_ncurses_ver}/configure  \
    --build="${_build}" --host="${_host}" --prefix="${_prefix}"  \
    --disable-dependency-tracking  \
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
    --disable-dependency-tracking  \
    --enable-{color,utf8,nanorc} --disable-{nls,speller,threads,rpath}
  make -j"${_nproc}"
  make install-strip
  popd
}

do_build ucrt64 x86_64-w64-mingw32
do_build mingw32 i686-w64-mingw32
