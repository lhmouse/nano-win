#!/bin/bash

VERSION="3.2"

./configure -C --enable-tiny &&  make &&
./configure -C --disable-wrapping-as-root &&

echo "Running autogen..." &&  ./autogen.sh &&
rm -v -f m4/*.m4~ *.asc *.sig *.gz *.xz &&

echo "Rebuilding..." &&  make &&
po/update_linguas.sh &&

make distcheck &&  make dist-xz &&

git commit -a -m "$(git log -1 --grep 'po: up' | grep o: | sed 's/^    //')" &&

gpg -a -b nano-$VERSION.tar.gz &&
gpg -a -b nano-$VERSION.tar.xz &&
gpg --verify nano-$VERSION.tar.gz.asc &&
gpg --verify nano-$VERSION.tar.xz.asc &&

git tag -u A0ACE884 -a v$VERSION -m "the nano $VERSION release" &&

for file in nano-$VERSION.tar.*z*; do scp $file bens@wh0rd.org:$file; done &&

gnupload --to ftp.gnu.org:nano  nano-$VERSION.tar.*z &&

echo "Tarballs have been rolled and uploaded."
