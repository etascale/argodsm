#! /usr/bin/env bash

cd tests
wget https://github.com/google/googletest/archive/release-1.10.0.zip
unzip release-1.10.0.zip
mv googletest-release-1.10.0 gtest-1.7.0  # FIXME PLEASE AS IN #40
cd ..
