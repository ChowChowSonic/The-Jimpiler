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

    jimpilier::ctxt = std::make_unique<llvm::LLVMContext>();
    jimpilier::GlobalVarsAndFunctions = std::make_unique<llvm::Module>("Jimbo jit", *jimpilier::ctxt);
    jimpilier::builder = std::make_unique<llvm::IRBuilder<>>(*jimpilier::ctxt);
    Stack<Token> tokens = jimpilier::loadTokens(f);
    std::unique_ptr<jimpilier::ExprAST> x; 
    while(!tokens.eof()) {
        x = jimpilier::getValidStmt(tokens); 
        if(jimpilier::errored) break; 
        if(x != NULL)x->codegen(); 
        if(jimpilier::DEBUGGING) std::cout<< endl;  
    }
    time_t end = time(nullptr); 
    std::cout << "Code was compiled in approx: "<< (end - now) << " seconds"<<endl; 
    jimpilier::GlobalVarsAndFunctions->dump();
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