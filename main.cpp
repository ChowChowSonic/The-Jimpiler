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

using namespace std; 
int main(int argc, char**args){
    std::vector<std::string> all_args;
    if (argc > 1) {
        all_args.assign(args + 1, args + argc);

    }else {
        std::cout << "Error: No args provided" <<endl; 
        return 1; 
    }

    jimpilier::ctxt = std::make_unique<llvm::LLVMContext>();
    jimpilier::GlobalVarsAndFunctions = std::make_unique<llvm::Module>("Jimbo jit", *jimpilier::ctxt);
    jimpilier::builder = std::make_unique<llvm::IRBuilder<>>(*jimpilier::ctxt);
    jimpilier::DataLayout = std::make_unique<llvm::DataLayout>(jimpilier::GlobalVarsAndFunctions.get()); 
    jimpilier::currentFunction = NULL; 
    if(argc > 2){
    std::string arg1 = all_args[0]; 
    if(arg1 == "-edu" || arg1 == "-rp"){
    jimpilier::currentFunction = (llvm::Function*) jimpilier::GlobalVarsAndFunctions->getOrInsertFunction("static", llvm::FunctionType::get(llvm::Type::getInt32Ty(*jimpilier::ctxt), {llvm::Type::getInt32Ty(*jimpilier::ctxt), llvm::PointerType::getInt8Ty(*jimpilier::ctxt)->getPointerTo()},false)).getCallee(); 
    jimpilier::STATIC = jimpilier::currentFunction; 
    llvm::BasicBlock *staticentry = llvm::BasicBlock::Create(*jimpilier::ctxt, "entry", jimpilier::currentFunction);
	jimpilier::builder->SetInsertPoint(staticentry);
    all_args.erase(all_args.begin()); 
    }
    }
    time_t now = time(nullptr); 
    Stack<Token> tokens = jimpilier::loadTokens(all_args[0]);
    std::unique_ptr<jimpilier::ExprAST> x; 
    while(!tokens.eof()) {
        x = jimpilier::getValidStmt(tokens); 
        if(jimpilier::errored) break; 
        if(x != NULL)x->codegen(); 
        if(jimpilier::DEBUGGING) std::cout<< endl;  
    }
    if(jimpilier::GlobalVarsAndFunctions->getFunction("main") == NULL && jimpilier::STATIC != NULL)
        jimpilier::STATIC->setName("main"); 
    else if(jimpilier::STATIC != NULL){
        jimpilier::STATIC->eraseFromParent(); 
    }
    time_t end = time(nullptr); 
    
    if(!jimpilier::errored){
        std::cout << "; Code was compiled in approx: "<< (end - now) << " seconds"<<endl; 
        jimpilier::GlobalVarsAndFunctions->dump();
    }
}