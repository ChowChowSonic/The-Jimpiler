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

class ExprAST
	{
		public:
		virtual ~ExprAST(){};
		virtual void* codegen() = 0;
	};

	/// NumberExprAST - Expression class for numeric literals like "1.0".
	class NumberExprAST : public ExprAST
	{
		double Val;

	public:
		NumberExprAST(double Val) : Val(Val) {}
		NumberExprAST(NumberExprAST* a) : Val(a->Val) {}
		~NumberExprAST(){};
		void* codegen(){
			cout << Val; 
			return NULL; 
		}
	};

	class MathExprAST : public ExprAST
	{
		 char op;
		 std::unique_ptr<ExprAST> LHS, RHS;

	public:
		~MathExprAST(){};
		MathExprAST(char Val, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs) 
			: op(Val), LHS(std::move(lhs)), RHS(std::move(rhs)) {}
		MathExprAST(MathExprAST* a): op(a->op), LHS(std::move(a->LHS)), RHS(std::move(a->RHS)) {}
		void* codegen(){
			cout << (char)(op) << "(";
			if(LHS != NULL)
			LHS.get()->codegen(); 
			cout <<", ";
			if(RHS != NULL)
			RHS.get()->codegen(); 
			cout << ")"; 
			//*/
			return NULL; 
		}
	};

std::unique_ptr<ExprAST> term(Stack<Token> &tokens)
{
	Token t = tokens.next(); 
	return std::make_unique<NumberExprAST>(new NumberExprAST(stod(t.lex))); 
}

std::unique_ptr<ExprAST> raisedToExpr(Stack<Token> &tokens){
	std::unique_ptr<ExprAST> LHS = std::move(term(tokens));
	if(tokens.peek() != POWERTO && tokens.peek() != LEFTOVER) return LHS; 
	Token t = tokens.next();
	std::unique_ptr<ExprAST> RHS = std::move(term(tokens));

	while (tokens.peek() == POWERTO || tokens.peek() == LEFTOVER)
	{
		t = tokens.next(); 
		RHS =std::make_unique<MathExprAST>(new MathExprAST(t.lex[0], std::move(RHS), std::move(term(tokens))));
		if (!LHS)
		{
			return NULL;
		}
	}
	return std::make_unique<MathExprAST>(new MathExprAST(t.lex[0], std::move(LHS), std::move(RHS)));
}

std::unique_ptr<ExprAST> multAndDivExpr(Stack<Token> &tokens){
	std::unique_ptr<ExprAST> LHS = std::move(raisedToExpr(tokens));
	if(tokens.peek() != MULT && tokens.peek() != DIV) return LHS; 
	Token t = tokens.next();
	std::unique_ptr<ExprAST> RHS = std::move(raisedToExpr(tokens));

	while (tokens.peek() == MULT || tokens.peek() == DIV)
	{
		t = tokens.next(); 
		RHS =std::make_unique<MathExprAST>(new MathExprAST(t.lex[0], std::move(RHS), std::move(raisedToExpr(tokens))));
		if (!LHS)
		{
			return NULL;
		}
	}
	return std::make_unique<MathExprAST>(new MathExprAST(t.lex[0], std::move(LHS), std::move(RHS)));
}

/**
 * @brief Parses a mathematical expression. mathExpr itself only handles 
 * addition and subtraction, but calls multAndDivExpr & RaisedToExper for any multiplication or division. 
 * A mathExpr is approximately defined as:
 * 
 * expr		-> <TERM> <join> <expr> | <TERM>
 * join		-> "+" | "-" | "*" | "/" | "^"
 * term is denfined in the function of the same name -> "std::unique_ptr<ExprAST> term(Stack<Token> tokens)"
 * @param tokens 
 * @return true - if it is a valid statement,
 * @return NULL otherwise
 */
std::unique_ptr<ExprAST> mathExpr(Stack<Token> &tokens){
	std::unique_ptr<ExprAST> LHS = std::move(multAndDivExpr(tokens));
	if(tokens.peek() != PLUS && tokens.peek() != MINUS) return LHS; 
	Token t = tokens.next();
	std::unique_ptr<ExprAST> RHS = std::move(multAndDivExpr(tokens));

	while (tokens.peek() == PLUS || tokens.peek() == MINUS)
	{
		t = tokens.next(); 
		RHS =std::make_unique<MathExprAST>(new MathExprAST(t.lex[0], std::move(RHS), std::move(multAndDivExpr(tokens))));
		if (!LHS)
		{
			return NULL;
		}
	}
	return std::make_unique<MathExprAST>(new MathExprAST(t.lex[0], std::move(LHS), std::move(RHS)));
}

int main(){
	Stack<Token> tokens; 
	std::vector<Token> toks;
	string s = "5"; 
	toks.push_back(Token(NUMCONST, "1", 1)); 
	toks.push_back(Token(PLUS, "+", 1)); 
	toks.push_back(Token(NUMCONST, "2", 1));  
	toks.push_back(Token(PLUS, "^", 1)); 
	toks.push_back(Token(NUMCONST, "3", 1)); 
	toks.push_back(Token(PLUS, "+", 1)); 
	toks.push_back(Token(NUMCONST, "4", 1)); 
	toks.push_back(Token(POWERTO, "+", 1)); 
	toks.push_back(Token(NUMCONST, "5", 1)); 
	tokens = Stack<Token>(toks);
	mathExpr(tokens)->codegen(); 

	// std::error_code EC; 
	// llvm::raw_fd_ostream OS("module.o", EC); 
	// jimpilier::GlobalVarsAndFunctions->ifunc_empty();  
	// jimpilier::GlobalVarsAndFunctions->end();  
	//jimpilier::builder.CreateAdd(v, v);
	//jimpilier::GlobalVarsAndFunctions->dump();  
	return 0; 
}