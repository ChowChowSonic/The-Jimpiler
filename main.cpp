//cd "c:\\Users\\joeya\\Documents\\GitHub\\CompilierAttempt\\" && g++ main.cpp -o tokenizer && "c:\\Users\\joeya\\Documents\\GitHub\\CompilierAttempt\\"tokenizer
#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <stdio.h>
#include <map>
#include <string>
#include "tokenizer.cpp"
#include "Stack.cpp"
#include "treeifyer.cpp"
using namespace std; 
int main(int argc, char**args){
    //time_t now = time(nullptr);
    vector<Token> tokens;
    ifstream file("test.txt"); 
    int ln = 1;
    while(!file.eof()){
        Token t = getNextToken(file, ln); 
        t.ln = ln; 
        tokens.push_back(t);
    }

    Stack<Token> s(tokens);
    //while(!s.eof()) cout << s.next().ln<<endl;
    bool b;
    while((b = getValidStmt(s))){
    }
    if(!b){
        Token err = s.peek(); 
        cout <<"Syntax error located at token '"<< err.lex <<"' on line "<<err.ln<<":\n";
        Token e2; 
        while((e2 = s.next()).ln == err.ln && !s.eof()){
            cout <<e2.lex << " "; 
        }
    }
    //time_t end = time(nullptr); 
    //cout << endl <<"compiled in approx: "<< (end - now) << " seconds"; 
    //while(!s.eof()){
    //    cout << s.next().lex << endl; 
    //} 
    //for(Token t : tokens){
    //    cout << std::setw(15) << keytokens[t.token]  << std::setw(15) <<"'" << t.lex<<"'" <<endl; 
    //} 
}