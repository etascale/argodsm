#! /usr/bin/env bash

VSN=1.7.0

cd tests
wget https://github.com/google/googletest/archive/release-${VSN}.zip
unzip release-${VSN}.zip
mv googletest-release-${VSN} gtest-1.7.0  # FIXME PLEASE AS IN #40
cd ..
