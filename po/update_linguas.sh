#!/bin/sh
cd "$(dirname "$0")" || exit
echo "Updating translations via TP" && \
rsync -Lrtvz  translationproject.org::tp/latest/nano/ .
git add -v *.po
echo "Updating LINGUAS for all translations"
FILE=LINGUAS
echo "# List of available languages." >"${FILE}"
echo $(printf '%s\n' *.po | LC_ALL=C sort | sed 's/\.po//g') >>"${FILE}"
git add -v "${FILE}"
NEWSTUFF=$(git status --porcelain *.po)
if [ -n "${NEWSTUFF}" ]; then
    printf "New langs found, re-running configure and make at top level (silently)..."
    (cd .. && ./configure  && make) >/dev/null
    echo "done"
fi
