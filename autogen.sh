#!/bin/sh
# Generate configure & friends for CVS users.

aclocal -I ./m4
autoheader
automake --add-missing
autoconf
