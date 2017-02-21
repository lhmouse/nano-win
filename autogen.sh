#!/bin/sh
# Generate configure & friends for GIT users.

gnulib_url="git://git.sv.gnu.org/gnulib.git"
gnulib_hash="4084b3a1094372b960ce4a97634e08f4538c8bdd"

modules="
	getdelim
	getline
	getopt-gnu
	isblank
	iswblank
	regex
	snprintf-posix
	strcase
	strcasestr-simple
	strnlen
	vsnprintf-posix
"

# Make sure the local gnulib git repo is up-to-date.
if [ ! -d "gnulib" ]; then
	git clone --depth=123 ${gnulib_url}
fi
cd gnulib >/dev/null || exit 1
curr_hash=$(git log -1 --format=%H)
if [ "${gnulib_hash}" != "${curr_hash}" ]; then
	git pull
	git checkout -f ${gnulib_hash}
fi
cd .. >/dev/null || exit 1

rm -rf lib
./gnulib/gnulib-tool \
	--import \
	${modules}

autoreconf -f -i -s
