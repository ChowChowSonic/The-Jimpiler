#!/bin/bash
g++ -g -O3 -c `llvm-config --cxxflags --ldflags --system-libs --libs core` $1 -o unlinked_exe && 
g++ unlinked_exe $(llvm-config --ldflags --libs) -lpthread -o jmb &&
rm -rf unlinked_exe &&
g++ -std=c++14 -o runTests -Wl,--copy-dt-needed-entries -lgtest_main -lgtest  `ls tests/*`