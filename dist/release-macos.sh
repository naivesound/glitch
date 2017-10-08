#!/bin/sh

VERSION=$(git describe --abbrev=0 --tags)

DIR=$(cd $(dirname $0)/..; pwd)
DISTNAME=glitch-macos-$(uname -m)-$VERSION
DISTDIR=$DIR/dist/$DISTNAME/

cd $DIR
mkdir -p $DISTDIR/Glitch.app/Contents/{MacOS,Resources}

go generate github.com/naivesound/glitch/...
go test github.com/naivesound/glitch/...
go vet github.com/naivesound/glitch/...
go build -o $DISTDIR/Glitch.app/Contents/MacOS/Glitch github.com/naivesound/glitch/cmd/glitch

cat > $DISTDIR/Glitch.app/Contents/Info.plist << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>Glitch</string>
    <key>CFBundleGetInfoString</key>
    <string>Glitch</string>
    <key>CFBundleIconFile</key>
    <string>Glitch</string>
    <key>CFBundleIdentifier</key>
    <string>com.naivesound.glitch</string>
    <key>CFBundleName</key>
    <string>Glitch</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
</dict>
</plist>
EOF
cp $DIR/dist/icons/glitch.icns $DISTDIR/Glitch.app/Contents/Resources/Glitch.icns

cp $DIR/LICENSE $DISTDIR/LICENSE.txt
cp $DIR/API.md $DISTDIR/API.md
cp -r $DIR/examples $DISTDIR/examples
cp -r $DIR/samples $DISTDIR/samples

cd $DIR/dist
tar czvf $DISTNAME.tar.gz $DISTNAME

ls -l $DISTNAME.tar.gz
