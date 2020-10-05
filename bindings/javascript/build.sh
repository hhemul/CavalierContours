#!/bin/sh

# install emscripten https://emscripten.org/docs/getting_started/downloads.html

emcc --bind --no-entry -I../../include -O3 -g4 --source-map-base / -fsanitize=address -s ALLOW_MEMORY_GROWTH  -s ASSERTIONS=1 -s WASM=1 -s STRICT=1 -s INVOKE_RUN=0 -s NO_FILESYSTEM=1 -s ENVIRONMENT=web -o bindings.js bindings.cpp

# ENVIRONMENT must be one of
#    'web'     - the normal web environment.
#    'webview' - just like web, but in a webview like Cordova;
#                considered to be same as "web" in almost every place
#    'worker'  - a web worker environment.
#    'node'    - Node.js.
#    'shell'   - a JS shell like d8, js, or jsc.
# Or it can be a comma-separated list of them, e.g., "web,worker". If this is
# the empty string, then all runtime environments are supported.

# use http static server for test.html (for example: npm i -g http-server)
