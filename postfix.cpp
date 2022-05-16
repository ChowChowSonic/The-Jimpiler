#include <iostream>
#include <string>
#include <stack>

using namespace std;
int getPrec(char c){
    switch(c){
        case '(':
        return 2; 
        case '*':
        case '/':
        return 2; 
        case '+':
        case '-':
        return 1; 
        default: 
        return 0; 
    }
}

string infToPostfix(string instr){
    string post; 
    stack<char> stk; 
    stack<char> stk2; 
    for(char c : instr){
        if(isspace(c)) continue; 
        if(isalpha(c)){
            post +=c; 
            continue; 
        }
        if( (stk.empty() || getPrec(c) > getPrec(stk.top())) ){
            stk.push(c); 
        }else if(c == ')'){
            while(stk.top() != '('){
                post += stk.top(); 
                stk.pop(); 
            }
        }else{
            while(!stk.empty() && post.size() > 1 &&getPrec(c) <= getPrec(stk.top()) && stk.top() != '(' && stk.top() != ')' ){
                post += stk.top(); 
                stk.pop(); 
            }
            stk.push(c); 
        }
    }
    while(!stk.empty()){
        if(stk.top() != ')' && stk.top() != '(')
            post+=stk.top(); 
            stk.pop(); 
    }
    return post; 
};

int main ( ) {
	string instr;
	cout<<"Please enter an infix notation expression using single lowercase characters:" << endl;
	getline(cin, instr);
	string s = infToPostfix(instr);
    for(char c : s)
	cout << c << " " ;
    cout << endl;
	return 0;	
}