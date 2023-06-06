#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/FileCheck/FileCheck.h"
namespace jimpilier
{
	llvm::LLVMContext ctxt;
	llvm::IRBuilder<> builder(ctxt);
	static std::unique_ptr<llvm::Module> GlobalVarsAndFunctions = std::make_unique<llvm::Module>("default", ctxt);
	std::map<string, llvm::Value *> variables;

	/// ExprAST - Base class for all expression nodes.
	class ExprAST
	{
	public:
		virtual ~ExprAST(){};
		virtual llvm::Value *codegen() = 0;
	};

	/// NumberExprAST - Expression class for numeric literals like "1.0".
	class NumberExprAST : public ExprAST
	{
		double Val;

	public:
		NumberExprAST(double Val) : Val(Val) {}
		llvm::Value *codegen()
		{
			cout << Val << endl;
			return NULL;
		}
	};
	/// VariableExprAST - Expression class for referencing a variable, like "a".
	class VariableExprAST : public ExprAST
	{
		string Name;

	public:
		VariableExprAST(const string &Name) : Name(Name) {}
		llvm::Value *codegen()
		{
			return nullptr;
		}
	};

	/// BinaryExprAST - Expression class for a binary operator.
	class BinaryExprAST : public ExprAST
	{
		char Op;
		unique_ptr<ExprAST> LHS, RHS;

	public:
		BinaryExprAST(char op, unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
			: Op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
		llvm::Value *codegen()
		{
			return nullptr;
		}
	};

	/// CallExprAST - Expression class for function calls.
	class CallExprAST : public ExprAST
	{
		string Callee;
		vector<std::unique_ptr<ExprAST>> Args;

	public:
		CallExprAST(const string &Callee, vector<std::unique_ptr<ExprAST>> Args)
			: Callee(Callee), Args(std::move(Args)) {}
		llvm::Value *codegen()
		{
			return nullptr;
		}
	};

/**
 * Represents a list of items in code, usually represented by a string such as : [1, 2, 3, 4, 5]
*/
	class ListExprAST : public ExprAST
	{
		std::vector<std::unique_ptr<ExprAST>> Contents;
		ListExprAST(std::vector<std::unique_ptr<ExprAST>> Args)
			: Contents(std::move(Args)) {}

		llvm::Value *ListExprAST::codegen()
		{
			llvm::Value *ret = NULL;
			for (auto i = Contents.begin(); i < Contents.end(); i++)
			{
				ret = i->get()->codegen();
			};
			return ret;
		}
	};

	/// PrototypeAST - This class represents the "prototype" for a function,
	/// which captures its name, and its argument names (thus implicitly the number
	/// of arguments the function takes).
	class PrototypeAST 
	{
		string Name;
		vector<std::string> Args;

	public:
		PrototypeAST(const std::string &name, std::vector<std::string> Args)
			: Name(name), Args(std::move(Args)) {}

		const std::string &getName() const { return Name; }
		llvm::Value *codegen()
		{
			return nullptr;
		}
	};

	/// FunctionAST - This class represents a function definition itself.
	class FunctionAST
	{
		std::unique_ptr<PrototypeAST> Proto;
		std::unique_ptr<ExprAST> Body;

	public:
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
					std::unique_ptr<ExprAST> Body)
			: Proto(std::move(Proto)), Body(std::move(Body)) {}
		llvm::Value *codegen()
		{
			return nullptr;
		}
	};

	// class LoopAST : public ExprAST {

	// public:

	// 	llvm::Value* codegen(){

	// 	}
	// };

	// <-- BEGINNING OF AST GENERATING FUNCTIONS -->

	std::unique_ptr<ExprAST> analyzeFile(string fileDir);

	std::unique_ptr<ExprAST> import(Stack<Token> &tokens)
	{
		Token t;
		std::unique_ptr<ExprAST> b;
		while ((t = tokens.next()).token != SEMICOL && (t.token == IDENT || t.token == COMMA) && b)
		{
			if (t == IDENT)
			{
				/*string s = t.lex + ".jmb";
				if(find(importedFiles.begin(), importedFiles.end(), s) == importedFiles.end()){
					importedFiles.push_back(s);
					b = analyzeFile(s);
				}*/
			}
		}
		if (t.token != SEMICOL || !b)
		{
			tokens.go_back(2);
			return NULL;
		}
		return b;
	}

	std::unique_ptr<ExprAST> mathExpr(Stack<Token> &tokens);
	std::unique_ptr<ExprAST> logicStmt(Stack<Token> &tokens);

	std::unique_ptr<ExprAST> greaterLessStmt(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS = std::move(mathExpr(tokens));
		if (tokens.peek() != GREATER || tokens.peek() != LESS)
			return LHS;
		Token t = tokens.next();
		std::unique_ptr<ExprAST> RHS = std::move(mathExpr(tokens));
		while (tokens.peek() == GREATER || tokens.peek() == LESS)
		{
			tokens.next();
			RHS = std::make_unique<BinaryExprAST>(new BinaryExprAST(t.lex[0], std::move(RHS), std::move(mathExpr(tokens))));
		}
		return std::make_unique<BinaryExprAST>(new BinaryExprAST(t.lex[0], std::move(LHS), std::move(RHS)));
	}

	/**
	 * @brief checks for a basic <IDENT> (["=="|"!="] <IDENT>)* However,
	 * I am torn between enabling N-way comparison (I.E.: "x == y == z == a == b").
	 * No other programming languages that I know of include this, and I have a
	 * feeling it's for good reason... But at the same time, I want to add it as I
	 * feel that is extremely convienent in the situations it comes up in.
	 *
	 * @param tokens
	 * @return true - if it is a valid statement,
	 * @return NULL otherwise
	 */
	std::unique_ptr<ExprAST> compareStmt(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS = std::move(greaterLessStmt(tokens));
		if (tokens.peek() != EQUALCMP || tokens.peek() != NOTEQUAL)
			return LHS;
		Token t = tokens.next();
		std::unique_ptr<ExprAST> RHS = std::move(greaterLessStmt(tokens));
		while (tokens.peek() == EQUALCMP || tokens.peek() == NOTEQUAL)
		{
			tokens.next();
			RHS = std::make_unique<BinaryExprAST>(new BinaryExprAST(t.lex[0], std::move(RHS), std::move(greaterLessStmt(tokens))));
		}
		return std::make_unique<BinaryExprAST>(new BinaryExprAST(t.lex[0], std::move(LHS), std::move(RHS)));
	}

	std::unique_ptr<ExprAST> andStmt(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS = std::move(compareStmt(tokens));
		if (tokens.peek() != AND)
			return LHS;
		Token t = tokens.next();
		std::unique_ptr<ExprAST> RHS = std::move(compareStmt(tokens));
		while (tokens.peek() == AND)
		{
			tokens.next();
			RHS = std::make_unique<BinaryExprAST>(new BinaryExprAST(t.lex[0], std::move(RHS), std::move(compareStmt(tokens))));
		}
		return std::make_unique<BinaryExprAST>(new BinaryExprAST(t.lex[0], std::move(LHS), std::move(RHS)));
	}

	/**
	 * @brief Parses a logic statement (I.E. "x == 5") consisting of only idents and string/number constants.
	 * LogicStmt itself only parses OR Statements (x || y), but makes calls to functions like andStmt, compareStmt, etc. to do the rest.
	 * EBNF isn't technically 100% accurate, but that's because a few assumptions should be made going into this, like,
	 * for example, all open parenthesis should have a matching closing parenthesis.
	 * logicStmt 	-> <Stmt> <join> <logicStmt> | <Stmt>
	 * Stmt			-> "("?<helper>")"? <Join> "("?<helper>")"?
	 * helper		-> <base> | <logicStmt>
	 * base			-> <terminal> <op> <terminal> | "true" | "NULL";
	 * op			-> "==" | ">=" | "<=" | ">" | "<"
	 * join			-> "and" | "or"
	 * terminal		-> SCONST | NUMCONST | VARIABLE //Literal string number or variable values
	 *
	 * TODO: Plan out operator precidence so that you can write code without/minimally hitting shift
	 *
	 * @param tokens
	 * @return true - if it is a valid logic statement,
	 * @return NULL otherwise
	 */
	std::unique_ptr<ExprAST> logicStmt(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS = std::move(andStmt(tokens));
		if (tokens.peek() != OR)
			return LHS;
		Token t = tokens.next();
		std::unique_ptr<ExprAST> RHS = std::move(andStmt(tokens));
		while (tokens.peek() == OR)
		{
			tokens.next();
			RHS = std::make_unique<BinaryExprAST>(new BinaryExprAST(t.lex[0], std::move(RHS), std::move(andStmt(tokens))));
		}
		return std::make_unique<BinaryExprAST>(new BinaryExprAST(t.lex[0], std::move(LHS), std::move(RHS)));
	}

	/**
	 * @brief using EBNF notation, a term is roughly defined as:
	 *   <term> = (["+"|"-"]["++"|"--"] | "->" | "@")<IDENT>["++"|"--"] | "true" | "NULL" | "(" <expr> ")";
	 * However the ending [++|--] are only allowed if there is no (++|--) at the beginning of the term.
	 * TODO: Holy Fuckles It's Knuckles I need to revamp this function entirely.
	 *
	 * @param tokens
	 * @return true if the syntax is valid
	 * @return NULL if syntax is not valid
	 */
	std::unique_ptr<ExprAST> term(Stack<Token> &tokens)
	{
		// int y=0,x = ++y++;  //Testing C++ syntax and what's valid
		std::unique_ptr<ExprAST> LHS;
		if (tokens.peek() == LPAREN)
		{
			tokens.next();
			LHS = std::move(logicStmt(tokens));
			if (tokens.peek() != RPAREN || !LHS)
			{
				if (LHS)
					cout << "Expected a closing parenthesis" << endl;
				return NULL;
			}
			tokens.next();
		}
		Token t = tokens.next();
		if (t == TRU || t == FALS)
			return std::make_unique<NumberExprAST>(new NumberExprAST(1));

		if (t == IDENT && (tokens.peek() == INCREMENT || tokens.peek() == DECREMENT))
		{
			tokens.next();
			LHS = std::make_unique<VariableExprAST>(new VariableExprAST(t.lex));
			return LHS;
		}
		else if (t == IDENT)
		{
			return std::make_unique<VariableExprAST>(new VariableExprAST(t.lex));
		}

		return LHS;
	}

	std::unique_ptr<ExprAST> raisedToExpr(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS = std::move(term(tokens));
		if (tokens.peek() != POWERTO && tokens.peek() != LEFTOVER)
			return LHS;
		Token t = tokens.next();
		std::unique_ptr<ExprAST> RHS = std::move(term(tokens));

		while (tokens.peek() == POWERTO || tokens.peek() == LEFTOVER)
		{
			t = tokens.next();
			RHS = std::make_unique<BinaryExprAST>(new BinaryExprAST(t.lex[0], std::move(RHS), std::move(term(tokens))));
			if (!LHS)
			{
				return NULL;
			}
		}
		return std::make_unique<BinaryExprAST>(new BinaryExprAST(t.lex[0], std::move(LHS), std::move(RHS)));
	}

	std::unique_ptr<ExprAST> multAndDivExpr(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS = std::move(raisedToExpr(tokens));
		if (tokens.peek() != MULT && tokens.peek() != DIV)
			return LHS;
		Token t = tokens.next();
		std::unique_ptr<ExprAST> RHS = std::move(raisedToExpr(tokens));

		while (tokens.peek() == MULT || tokens.peek() == DIV)
		{
			t = tokens.next();
			RHS = std::make_unique<BinaryExprAST>(new BinaryExprAST(t.lex[0], std::move(RHS), std::move(raisedToExpr(tokens))));
			if (!LHS)
			{
				return NULL;
			}
		}
		return std::make_unique<BinaryExprAST>(new BinaryExprAST(t.lex[0], std::move(LHS), std::move(RHS)));
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
	std::unique_ptr<ExprAST> mathExpr(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS = std::move(multAndDivExpr(tokens));
		if (tokens.peek() != PLUS && tokens.peek() != MINUS)
			return LHS;
		Token t = tokens.next();
		std::unique_ptr<ExprAST> RHS = std::move(multAndDivExpr(tokens));

		while (tokens.peek() == PLUS || tokens.peek() == MINUS)
		{
			t = tokens.next();
			RHS = std::make_unique<BinaryExprAST>(t.lex[0], std::move(RHS), std::move(multAndDivExpr(tokens)));
			if (!LHS)
			{
				return NULL;
			}
		}
		return std::make_unique<BinaryExprAST>(t.lex[0], std::move(LHS), std::move(RHS));
	}

	/**
	 * @brief Called whenever a primitive type or object type keyword is seen.
	 *
	 * @param tokens
	 * @return true
	 * @return NULL
	 */
	std::unique_ptr<ExprAST> declare(Stack<Token> &tokens)
	{
		tokens.go_back(1);
		Token t = tokens.next();
		KeyToken type = t.token; // int, char, std::unique_ptr<ExprAST>, etc.
		if (tokens.peek() == POINTER)
			t = tokens.next();
		t = tokens.next();
		string id = t.lex; // the variable name
		if (t.token != IDENT)
		{
			cout << "Error: Not an identifier: " << t.lex << endl;
			tokens.go_back(2);
			return NULL;
		}
		if (tokens.peek() == SEMICOL)
		{
			return std::make_unique<VariableExprAST>(new VariableExprAST(id));
		}
		else if (tokens.next() != EQUALS)
		{
			return NULL;
		}
		std::unique_ptr<ExprAST> b = NULL;
		if (tokens.peek().lex != "[")
			b = logicStmt(tokens);
		else
			b = listRepresentation(tokens); // Ok now I have to figure out what to do with b...
		// tokens.go_back();
		// cout <<tokens.peek();
		if (!b)
		{
			// tokens.go_back();
			// cout << tokens.peek();
			if (tokens.next().lex == id)
				cout << "Cannot use variable in its own declaration statement" << endl;
			return NULL;
		}
		// cout << t.lex <<endl ;
		t = tokens.next();
		// variables[t] = type;
		// put a way to interpret statements here
		if (t == SEMICOL)
		{
			variables.insert({id, std::move(b)->codegen()}); // TODO: Double check that this is correct
			return std::make_unique<VariableExprAST>(id);
		}
		tokens.go_back(2);
		return NULL;
	}

	std::unique_ptr<ExprAST> obj(Stack<Token> &tokens)
	{
		if (tokens.next().token != IDENT)
		{
			return NULL;
		}
		return NULL; // TODO: Fix this
	}
	/**
	 * Note: This function does not yet parse the square brackets, only the innter sections.
	 * A list representation can be displayed in EBNF as:
	 *  <OBRACKET> 	-> [ //Literal character
	 *  <CBRACKET> 	-> ] //Literal character
	 * 	<REGLIST>	-> <BoolStmt>{, <BoolStmt>} //Regular list: [1, 2, 3, 4, 5]
	 * 	<ITERATOR>	-> <VAR> <FOREACHSTMT> //stolen directly from python syntax: [x+1 for x in list]
	 *  <ListRep>	-> <OBRACKET> (<REGLIST>|<ITERATOR>) <CBRACKET>
	 */
	std::unique_ptr<ExprAST> listRepresentation(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS = std::move(logicStmt(tokens));
		std::vector<std::unique_ptr<ExprAST>> contents;
		while (tokens.peek() == COMMA)
		{
			tokens.next();
			contents.push_back(std::move(LHS));
			LHS = std::move(logicStmt(tokens));
		}
		return std::make_unique<ListExprAST>(std::move(contents));
	}

	std::unique_ptr<FunctionAST> func(Stack<Token> &tokens)
	{
		tokens.go_back();
		Token t;
		if ((t = tokens.next()) != IDENT)
			return NULL;
		std::string name = t.lex;
		t = tokens.next();
		if (t != RPAREN)
			return NULL;
		std::vector<std::string> argnames;
		while (tokens.peek().token >= INT)
		{
			tokens.next();
			argnames.push_back(tokens.next().lex);
			tokens.next();
		}
		if (tokens.peek() != LPAREN) return NULL;
		std::unique_ptr<PrototypeAST> proto = std::make_unique<PrototypeAST>(name, std::move(argnames));
		if(auto E = getValidStmt(tokens))
			return std::make_unique<FunctionAST>(std::move(proto), std::move(E)); 
	}

	/**
	 * @brief Called whenever the words "public", "private", "protected", or a class of object/primitive ("int", "string", "object") are seen.
	 * Essentially we don't know from here if what's being defined is a function or a variable, so we just try to account for anything that comes up.
	 *
	 * @param tokens
	 * @return true if a valid function
	 */
	std::unique_ptr<ExprAST> declareOrFunction(Stack<Token> &tokens)
	{
		Token t = tokens.next();
		if (tokens.peek() == POINTER)
			t = tokens.next();
		if (t.token >= CONST && t.token <= PROTECTED)
		{
			// Operators.push_back(t) //Add this to the variable modifier memory
			return declareOrFunction(tokens);
		} //*/
		if (t != IDENT && t.token < INT)
		{
			tokens.go_back(1);
			return NULL;
		}
		if (t != IDENT)
		{
			t = tokens.next();
		}
		if (t.token == IDENT)
		{
			t = tokens.peek();
			if (t.token == LPAREN)
			{ // It's a function
				return func(tokens);
				// return function(tokens); //TBI
			}
			else if (t == EQUALS || t == SEMICOL)
			{ // It's a variable
				tokens.go_back(2);
				return declare(tokens);
			}
		}
		tokens.go_back(2);
		return NULL;
	}

	std::unique_ptr<ExprAST> construct(Stack<Token> &tokens)
	{
		Token t = tokens.next();
		if (t != LPAREN)
		{
			tokens.go_back(1);
			return NULL;
		}
		t = tokens.next();
		if (t != RPAREN)
		{
			tokens.go_back(1);
			return NULL;
		}
		return NULL; // TODO: Fix this
		// Need to handle closing curly bracket
	}

	std::unique_ptr<ExprAST> destruct(Stack<Token> &tokens)
	{
		Token t = tokens.next();
		if (t != LPAREN)
		{
			tokens.go_back(1);
			return NULL;
		}
		t = tokens.next();
		if (t != RPAREN)
		{
			tokens.go_back(1);
			return NULL;
		}
		return NULL; // TODO: Fix this
		// Need to handle closing curly bracket
	}
	/**
	 * @brief Described in short using EBNF, generic statements consist of: \n
	 * generic	-> <inc/dec> | <assign>
	 * inc/dec	-> <variable>["++"|"--"]";" | [++|--]<variable>";"
	 * assign	-> <variable> "=" <someValue>;
	 * @param tokens
	 * @return true
	 * @return NULL
	 */
	std::unique_ptr<ExprAST> genericStmt(Stack<Token> &tokens)
	{
		VariableExprAST *retval = NULL;
		Token t = tokens.next();
		if (t == INCREMENT || t == DECREMENT)
		{
			Token t2 = tokens.next();
			retval = new VariableExprAST(t2.lex);
			return std::make_unique<ExprAST>(retval);
		}
		t = tokens.next();
		retval = new VariableExprAST(t.lex);
		if (t == INCREMENT || t == DECREMENT)
			return std::make_unique<ExprAST>(retval);
		if (t == EQUALS)
		{
			return mathExpr(tokens);
		}
		return std::make_unique<ExprAST>(retval);
	}

	std::unique_ptr<ExprAST> forStmt(Stack<Token> &tokens)
	{
		Token t = tokens.next();

		std::unique_ptr<ExprAST> b = declare(tokens);

		b = logicStmt(tokens);
		if (tokens.next() != SEMICOL)
		{
			tokens.go_back(1);
			return NULL;
		}
		b = mathExpr(tokens); // TBI: variable checking
		if (tokens.peek() == SEMICOL)
			tokens.next();
		return b;
	}

	std::unique_ptr<ExprAST> ifStmt(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> b = logicStmt(tokens);
		if (tokens.next() != OPENCURL)
			return NULL;
		return b;
	}
	std::unique_ptr<ExprAST> caseStmt(Stack<Token> &tokens)
	{
		Token variable = tokens.next();
		if (variable.token != SCONST && variable.token != NUMCONST && variable.token != TRU && variable.token != FALS)
			return NULL;
		return NULL; // Leaving this for now because I have to figure the specifics out later...
	}

	std::unique_ptr<ExprAST> caseSwitchStmt(Stack<Token> &tokens)
	{
		Token variable = tokens.next();
		if (variable != IDENT)
		{
			cout << "Unexpected token: '" << variable.lex << "' - expected a variable." << endl;
			tokens.go_back();
			return NULL;
		}
		return NULL; // TODO: Fix this
	}

	/**
	 * @brief Get the next Valid Stmt in the code file provided.
	 *
	 * @param tokens
	 * @return true if the statement is valid
	 * @return NULL if it is not a vald statement
	 */
	std::unique_ptr<ExprAST> getValidStmt(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> status = NULL;
		Token t = tokens.next();
		switch (t.token)
		{
		case IMPORT:
			return jimpilier::import(tokens);
		case FOR:
			return jimpilier::forStmt(tokens);
		case IF:
			return jimpilier::ifStmt(tokens);
		case SWITCH:
			return jimpilier::caseSwitchStmt(tokens);
		case CASE:
			return jimpilier::caseStmt(tokens);
		case OBJECT:
			return obj(tokens);
		case CONSTRUCTOR:
			return jimpilier::construct(tokens);
		case DESTRUCTOR:
			return jimpilier::destruct(tokens);
		case IDENT:
		case RPAREN:
		case LPAREN:
		case INCREMENT:
		case DECREMENT:
			tokens.go_back(1);
			return jimpilier::genericStmt(tokens);
		case SEMICOL:
			return NULL;
		case OPENCURL:
		{
			return NULL;
		}
		case CLOSECURL:
			// cout << "Close curl called" << endl;
			return NULL;
		case RET:
			return NULL;
		case INT:
		case SHORT: // int i = 0; << We start at the int token, then have to move to the ident, so we go back 1 to undo that
		case POINTER:
		case LONG:
		case FLOAT:
		case DOUBLE:
		case STRING:
		case CHAR:
		case BOOL:
		case BYTE:
			tokens.go_back(1);
		case PUBLIC:
		case PRIVATE:
		case PROTECTED:
		case SINGULAR:
		case CONST:
			return declareOrFunction(tokens);
		default:
		{
			cout << "ERROR: Unknown token: \n"
				 << t << endl;
			tokens.go_back(1);
			return NULL;
		}
		}
		cout << "Returning NULL by error?" << endl;
		return NULL;
	}
	Stack<Token> loadTokens(string fileDir)
	{
		vector<Token> tokens;

		ifstream file;
		file.open(fileDir);
		if (!file)
		{
			cout << "File '" << fileDir << "' does not exist" << endl;
			Stack<Token> s;
			return s;
		}
		int ln = 1;
		while (!file.eof())
		{
			Token t = getNextToken(file, ln);
			t.ln = ln;
			if (t != ERR)
				tokens.push_back(t);
		}
		Stack<Token> s(tokens);
		return s;
	}

	std::unique_ptr<ExprAST> analyzeFile(string fileDir)
	{
		std::unique_ptr<ExprAST> b;
		Stack<Token> s = loadTokens(fileDir);
		while (!s.eof() && (b = getValidStmt(s)))
		{
			// cout<< s.eof() <<endl;
		}
		if (!b)
		{
			Token err = s.peek(); // cout << openbrackets <<endl ;
			cout << "Syntax error located at token '" << err.lex << "' on line " << err.ln << " in file: " << fileDir << "\n";
			Token e2;
			while (err.ln != -1 && (e2 = s.next()).ln == err.ln && !s.eof())
			{
				cout << e2.lex << " ";
			}
			cout << endl;
			return NULL;
		}
		return NULL; // TODO: Fix this
	}
};