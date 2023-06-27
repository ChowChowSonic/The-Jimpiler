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
	std::map<std::string, llvm::Value *> variables;
	std::vector<std::string> importedFiles;

	/// ExprAST - Base class for all expression nodes.
	class ExprAST
	{
	public: // TODO: Implement types here
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
			std::cout << Val;
			return llvm::ConstantFP::get(ctxt, llvm::APFloat(Val));
		}
	};
	class StringExprAST : public ExprAST
	{
		std::string Val;

	public:
		StringExprAST(std::string val) : Val(val) {}
		llvm::Value *codegen()
		{
			std::cout << Val;
			return NULL;//llvm::ConstantFP::get(ctxt, llvm::AP(Val));
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
			std::cout << Name; 
			llvm::Value *V = variables[Name];
			if (!V)
			{
				std::cout << "Unknown variable name: " << Name << endl;
				return nullptr;
			}
			return V;
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
			std::cout << Op << "( ";
			llvm::Value *L = LHS->codegen();
			std::cout << ", ";
			llvm::Value *R = RHS->codegen();
			std::cout << " )";
			switch (Op)
			{
			case '+':
				return builder.CreateFAdd(L, R, "addtmp");
			case '-':
				return builder.CreateFSub(L, R, "subtmp");
			case '*':
				return builder.CreateFMul(L, R, "multmp");
			case '<':
				L = builder.CreateFCmpULT(L, R, "cmptmp");
				// Convert bool 0/1 to double 0.0 or 1.0
				return builder.CreateUIToFP(L, llvm::Type::getDoubleTy(ctxt),
											"booltmp");
			default:
				std::cout << "Error: Unknown operator" << Op; 
				return NULL;
			}
		}
	};
	/// CallExprAST - Expression class for function calls.
	class CallExprAST : public ExprAST
	{
		string Callee;
		vector<std::unique_ptr<ExprAST>> Args;

	public:
		CallExprAST(const string callee, vector<std::unique_ptr<ExprAST>> Arg)
		{
			for (auto &x : Arg)
			{
				Args.push_back(std::move(x));
			}
		}
		// : Callee(callee), Args(Arg) {}
		llvm::Value *codegen()
		{
			// Look up the name in the global module table.
			llvm::Function *CalleeF = GlobalVarsAndFunctions->getFunction(Callee);
			if (!CalleeF)
			{
				std::cout << "Unknown function referenced:" << Callee << endl;
				return NULL;
			}
			// If argument mismatch error.
			if (CalleeF->arg_size() != Args.size())
			{
				std::cout << "Incorrect Argument count from function: " << Callee << endl;
				return NULL;
			}

			std::vector<llvm::Value *> ArgsV;
			for (unsigned i = 0, e = Args.size(); i != e; ++i)
			{
				ArgsV.push_back(Args[i]->codegen());
				if (!ArgsV.back())
					{
						std::cout << "Error saving function args"; 
						return nullptr;
					}
			}

			return builder.CreateCall(CalleeF, ArgsV, "calltmp");
		}
	};

	/**
	 * Represents a list of items in code, usually represented by a string such as : [1, 2, 3, 4, 5]
	 */
	class ListExprAST : public ExprAST
	{
	public:
		std::vector<std::unique_ptr<ExprAST>> Contents;
		ListExprAST(std::vector<std::unique_ptr<ExprAST>> &Args)
		{
			for (auto &x : Args)
			{
				Contents.push_back(std::move(x));
			}
		}
		// : Contents(std::move(Args)) {}
		ListExprAST() {}
		llvm::Value *codegen()
		{
			llvm::Value *ret = NULL;
			std::cout << "[ ";
			for (auto i = Contents.begin(); i < Contents.end(); i++)
			{
				ret = i->get()->codegen();
				if(i != Contents.end()-1)std::cout << ", ";
			};
			std::cout << " ]";
			return ret;
		}
	};

	class CodeBlockAST : public ExprAST
	{
	public:
		std::vector<std::unique_ptr<ExprAST>> Contents;
		CodeBlockAST(std::vector<std::unique_ptr<ExprAST>> &Args)
		{
			for (auto &x : Args)
			{
				Contents.push_back(std::move(x));
			}
		}
		// : Contents(std::move(Args)) {}
		CodeBlockAST() {}
		llvm::Value *codegen()
		{
			std::cout << '{' << endl;
			llvm::Value *ret = NULL;
			for (int i = 0; i < Contents.size(); i++)
			{
				ret = Contents[i]->codegen();
				std::cout << endl;
			};
			std::cout << '}' << endl;
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
			: Name(name)
		{
			for (auto &x : Args)
			{
				this->Args.push_back(std::move(x));
			}
		}

		const std::string &getName() const { return Name; }
		llvm::Function *codegen()
		{
			std::vector<llvm::Type *> Doubles(Args.size(),
											  llvm::Type::getDoubleTy(ctxt));
			llvm::FunctionType *FT =
				llvm::FunctionType::get(llvm::Type::getDoubleTy(ctxt), Doubles, false);

			llvm::Function *F =
				llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name, GlobalVarsAndFunctions.get());
			unsigned Idx = 0;
			for (auto &Arg : F->args())
				Arg.setName(Args[Idx++]);

			return F;
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
			std::cout << Proto->getName(); 
			llvm::Function *TheFunction = GlobalVarsAndFunctions->getFunction(Proto->getName());

			if (!TheFunction)
				TheFunction = Proto->codegen();

			if (!TheFunction){
				return nullptr;
			}

			if (!TheFunction->empty())
			{
				std::cout << "Function Cannot be redefined" << endl;
				return (llvm::Function *)NULL;
			}

			llvm::BasicBlock *BB = llvm::BasicBlock::Create(ctxt, "entry", TheFunction);
			builder.SetInsertPoint(BB);

			// Record the function arguments in the NamedValues map.
			variables.clear();
			for (auto &Arg : TheFunction->args())
				variables[std::string(Arg.getName())] = &Arg;

			if (llvm::Value *RetVal = Body->codegen())
			{
				// Finish off the function.
				builder.CreateRet(RetVal);

				// Validate the generated code, checking for consistency.
				verifyFunction(*TheFunction);
				std::cout << "//end of " << Proto->getName() <<endl; 
				return TheFunction;
			}else std::cout << "Error in body of function"<< endl; 
			TheFunction->eraseFromParent();
			return nullptr;
		}
	};

	void logError(string s, Token t)
	{
		std::cout << s << ' ' << t.toString() << endl;
	}

	// <-- BEGINNING OF AST GENERATING FUNCTIONS -->

	std::unique_ptr<ExprAST> analyzeFile(string fileDir);
	std::unique_ptr<ExprAST> getValidStmt(Stack<Token> &tokens);

	std::unique_ptr<ExprAST> import(Stack<Token> &tokens)
	{
		if (tokens.next() != IMPORT)
			return NULL;
		Token t;
		std::unique_ptr<ExprAST> b = std::make_unique<NumberExprAST>(1);
		while ((t = tokens.next()).token != SEMICOL && (t.token == IDENT || t.token == COMMA) && b)
		{
			if (t == IDENT)
			{
				string s = t.lex + ".jmb";
				if (find(importedFiles.begin(), importedFiles.end(), s) == importedFiles.end())
				{
					importedFiles.push_back(s);
					b = analyzeFile(s);
				} //*/
			}
		}
		if (t.token != SEMICOL)
		{
			tokens.go_back(2);
			logError("Invalid token when importing:", t);
			return NULL;
		}
		// if(!b){
		// 	logError("Error in dependency designated by the following import:", t);
		// 	return NULL;
		// }
		return b;
	}

	std::unique_ptr<ExprAST> logicStmt(Stack<Token> &tokens);
	std::unique_ptr<ExprAST> mathExpr(Stack<Token> &tokens);
	std::unique_ptr<ExprAST> listExpr(Stack<Token> &tokens);

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
		std::unique_ptr<ExprAST> LHS;
		if (tokens.peek() == LPAREN)
		{
			tokens.next();
			LHS = std::move(listExpr(tokens));
			if (tokens.peek() == RPAREN)
			{
				tokens.next();
				return LHS;
			}
		}

		if (tokens.peek() == SCONST )
		{
			Token s = tokens.next();
			return std::make_unique<StringExprAST>(s.lex);
		}else if(tokens.peek() == IDENT){
			Token s = tokens.next();
			return std::make_unique<VariableExprAST>(s.lex);
		}
		else if (tokens.peek() == NUMCONST)
		{
			return std::make_unique<NumberExprAST>(stoi(tokens.next().lex));
		}

		logError("Invalid term:", tokens.currentToken());
		return NULL;
	}
	std::unique_ptr<ExprAST> incDecExpr(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS;
		if (tokens.peek() == INCREMENT || tokens.peek() == DECREMENT)
		{
			Token t = tokens.next();
			LHS = std::move(term(tokens));
			return std::make_unique<BinaryExprAST>(t.lex[0], std::move(LHS), std::make_unique<NumberExprAST>(1));
		}
		LHS = std::move(term(tokens));
		if (LHS == NULL)
		{
			return NULL;
		}
		if (tokens.peek() == INCREMENT || tokens.peek() == DECREMENT)
		{
			Token t = tokens.next();
			return std::make_unique<BinaryExprAST>(t.lex[0], std::move(LHS), std::make_unique<NumberExprAST>(1));
		}
		return LHS;
	}

	std::unique_ptr<ExprAST> raisedToExpr(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS = std::move(incDecExpr(tokens));
		if (tokens.peek() != POWERTO && tokens.peek() != LEFTOVER)
			return LHS;
		Token t = tokens.next();
		std::unique_ptr<ExprAST> RHS = std::move(incDecExpr(tokens));

		while (tokens.peek() == POWERTO || tokens.peek() == LEFTOVER)
		{
			t = tokens.next();
			RHS = std::make_unique<BinaryExprAST>(t.lex[0], std::move(RHS), std::move(term(tokens)));
			if (!LHS)
			{
				logError("Error parsing a term in raisedToExpr ", t);
				return NULL;
			}
		}
		return std::make_unique<BinaryExprAST>(t.lex[0], std::move(LHS), std::move(RHS));
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
			RHS = std::make_unique<BinaryExprAST>(t.lex[0], std::move(RHS), std::move(raisedToExpr(tokens)));
			if (!LHS)
			{
				logError("Error parsing a term in multDivExpr ", t);
				return NULL;
			}
		}
		return std::make_unique<BinaryExprAST>(t.lex[0], std::move(LHS), std::move(RHS));
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
				logError("Error parsing a term in mathExpr", t);
				return NULL;
			}
		}
		return std::make_unique<BinaryExprAST>(t.lex[0], std::move(LHS), std::move(RHS));
	}

	std::unique_ptr<ExprAST> greaterLessStmt(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS = std::move(mathExpr(tokens));
		if (tokens.peek() != GREATER && tokens.peek() != LESS)
			return LHS;
		Token t = tokens.next();
		std::unique_ptr<ExprAST> RHS = std::move(mathExpr(tokens));
		while (tokens.peek() == GREATER || tokens.peek() == LESS)
		{
			tokens.next();
			RHS = std::make_unique<BinaryExprAST>(t.lex[0], std::move(RHS), std::move(mathExpr(tokens)));
		}
		return std::make_unique<BinaryExprAST>(t.lex[0], std::move(LHS), std::move(RHS));
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
		if (tokens.peek() != EQUALCMP && tokens.peek() != NOTEQUAL)
			return LHS;
		Token t = tokens.next();
		std::unique_ptr<ExprAST> RHS = std::move(greaterLessStmt(tokens));
		while (tokens.peek() == EQUALCMP || tokens.peek() == NOTEQUAL)
		{
			tokens.next();
			RHS = std::make_unique<BinaryExprAST>(t.lex[0], std::move(RHS), std::move(greaterLessStmt(tokens)));
		}
		return std::make_unique<BinaryExprAST>(t.lex[0], std::move(LHS), std::move(RHS));
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
			RHS = std::make_unique<BinaryExprAST>(t.lex[0], std::move(RHS), std::move(compareStmt(tokens)));
		}
		return std::make_unique<BinaryExprAST>(t.lex[0], std::move(LHS), std::move(RHS));
	}

	/**
	 * @brief Parses a logic statement (I.E. "x == 5") consisting of only idents and string/number constants.
	 * LogicStmt itself only parses OR Statements (x || y), but makes calls to functions like andStmt, compareStmt, etc. to do the rest.
	 * EBNF isn't technically 100% accurate, but that's because a few assumptions should be made going into this, like,
	 * for example, all open parenthesis should have a matching closing parenthesis.
	 * logicStmt	-> <Stmt> <join> <logicStmt> | <Stmt>
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
			RHS = std::make_unique<BinaryExprAST>(t.lex[0], std::move(RHS), std::move(andStmt(tokens)));
		}
		return std::make_unique<BinaryExprAST>(t.lex[0], std::move(LHS), std::move(RHS));
	}

	/**
	 * A list representation can be displayed in EBNF as:
	 *  OBRACKET-> [ //Literal character
	 *  CBRACKET-> ] //Literal character
	 * 	REGLIST	-> <BoolStmt>{, <BoolStmt>} //Regular list: [1, 2, 3, 4, 5]
	 * 	ITERATOR-> <VAR> <FOREACHSTMT> //stolen directly from python syntax: [x+1 for x in list]
	 *  ListRep	-> <OBRACKET> (<REGLIST>|<ITERATOR>) <CBRACKET>
	 */
	std::unique_ptr<ExprAST> listExpr(Stack<Token> &tokens)
	{
		if (tokens.peek() != OPENSQUARE)
			return std::move(logicStmt(tokens));
		tokens.next();
		std::vector<std::unique_ptr<ExprAST>> contents;
		do
		{
			std::unique_ptr<ExprAST> LHS = std::move(logicStmt(tokens));
			if (LHS != NULL)
				contents.push_back(std::move(LHS));
			else
			{
				logError("Error when parsing list:", tokens.currentToken());
				return NULL;
			}
		} while (tokens.peek() == COMMA && tokens.next() == COMMA);
		if (tokens.peek() != CLOSESQUARE)
		{
			logError("Expected a closing square parenthesis here", tokens.currentToken());
			return NULL;
		}
		tokens.next();
		return std::make_unique<ListExprAST>(contents);
	}
	/**
	 * @brief Parses a block of code encased by two curly braces, if possible.
	 * If no braces are found, it just parses a single expression. EBNF is approximately: 
	 * <BLOCK>	-> '{' <EXPR>* '}' | <EXPR>
	 * <EXPR>	-> I'm not writing out the EBNF for every possible expression, you get the idea.
	 * 
	 *
	 * @param tokens
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> codeBlockExpr(Stack<Token> &tokens)
	{
		// return std::move(listExpr(tokens));
		std::vector<std::unique_ptr<ExprAST>> contents;
		if (tokens.peek() == OPENCURL)
		{
			tokens.next();
			if (tokens.peek() == CLOSECURL)
			{
				tokens.next();
				return std::move(std::make_unique<CodeBlockAST>(contents));
			}
			if (tokens.eof())
			{
				logError("Error: Unbalanced curly brace here:", tokens.currentToken());
				return NULL;
			}
		}
		else
			return std::move(getValidStmt(tokens));
		do
		{
			std::unique_ptr<ExprAST> LHS = std::move(getValidStmt(tokens));
			if (LHS != NULL)
				contents.push_back(std::move(LHS));
			else if (tokens.peek() != CLOSECURL)
			{
				logError("Error when parsing code block:", tokens.peek());
				return NULL;
			}
		} while (tokens.peek() != CLOSECURL && !tokens.eof());
		if (tokens.peek() != CLOSECURL || tokens.eof())
		{
			logError("Unbalanced curly brace after this token:", tokens.currentToken());
			return NULL;
		}
		tokens.next();
		return std::move(std::make_unique<CodeBlockAST>(contents)); //*/
	}

/**
 * @brief Parses an object/class blueprint
 * TODO: Get this function off the ground
 * 
 * @param tokens 
 * @return std::unique_ptr<ExprAST> 
 */
	std::unique_ptr<ExprAST> obj(Stack<Token> &tokens)
	{
		if (tokens.next().token != IDENT)
		{
			logError("Not an identifier", tokens.currentToken());
			return NULL;
		}
		logError("Error parsing a term in obj", tokens.peek());
		return NULL; // TODO: Fix this
	}

	/**
	 * @brief Takes in a name as an identifier, an equals sign, 
	 * then calls the lowest precidence operation in the AST.
	 *
	 * @param tokens
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> assign(Stack<Token> &tokens)
	{
		Token name = tokens.peek();
		if (name != IDENT)
		{
			logError("Expected identifier in place of this token:", tokens.peek());
			return NULL;
		}
		tokens.next();
		if (tokens.peek() == EQUALS) tokens.next();
		else if (tokens.peek() == INCREMENT || tokens.peek() == DECREMENT) tokens.go_back(); 
		else if (tokens.peek() == LPAREN){ 
			tokens.go_back();  
			return NULL; 
		}
		return std::move(listExpr(tokens));
	}

/**
 * @brief Parses the declaration for a function, from a stack of tokens, 
 * including the header prototype and argument list. 
 * 
 * @param tokens 
 * @return std::unique_ptr<FunctionAST> 
 */
	std::unique_ptr<FunctionAST> functionDecl(Stack<Token> &tokens)
	{
		Token name = tokens.next();
		std::vector<std::string> argnames;
		if (tokens.next() != LPAREN)
		{
			logError("Expected parenthesis here:", tokens.currentToken());
			return NULL;
		}
		if (tokens.peek() == RPAREN)
		{
			tokens.next();
			std::unique_ptr<PrototypeAST> proto = std::make_unique<PrototypeAST>(name.lex, argnames);
			std::unique_ptr<ExprAST> body = std::move(codeBlockExpr(tokens));
			if (body == NULL)
			{
				logError("Error in the body of function:", name);
				return NULL;
			}
			std::unique_ptr<FunctionAST> func = std::make_unique<FunctionAST>(std::move(proto), std::move(body));
			return func;
		}
		do
		{
			if (tokens.next().token > INT)
			{
				logError("Expected data type here:", tokens.currentToken());
				return NULL;
			}
			if (tokens.peek() == POINTER)
				tokens.next();
			if (tokens.peek() != IDENT)
			{
				logError("Expected identifier here:", tokens.currentToken());
				return NULL;
			}
			argnames.push_back(tokens.next().lex);
		} while (tokens.peek() == COMMA && tokens.next() == COMMA);
		std::unique_ptr<PrototypeAST> proto = std::make_unique<PrototypeAST>(name.lex, argnames);
		std::unique_ptr<ExprAST> body = std::move(codeBlockExpr(tokens));
		if (body == NULL)
		{
			logError("Error in the body of function:", name);
			return NULL;
		}
		std::unique_ptr<FunctionAST> func = std::make_unique<FunctionAST>(std::move(proto), std::move(body));
		return func;
	}

	/**
	 * @brief Called whenever the words "public", "private", "protected" are seen.
	 * Essentially we don't know from here if what's being defined is a function or a variable,
	 * so we just try to account for anything that comes up.
	 * @param tokens
	 * @return true if a valid function
	 */
	void declareOrFunction(Stack<Token> &tokens)
	{
		Token t;
		while (((t = tokens.peek()).token >= CONST && t.token <= PROTECTED) || t.token >= INT)
		{
			t = tokens.next();
			// Operators.push_back(t) //Add this to the variable modifier memory
			// return declareOrFunction(tokens);
		} //*/
		//std::unique_ptr<ExprAST> retval = std::move(assign(tokens));
	}

	/**
	 * @brief Parses a constructor for this class
	 * TODO: Get this function off the ground
	 * 
	 * @param tokens 
	 * @return std::unique_ptr<ExprAST> 
	 */
	std::unique_ptr<ExprAST> construct(Stack<Token> &tokens)
	{
		Token t = tokens.next();
		if (t != LPAREN)
		{
			tokens.go_back(1);
			logError("Expecting a Left Parenthesis at this token:", t);
			return NULL;
		}
		t = tokens.next();
		if (t != RPAREN)
		{
			tokens.go_back(1);
			logError("Expecting a right parenthesis at this token:", t);
			return NULL;
		}
		logError("Error parsing a term in construct ", t);
		return NULL;
		// Need to handle closing curly bracket
	}
	/**
	 * @brief Parses a destructor for a class
	 * TODO: Get this fuction off the ground
	 * 
	 * @param tokens 
	 * @return std::unique_ptr<ExprAST> 
	 */
	std::unique_ptr<ExprAST> destruct(Stack<Token> &tokens)
	{
		Token t = tokens.next();
		if (t != LPAREN)
		{
			tokens.go_back(1);
			logError("Expecting a left parenthesis at this token:", t);
			return NULL;
		}
		t = tokens.next();
		if (t != RPAREN)
		{
			tokens.go_back(1);
			logError("Expecting a right parenthesis at this token:", t);
			return NULL;
		}
		logError("Error in destruct()", t);
		return NULL; // TODO: Fix this
					 // Need to handle closing curly bracket
	}

	/**
	 * @brief Parses a for statement header, followed by a body statement/block
	 * TODO: Don't just return the body code, actually get the for stmt code generating properly
	 * TODO: Add support for do-while stmts
	 * TODO: Add support for while stmts
	 * @param tokens 
	 * @return std::unique_ptr<ExprAST> 
	 */
	std::unique_ptr<ExprAST> forStmt(Stack<Token> &tokens)
	{
		Token t = tokens.next();
		std::unique_ptr<ExprAST> b;
		do
		{
			b = std::move(assign(tokens));
		} while (tokens.peek() == COMMA && tokens.next() == COMMA);
		if (tokens.next() != SEMICOL)
		{
			tokens.go_back(1);
			logError("Expecting a semicolon at this token:", t);
			return NULL;
		}
		b = logicStmt(tokens);
		if (tokens.next() != SEMICOL)
		{
			tokens.go_back(1);
			logError("Expecting a semicolon at this token:", t);
			return NULL;
		}
		do
		{
			b = std::move(getValidStmt(tokens)); // TBI: variable checking
		} while (tokens.peek() == COMMA && tokens.next() == COMMA);
		if (tokens.peek() == SEMICOL)
			tokens.next();
		return b;
	}

	/**
	 * @brief Parses an if/else statement header and it's associated body
	 * TODO: Add support for else statement chains
	 * TODO: Get IfStmt code generation working
	 * TODO: Don't just return the body code, actually get the for stmt code generating properly
	 * @param tokens 
	 * @return std::unique_ptr<ExprAST> 
	 */
	std::unique_ptr<ExprAST> ifStmt(Stack<Token> &tokens)
	{
		if (tokens.next() != IF)
		{
			logError("Somehow we ended up looking for an if statement when there was no 'if'. Wtf.", tokens.currentToken());
			return NULL;
		}
		std::unique_ptr<ExprAST> b = std::move(logicStmt(tokens));

		return std::move(codeBlockExpr(tokens));
	}
	std::unique_ptr<ExprAST> caseStmt(Stack<Token> &tokens)
	{
		Token variable = tokens.next();
		if (variable.token != SCONST && variable.token != NUMCONST && variable.token != TRU && variable.token != FALS)
		{
			logError("I forget why this is here lol", variable);
			return NULL;
		}
		logError("Error in CaseStmt()", variable);
		return NULL; // Leaving this for now because I have to figure the specifics out later...
	}

/**
 * @brief parses a switch stmt, alongside it's associated case stmts.
 *  TODO: Get this function off the ground
 * 
 * @param tokens 
 * @return std::unique_ptr<ExprAST> 
 */
	std::unique_ptr<ExprAST> caseSwitchStmt(Stack<Token> &tokens)
	{
		Token variable = tokens.next();
		if (variable != IDENT)
		{
			tokens.go_back();
			logError("Expecting a variable at this token:", variable);
			return NULL;
		}
		return NULL; 
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
		Token t = tokens.peek();
		switch (t.token)
		{
		case IMPORT:
			return std::move(jimpilier::import(tokens));
		case FOR:
			return std::move(jimpilier::forStmt(tokens));
		case IF:
			return std::move(jimpilier::ifStmt(tokens));
		case SWITCH:
			return std::move(jimpilier::caseSwitchStmt(tokens));
		case CASE:
			return std::move(jimpilier::caseStmt(tokens));
		case OBJECT:
			return obj(tokens);
		case CONSTRUCTOR:
			return std::move(jimpilier::construct(tokens));
		case DESTRUCTOR:
			return std::move(jimpilier::destruct(tokens));
		case RPAREN:
		case LPAREN:
		case INCREMENT:
		case DECREMENT:
			return std::move(jimpilier::incDecExpr(tokens));
		case SEMICOL:
			logError("Returning null on token (under semicol)", tokens.peek());
			return NULL;
		case OPENCURL:
			return std::move(jimpilier::codeBlockExpr(tokens));
		case CLOSECURL:
			logError("Unbalanced curly brackets here:", tokens.peek());
			return NULL;
		case RET:
			logError("Returning null on token (under ret)", tokens.peek());
			return NULL;
		case INT:
		case SHORT:
		case POINTER:
		case LONG:
		case FLOAT:
		case DOUBLE:
		case STRING:
		case CHAR:
		case BOOL:
		case BYTE:
		case PUBLIC:
		case PRIVATE:
		case PROTECTED:
		case SINGULAR:
		case CONST:
			declareOrFunction(tokens);
		case IDENT:
			{
				std::unique_ptr<ExprAST> retval = std::move(assign(tokens)); 
				if(retval == NULL) {
					std::unique_ptr<FunctionAST> funcval = std::move(functionDecl(tokens)); 
					if(funcval == NULL){
						logError("Error parsing identifier here:", tokens.currentToken()); 
						return NULL; 
					}
					funcval->codegen(); 
					return NULL;
				}
				return retval; 
			}
		default:
			logError("Unknown token:", tokens.peek());
			tokens.go_back(1);
			return NULL;
		}
		logError("Returning null on error?", tokens.currentToken());
		return NULL;
	}
	/**
	 * @brief Takes a file name, opens the file, tokenizes it, and loads those into a custom Stack<Token> object
	 *  TODO: Move this function into driver code maybe ???
	 * @param fileDir 
	 * @return Stack<Token> 
	 */
	Stack<Token> loadTokens(string fileDir)
	{
		vector<Token> tokens;

		ifstream file;
		file.open(fileDir);
		if (!file)
		{
			std::cout << "File '" << fileDir << "' does not exist" << std::endl;
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
	/**
	 * @brief Soon-to-be depreciated
	 * TODO: Fix this whole damn function oh Lord help me
	 * @deprecated
	 * @param fileDir
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> analyzeFile(string fileDir)
	{
		std::unique_ptr<ExprAST> b;
		Stack<Token> s = loadTokens(fileDir);
		while (!s.eof() && (b = std::move(getValidStmt(s))))
		{
			if (!b)
				b->codegen();
			// std::cout<< s.eof() <<std::endl;
		}
		// if (!b)
		// {
		// 	Token err = s.peek(); // std::cout << openbrackets <<std::endl ;
		// 	std::cout << "Syntax error located at token '" << err.lex << "' on line " << err.ln << " in file: " << fileDir << "\n";
		// 	Token e2;
		// 	while (err.ln != -1 && (e2 = s.next()).ln == err.ln && !s.eof())
		// 	{
		// 		std::cout << e2.lex << " ";
		// 	}
		// 	std::cout << std::endl;
		// 	return NULL;
		// }
		return b;
	}
};