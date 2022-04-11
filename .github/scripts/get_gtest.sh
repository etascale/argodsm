#! /usr/bin/env bash

VSN=1.11.0

cd tests
wget https://github.com/google/googletest/archive/release-${VSN}.zip
unzip release-${VSN}.zip
mv googletest-release-${VSN} googletest
rm release-${VSN}.zip
cd ..
