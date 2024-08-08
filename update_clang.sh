#!/bin/bash

cd "$(dirname $(realpath "$0"))"

if ! [ -f "./tools/bin/clang++" ]; then
   echo "Local Clang installation doesn't seem to exist." 1>&2
else
   cd tools/clang-p2996
   git fetch --depth 1 origin p2996
   git checkout FETCH_HEAD
   cmake --build build --target install
fi
