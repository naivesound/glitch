#!/bin/sh

VERSION=$(git describe --abbrev=0 --tags)

DIR=$(cd $(dirname $0)/..; pwd)
DISTNAME=glitch-linux-$(uname -m)-$VERSION
DISTDIR=$DIR/dist/$DISTNAME

cd $DIR
mkdir -p $DISTDIR

go generate github.com/naivesound/glitch/...
go test github.com/naivesound/glitch/...
go vet github.com/naivesound/glitch/...
go build -o $DISTDIR/glitch github.com/naivesound/glitch/cmd/glitch

cp $DIR/API.md $DISTDIR/API.md
cp $DIR/samples $DISTDIR/samples

cd $DIR/dist
tar czvf $DISTNAME.tar.gz $DISTNAME

ls -l $DISTNAME.tar.gz
