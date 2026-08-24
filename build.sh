#!/usr/bin/env sh
set -eu
cmake --preset ninja-release
cmake --build --preset release
