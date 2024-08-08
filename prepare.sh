#!/bin/bash

# Repo tag to use for the Clang repository
CLANG_TAG=p2996

# A safe Clang tag that is known to build
SAFE_CLANG_TAG=b28f8995d056d3740540fda0abef66f09de811f8

# Make sure the script works anywhere
cd "$(dirname $(realpath "$0"))"
base_dir=$(pwd)

function clone_repo {
   # $1 == repo name
   # $2 == git tag to clone
   git init
   git remote add origin "$1"
   git fetch --depth 1 origin "$2"
   git checkout FETCH_HEAD
   git submodule update --init
}

mkdir -p tools/bin
pushd tools

if ! [ -f "./bin/clang++" ]; then
   if [[ ${GUARANTEE_BUILD:-0} != 0 ]]; then
      clang_tag_to_use=${SAFE_CLANG_TAG}
   else
      clang_tag_to_use=${CLANG_TAG}
   fi

   mkdir clang-p2996
   pushd clang-p2996
   clone_repo "https://github.com/bloomberg/clang-p2996.git" "${clang_tag_to_use}"

   cmake -DCMAKE_BUILD_TYPE=Release -B build -G Ninja \
      -DLLVM_ENABLE_PROJECTS="clang" \
      -DCMAKE_INSTALL_PREFIX="$(pwd)/.." \
      -S llvm \
      -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind"

   cmake --build build --target install

   popd
fi

popd

CC="$(pwd)/tools/bin/clang" CXX="$(pwd)/tools/bin/clang++" \
   cmake \
   -DCMAKE_EXPORT_COMPILE_COMMANDS=True \
   -DCMAKE_BUILD_TYPE=Debug -B build -G Ninja
