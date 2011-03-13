#!/bin/sh
echo "Updating translations via TP" && \
rsync -Lrtvz  translationproject.org::tp/latest/nano/ .
#
NEWSTUFF=`svn status | grep "^\? .*.po$" | awk '{ print $NF }'`
if [ "x$NEWSTUFF" != "x" ]; then
    echo "Adding new languaes to SVN"
    svn add $NEWSTUFF
fi
#
echo "Updating LINGUAS for all translations"
FILE=LINGUAS
echo "# List of available languages." >$FILE
/bin/ls *.po | tr '\n' ' ' | sed 's/\.po//g' >>$FILE
echo >> $FILE
#
if [ "x$NEWSTUFF" != "x" ]; then
    echo -n "New langs found, re-running configure and make at top level (silently)..."
    (cd .. && ./configure  && make) >/dev/null
    echo "done"
fi
