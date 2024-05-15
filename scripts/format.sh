#!/bin/sh

clang-format -i \
$(find include/ -iname '*.hpp') \
$(find src/ -iname '*.hpp') \
$(find src/ -iname '*.cpp')

