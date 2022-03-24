//cd "c:\\Users\\joeya\\Documents\\GitHub\\CompilierAttempt\\" && g++ main.cpp -o tokenizer && "c:\\Users\\joeya\\Documents\\GitHub\\CompilierAttempt\\"tokenizer
#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <stdio.h>
#include "Tokenizer.cpp"
#include "Stack.cpp"
#include "treeifyer.cpp"
using namespace std; 
int main(int argc, char**args){
    vector<Token> tokens;
    ifstream file("test.txt"); 
    int ln = 0;
    while(!file.eof()){
        tokens.push_back(getNextToken(file, ln));
    }
    Stack<Token> s(tokens);
    for(int i = 0; i < 10; i ++){
    cout << getValidStmt(s); 
    }
    //while(!s.eof()){
    //    cout << s.next().lex << endl; 
    //} 
    //for(Token t : tokens){
    //    cout << std::setw(15) << keytokens[t.token]  << std::setw(15) <<"'" << t.lex<<"'" <<endl; 
    //}
}