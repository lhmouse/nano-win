#!/bin/sh
# Generate configure & friends for CVS users.

autoheader
aclocal -I ./m4
automake --add-missing
autoconf
