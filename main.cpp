//cd "c:\\Users\\joeya\\Documents\\GitHub\\CompilierAttempt\\" && g++ main.cpp -o tokenizer && "c:\\Users\\joeya\\Documents\\GitHub\\CompilierAttempt\\"tokenizer
#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <stdio.h>
#include <map>
#include <string>
#include <algorithm>
#include <filesystem>
#include "tokenizer.cpp"
#include "Scope.cpp"
#include "Stack.cpp"
#include "analyzer.cpp"
using namespace std; 
int main(int argc, char**args){
    string f; 
    if (argc == 1) f = "test.txt"; //technically bad practice, but not a big deal for now. 
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