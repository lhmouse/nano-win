#!/bin/sh

# Execute this is the po/ subdir.
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

echo "Remerging and recompiling the PO files..."
make

#echo "Staging the files"
#git add -v nano.pot
#git add -v *.po
#git add -v LINGUAS
