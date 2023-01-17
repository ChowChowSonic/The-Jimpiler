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
#include "jimpilier.h"
#include "tokenizer.cpp"
#include "Scope.cpp"
#include "Stack.cpp"
#include "analyzer.cpp"
using namespace std; 
int main(int argc, char**args){
    string f; 
    if (argc == 1) f = "test.txt";
    else f = args[1]; 
    time_t now = time(nullptr);
    bool correct = analyzeFile(f); 
    time_t end = time(nullptr); 
    if(correct)
        cout << "Successful analysis of provided code - no syntax errors found"<<endl; 
    cout << "Code was compiled in approx: "<< (end - now) << " seconds"<<endl; 
    //while(!s.eof()){
    //    cout << s.next().lex << endl; 
    //} 
    //for(Token t : tokens){
    //    cout << std::setw(15) << keytokens[t.token]  << std::setw(15) <<"'" << t.lex<<"'" <<endl; 
    //} 
}