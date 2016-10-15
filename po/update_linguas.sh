#!/bin/sh

# Let this be executed in the po/ subdir.
cd "$(dirname "$0")" || exit

echo "Updating translations via TP"
rsync -Lrtvz  translationproject.org::tp/latest/nano/ .

# Are there now PO files that are not in git yet?
NEWSTUFF=$(git status --porcelain *.po | grep "^??")

if [ -n "${NEWSTUFF}" ]; then
    echo "New languages found; updating LINGUAS"
    echo "# List of available languages." >LINGUAS
    echo $(printf '%s\n' *.po | LC_ALL=C sort | sed 's/\.po//g') >>LINGUAS
fi

echo "Regenerating POT file and remerging and recompiling PO files..."
make update-po

# If needed, fix a problem in the Makefile template.
grep -q '^datarootdir' Makefile.in.in || \
	sed -i 's/^\(datadir.*\)/datarootdir = @datarootdir@\n\1/' Makefile.in.in
