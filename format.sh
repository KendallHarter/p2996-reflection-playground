#!/bin/bash

# Provide a script even though clang-format is run automatically because
# clang-format interacts oddly with reflection at times

for i in src/* src/json_schema2/*; do
   clang-format -i "${i}"
done
