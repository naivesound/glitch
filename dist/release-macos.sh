#!/bin/sh

VERSION=0.0.0

DIR=$(cd $(dirname $0)/..; pwd)
DISTNAME=glitch-macos-$(uname -m)-$VERSION
DISTDIR=$DIR/dist/$DISTNAME/

cd $DIR
mkdir -p $DISTDIR/Glitch.app/Contents/MacOS
mkdir -p $DISTDIR/Glitch.app/Contents/Resources

go get -u github.com/jteeuwen/go-bindata/...
go get ./...
go generate ./...
go test ./...

go build -o $DISTDIR/Glitch.app/Contents/MacOS/Glitch ./cmd/glitch

cp $DIR/dist/icons/glitch.icns $DISTDIR/Glitch.app/Contents/Resources/Glitch.icns
cp $DIR/API.md $DISTDIR/API.md
cp -rv $DIR/samples $DISTDIR/samples

cd $DIR/dist
tar czvf $DISTNAME.tar.gz $DISTNAME
