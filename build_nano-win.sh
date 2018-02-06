#!/bin/bash

set -e

./autogen.sh

tar -xzvf ncurses-6.0.tar.gz
patch -p1 < ncurses-6.0.patch

function Build(){
  _pwd="$(cygpath -m $(pwd))"
  _nproc="$(nproc)"
  _host="${1}-w64-mingw32"
  _prefix="${_pwd}/${_host}"

  export PKG_CONFIG="true"  # Force it to succeed.
  export CPPFLAGS="-D__USE_MINGW_ANSI_STDIO -I\"${_prefix}/include\""
  export CFLAGS="-O2"
  export LDFLAGS="-O2 -L\"${_prefix}/lib/\" -static -Wl,-s"

  mkdir -p "${_pwd}/${_host}"
  mkdir -p "${_pwd}/build_${_host}"
  pushd "${_pwd}/build_${_host}"

  mkdir -p "ncurses"
  pushd "ncurses"
  ../../ncurses-6.0/configure --{host,build}="${_host}" --prefix="${_prefix}"  \
    --without-ada --without-cxx-binding --disable-db-install --without-manpages --without-pthread --without-debug  \
    --enable-widec --disable-database --disable-rpath --enable-termcap --disable-home-terminfo --enable-sp-funcs --enable-term-driver  \
    --enable-static --enable-shared
  make -j"${_nproc}"
  make install
  popd

  mkdir -p "nano"
  pushd "nano"
  mkdir -p .git  # Lie to configure.ac to make use of `git describe`.
  NCURSES_CFLAGS="-I\"${_prefix}/include/ncursesw\"" NCURSES_LIBS="-lncursesw" ../../configure --{host,build}="${_host}" --prefix="${_prefix}"  \
    --enable-nanorc --enable-color --disable-justify --disable-utf8 --disable-speller --disable-nls
  make -j"${_nproc}"
  make install
  popd

  popd
}

Build i686
Build x86_64
