#!/bin/bash

LD_LIBRARY_PATH="$(pwd)/tools/lib/x86_64-unknown-linux-gnu/" "./build/$1"
