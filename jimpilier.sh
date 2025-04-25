#!/bin/bash
g++ -g -O3 -c -fexceptions -lfmt `llvm-config-14 --cxxflags --ldflags --system-libs --libs core | sed 's/-fno-exceptions//g'` $1 -o unlinked_exe && 
g++ unlinked_exe $(llvm-config-14 --ldflags --libs) -lfmt -lpthread -o jmb &&
rm -rf unlinked_exe &&
g++ -std=c++14 -o runTests -Wl,--copy-dt-needed-entries -lgtest_main -lgtest  `ls tests/*`