#!/bin/bash -e

./autogen.sh

wget -c "https://invisible-mirror.net/archives/ncurses/ncurses-6.1.tar.gz"
tar -xzvf ncurses-6.1.tar.gz
patch -p1 < ncurses-6.1.patch

_pwd="$(cygpath -m $(pwd) || readlink -f $(pwd))"
_nproc="$(nproc)"

function do_build()
  {
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
    ../../ncurses-6.1/configure --host="${_host}" --prefix="${_prefix}"  \
      --without-ada --without-cxx-binding --disable-db-install --without-manpages  \
      --without-pthread --without-debug --enable-widec --disable-database  \
      --disable-rpath --enable-termcap --disable-home-terminfo --enable-sp-funcs  \
      --enable-term-driver --enable-static --disable-shared
    make -j"${_nproc}"
    make install
    popd

    mkdir -p "nano"
    pushd "nano"
    touch roll-a-release.sh  # Lie to configure.ac to make use of `git describe`.
    NCURSES_CFLAGS="-I\"${_prefix}/include/ncursesw\"" NCURSES_LIBS="-lncursesw"  \
      ../../configure --host="${_host}" --prefix="${_prefix}" --enable-nanorc  \
      --enable-color --disable-utf8 --disable-nls --disable-speller  \
      --disable-threads --disable-rpath
    make -j"${_nproc}"
    make install-strip
    popd

    cd "${_pwd}"
  }

do_build i686
do_build x86_64
