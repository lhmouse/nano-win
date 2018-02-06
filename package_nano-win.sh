#!/bin/bash -e

_pkgversion="$(git describe --tags || echo "v0.0.0-0-unknown")"
_revision="$(git rev-list --count HEAD)"

strip -s {i686,x86_64}-w64-mingw32/bin/nano.exe
cp doc/sample.nanorc.in .nanorc
7z a -aoa -mmt"$(nproc)" -- "nano-win_${_revision}_${_pkgversion}.7z" {i686,x86_64}-w64-mingw32/{bin/nano.exe,share/{nano,doc}/} .nanorc
