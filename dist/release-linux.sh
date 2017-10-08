#!/bin/sh

VERSION=0.0.0

DIR=$(cd $(dirname $0)/..; pwd)
DISTNAME=glitch-linux-$(uname -m)-$VERSION
DISTDIR=$DIR/dist/$DISTNAME

cd $DIR
mkdir -p $DISTDIR

go get -u github.com/jteeuwen/go-bindata/...
go get ./...
go generate ./...
go test ./...

go build -o $DISTDIR/glitch ./cmd/glitch

cp $DIR/API.md $DISTDIR/API.md
cp -rv $DIR/samples $DISTDIR/samples

cd $DIR/dist
tar czvf $DISTNAME.tar.gz $DISTNAME

