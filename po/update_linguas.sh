#!/bin/sh

# Let this be executed in the po/ subdir.
cd "$(dirname "$0")"  ||  exit 1

echo "Updating translations via TP"
# First remove existing PO files, as wget will not overwrite them.
rm *.po
wget --recursive --level=1 --accept=po --no-directories --no-verbose \
       https://translationproject.org/latest/nano/  ||  exit 2

# Are there now PO files that are not in git yet?
NEWSTUFF=$(git status --porcelain *.po | grep "^??")

if [ -n "${NEWSTUFF}" ]; then
    echo "New languages found; updating LINGUAS"
    echo "# List of available languages." >LINGUAS
    echo $(printf '%s\n' *.po | LC_ALL=C sort | sed 's/\.po//g') >>LINGUAS
fi

echo "Regenerating POT file and remerging and recompiling PO files..."
make update-po

# Ensure that the PO files are newer than the POT.
touch *.po

# If needed, fix a problem in the Makefile template.
grep -q '^datarootdir' Makefile.in.in || \
	sed -i 's/^\(datadir.*\)/datarootdir = @datarootdir@\n\1/' Makefile.in.in
