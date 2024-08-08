#!/bin/bash

base_dir=$(dirname $(realpath "$0"))

ln -s "${base_dir}/pre-commit" "${base_dir}/../.git/hooks/pre-commit"
