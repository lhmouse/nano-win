#!/bin/sh
# Generate configure & friends for CVS users.

aclocal
automake --add-missing
autoheader
autoconf
