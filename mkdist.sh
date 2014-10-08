#!/bin/sh
set -x
. ./IDMETA
git log >ChangeLog
git archive --format=tar --prefix=$NAME-$VERSION/ HEAD > $NAME-$VERSION.tar
tar xf $NAME-$VERSION.tar
cp ChangeLog $NAME-$VERSION
cd $NAME-$VERSION
rm -fr debian
cd ..
tar cfz $NAME-$VERSION.tar.gz $NAME-$VERSION
rm $NAME-$VERSION.tar
rm -r $NAME-$VERSION
exit 0


