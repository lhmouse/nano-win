#!/bin/sh
# Generate configure & friends for CVS users.

aclocal -I ./m4
automake --add-missing
autoheader
autoconf
