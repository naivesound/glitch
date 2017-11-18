#!/bin/sh

VERSION=$(git describe --abbrev=0 --tags)

DIR=$(cd $(dirname $0)/..; pwd)
DISTNAME=glitch-web-$(uname -m)-$VERSION
DISTDIR=$DIR/dist/$DISTNAME

cd $DIR
mkdir -p $DISTDIR

EMCC_EXPORT="['_glitch_init','_glitch_create','_glitch_destroy','_glitch_reset','_glitch_compile','_glitch_set','_glitch_get','_glitch_midi','_glitch_set_sample_loader','_glitch_add_sample','_glitch_remove_sample','_glitch_fill']"

#
# asm.js build
#
docker run --rm -v $DIR/core:/src naivesound/emcc bash -c \
  "mkdir -p build-asmjs && \
  cd build-asmjs && \
  /emscripten/emcmake cmake .. \
  && /emscripten/emmake make && \
  emcc libglitch-core.a \
    -o glitch-asm.js \
    --use-preload-plugins \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORTED_FUNCTIONS=\"$EMCC_EXPORT\" -O3"
cp $DIR/core/build-asmjs/glitch-asm.js $DIR/core/build-asmjs/glitch-asm.js.mem $DISTDIR

#
# WebAssembly build
#
docker run --rm -v $DIR/core:/src naivesound/emcc bash -c \
  "mkdir -p build-wasm && \
  cd build-wasm && \
  /emscripten/emcmake cmake .. \
  && /emscripten/emmake make && \
  emcc libglitch-core.a \
    -o glitch.html \
    --use-preload-plugins \
    -s WASM=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORTED_FUNCTIONS=\"$EMCC_EXPORT\" -O3"
cp $DIR/core/build-wasm/glitch.wasm $DISTDIR
cp $DIR/core/build-wasm/glitch.js $DISTDIR/glitch-loader.js

# Copy other javascript assets
cp -r $DIR/ui/app.js $DIR/ui/styles.css $DIR/ui/index.html $DIR/ui/vendor $DISTDIR
