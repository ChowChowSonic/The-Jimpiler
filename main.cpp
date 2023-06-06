/*
Shell command to run the file: (I should probably just run this in a .sh)
g++ -g -O3 -c `llvm-config --cxxflags --ldflags --system-libs --libs core` main.cpp -o unlinked_exe && 
g++ unlinked_exe $(llvm-config --ldflags --libs) -lpthread -o jmb && ./jmb
*/
#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <stdio.h>
#include <map>
#include <string>
#include <algorithm>
#include <filesystem>
#include <stack>
#include "tokenizer.cpp"
#include "Scope.cpp"
#include "Stack.cpp"
#include "analyzer.cpp"
#include "jimpilier.h"
//#include "compilierbeta.cpp" 

using namespace std; 
int main(int argc, char**args){
    string f; 
    if (argc == 1) f = "test.txt";
    else f = args[1]; 
    time_t now = time(nullptr);
    bool correct = analyzer::analyzeFile(f); 
    time_t end = time(nullptr); 
    if(correct)
        cout << "Successful analysis of provided code - no syntax errors found"<<endl; 
    cout << "Code was compiled in approx: "<< (end - now) << " seconds"<<endl; 
    
    scope* s;

/*  fstream outputfile; 
    outputfile.open("out.s", std::fstream::out); 
    SLL<scope> reversed; 
    while((s = scopes.pop()) != NULL){
        reversed.emplace_back(*s); 
    }
    while((s = reversed.pop()) != NULL){
       s->dumpASM(outputfile); 
    }
    outputfile.close();//*/
}