#!/bin/sh
set -x
DEBCHANGELOG=debian/changelog

l=`head -1 $DEBCHANGELOG`
NAME=`echo $l|sed 's/ .*//g'`
FULLVERSION=`echo $l|sed 's/.*(//g'|sed 's/).*//g'`
DEBVERSION=`echo $FULLVERSION|sed 's/-.*//g'`
. ./IDMETA
if test "$DEBVERSION" != "$VERSION"; then
    echo "WARNING: File VERSION and debian/changelog do not match"
    sleep 2
fi
git log >ChangeLog
git archive --format=tar --prefix=$NAME-$VERSION/ HEAD > $NAME-$VERSION.tar
tar xf $NAME-$VERSION.tar
cp ChangeLog $NAME-$VERSION
cd  $NAME-$VERSION
rm -fr debian
cd ..
tar cfz $NAME-$VERSION.tar.gz $NAME-$VERSION
rm $NAME-$VERSION.tar
rm -r $NAME-$VERSION
exit 0


