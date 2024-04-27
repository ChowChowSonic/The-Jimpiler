#include "llvm/Support/Casting.h"
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
#ifndef jimbo
#define jimbo
#include "globals.cpp" 
#include "TypeExpr.cpp" 
#include "AliasManager.cpp"
#include "ExprAST.cpp"
#include "tokenizer.cpp"
#include "Stack.cpp"
using namespace jimpilier; 
namespace jimpilier
{
	
	// <-- BEGINNING OF UTILITY FUNCTIONS -->

	void logError(string s, Token t)
	{
		errored = true;
		std::cout << s << ' ' << t.toString() << endl;
	}
	// <-- BEGINNING OF AST GENERATING FUNCTIONS -->

	std::vector<Variable> functionArgList(Stack<Token> &tokens);
	std::map<KeyToken, bool> variableModStmt(Stack<Token> &tokens);
	std::unique_ptr<FunctionAST> functionDecl(Stack<Token> &tokens, std::unique_ptr<TypeExpr> &dtype, std::string name, std::string objBase = "");
	std::unique_ptr<ExprAST> analyzeFile(string fileDir);
	std::unique_ptr<ExprAST> getValidStmt(Stack<Token> &tokens);
	std::unique_ptr<ExprAST> debugPrintStmt(Stack<Token> &tokens);
	std::unique_ptr<ExprAST> logicStmt(Stack<Token> &tokens);
	std::unique_ptr<ExprAST> mathExpr(Stack<Token> &tokens);
	std::unique_ptr<ExprAST> listExpr(Stack<Token> &tokens);
	std::unique_ptr<ExprAST> assignStmt(Stack<Token> &tokens, std::unique_ptr<ExprAST> LHS = NULL);
	std::unique_ptr<TypeExpr> variableTypeStmt(Stack<Token> &tokens);
	void functionArg(Stack<Token> &tokens, Variable &out);

	// TODO: Move this function into driver code maybe ???
	/**
	 * @brief Takes a file name, opens the file, tokenizes it, and loads those into a custom Stack<Token> object
	 *
	 * @param fileDir
	 * @return Stack<Token>
	 */
	Stack<Token> loadTokens(string fileDir)
	{
		vector<Token> tokens;
		std::ifstream file;
		file.open(fileDir);
		if (!file)
		{
			std::cout << "File '" << fileDir << "' does not exist" << std::endl;
			errored = true;
			Stack<Token> s;
			return s;
		}
		int ln = 1;
		while (!errored && !file.eof())
		{
			Token t = getNextToken(file, ln);
			t.ln = ln;
			if (t != ERR)
				tokens.push_back(t);
		}
		Stack<Token> s(tokens);
		return s;
	}

	std::string getFilePath(Stack<Token> &tokens)
	{
		std::string ret;
		if (tokens.peek() == SCONST)
			return tokens.next().lex;
		do
		{
			ret += tokens.next().lex;
		} while (!errored && tokens.peek() == PERIOD && tokens.next() == PERIOD && (ret += '/') != "");
		if (ret[0] != '.')
			ret = "./" + ret + ".jmb";
		std::cout << (ret) << endl;
		return ret;
	}

	std::unique_ptr<ExprAST> import(Stack<Token> &tokens)
	{
		if (tokens.next() != IMPORT)
			return NULL;
		std::unique_ptr<ExprAST> x;
		do
		{
			Stack<Token> tokens2 = loadTokens(getFilePath(tokens));
			while (!tokens2.eof())
			{
				x = getValidStmt(tokens2);
				if (errored)
					break;
				if (x != NULL)
					x->codegen();
				if (DEBUGGING)
					std::cout << endl;
			}
		} while (!errored && tokens.peek() == COMMA && tokens.next() == COMMA);

		return x;
	}

	/**
	 * @brief using EBNF notation, a term is roughly defined as:
	 *   <term> = (["+"|"-"]["++"|"--"] | "->" | "@")<IDENT>["++"|"--"] | "true" | "NULL" | "(" <expr> ")";
	 * However the ending [++|--] are only allowed if there is no (++|--) at the beginning of the term.
	 *
	 * @param tokens
	 * @return true if the syntax is valid
	 * @return NULL if syntax is not valid
	 */
	std::unique_ptr<ExprAST> term(Stack<Token> &tokens, std::unique_ptr<ExprAST> memberAccessParent = NULL, bool derefParent = false)
	{
		if (memberAccessParent != NULL)
			return std::make_unique<MemberAccessExprAST>(memberAccessParent, tokens.next().lex, derefParent);
		std::unique_ptr<ExprAST> LHS;
		Token s;
		string x;
		if (tokens.peek() == LPAREN)
		{
			tokens.next();
			LHS = std::move(assignStmt(tokens));
			if (tokens.peek() == RPAREN)
			{
				tokens.next();
				return LHS;
			}
		}
		switch (tokens.peek().token)
		{
		case (SCONST):
			s = tokens.next();
			return std::make_unique<StringExprAST>(s.lex);
		case (IDENT):
			s = tokens.next();
			return std::make_unique<VariableExprAST>(s.lex);
		case (NUMCONST):
			x = tokens.peek().lex;
			if (std::find(x.begin(), x.end(), '.') == x.end())
			{
				return std::make_unique<NumberExprAST>(stoi(tokens.next().lex));
			}
			return std::make_unique<NumberExprAST>(stod(tokens.next().lex));
		case (TRU):
		case (FALS):
			return std::make_unique<NumberExprAST>(tokens.next() == TRU);
		}
		logError("Invalid term:", tokens.peek());
		return NULL;
	}

	// TODO: Fix this function
	/**
	 * @brief Parses a function call statement, returns std::move(term(tokens)) if it does not find one
	 * @param tokens
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> functionCallExpr(Stack<Token> &tokens, std::unique_ptr<ExprAST> memberAccessParent = NULL, bool derefParent = false)
	{
		Token t = tokens.next();
		if (tokens.peek() == LPAREN)
		{
			tokens.next();
			std::vector<std::unique_ptr<ExprAST>> params;
			if (tokens.peek() != RPAREN)
				do
				{
					std::unique_ptr<ExprAST> param = std::move(jimpilier::assignStmt(tokens));
					if (param == NULL)
					{
						logError("Invalid parameter passed to function", t);
						return NULL;
					}
					params.push_back(std::move(param));
				} while (!errored && (tokens.peek() == COMMA && tokens.next() == COMMA));

			if (tokens.next() != RPAREN)
			{
				logError("Expected a closing parethesis '(' here:", tokens.currentToken());
				return NULL;
			}
			std::unique_ptr<ExprAST> retval;
			if (AliasMgr(t.lex) != NULL)
			{
				retval = std::make_unique<ObjectConstructorCallExprAST>(t.lex, params);
			}
			else if (memberAccessParent != NULL)
			{
				retval = std::make_unique<ObjectFunctionCallExprAST>(t.lex, params, memberAccessParent, derefParent);
			}
			else
			{
				retval = std::make_unique<CallExprAST>(t.lex, params);
			}
			return retval;
		}
		tokens.go_back();
		return std::move(term(tokens, std::move(memberAccessParent), derefParent));
	}

	std::unique_ptr<ExprAST> indexExpr(Stack<Token> &tokens, std::unique_ptr<ExprAST> base = NULL, std::unique_ptr<ExprAST> memberAccessParent = NULL, bool derefParent = false)
	{
		if (base == NULL)
			base = std::move(functionCallExpr(tokens, std::move(memberAccessParent), derefParent));
		if (tokens.peek() != OPENSQUARE)
			return base;
		tokens.next();
		std::unique_ptr<ExprAST> index = std::move(debugPrintStmt(tokens));
		if (tokens.peek() != CLOSESQUARE)
		{
			logError("Expected a closing square bracket when getting an index of an array/pointer here:", tokens.peek());
			return NULL;
		}
		tokens.next();
		if (tokens.peek() == OPENSQUARE)
		{
			std::unique_ptr<ExprAST> thisval = std::make_unique<IndexExprAST>(base, index);
			return std::move(indexExpr(tokens, std::move(thisval), std::move(memberAccessParent), derefParent));
		}
		else
			return std::make_unique<IndexExprAST>(base, index);
	}
	/**
	 * @brief function responsible for parsing dot expressions; for example: "vector.length()"
	 *
	 * @param tokens
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> memberAccessExpr(Stack<Token> &tokens, std::unique_ptr<ExprAST> base = NULL, bool derefParent = false)
	{
		if (base == NULL)
			base = std::move(indexExpr(tokens, NULL, NULL, derefParent));
		if (tokens.peek() != PERIOD && tokens.peek() != POINTERTO)
			return base;
		derefParent = tokens.next() == POINTERTO;
		std::unique_ptr<ExprAST> member = std::move(indexExpr(tokens, NULL, std::move(base), derefParent));
		// member = std::make_unique<MemberAccessExprAST>(base, member);
		if (tokens.peek() == PERIOD || tokens.peek() == POINTERTO)
			return std::move(memberAccessExpr(tokens, std::move(member)));
		return member;
	}

	std::unique_ptr<ExprAST> pointerToExpr(Stack<Token> &tokens)
	{
		if (tokens.peek() != POINTERTO)
		{
			return std::move(memberAccessExpr(tokens));
		}
		tokens.next();
		std::unique_ptr<ExprAST> refval = std::move(pointerToExpr(tokens));
		return std::make_unique<RefrenceExprAST>(refval);
	}

	std::unique_ptr<ExprAST> valueAtExpr(Stack<Token> &tokens)
	{
		if (tokens.peek() != REFRENCETO)
		{
			return std::move(pointerToExpr(tokens));
		}
		tokens.next();
		std::unique_ptr<ExprAST> drefval = std::move(valueAtExpr(tokens));

		return std::make_unique<DeRefrenceExprAST>(drefval);
	}

	std::unique_ptr<ExprAST> notExpr(Stack<Token> &tokens)
	{
		if (tokens.peek() != NOT)
			return std::move(valueAtExpr(tokens));
		tokens.next();
		std::unique_ptr<ExprAST> val = std::move(valueAtExpr(tokens));
		return std::make_unique<NotExprAST>(std::move(val));
	}

	std::unique_ptr<ExprAST> deleteStmt(Stack<Token> &tokens)
	{
		if (tokens.peek() != DEL)
		{
			return std::move(notExpr(tokens));
		}
		tokens.next();
		std::unique_ptr<ExprAST> delme = std::move(assignStmt(tokens));
		return std::make_unique<DeleteExprAST>(delme);
	}

	std::unique_ptr<ExprAST> ConstructorCallStmt(Stack<Token> &tokens, std::unique_ptr<ExprAST> heapVal = NULL)
	{
		std::unique_ptr<TypeExpr> ty = std::move(variableTypeStmt(tokens));
		if (ty == NULL)
		{
			return std::move(deleteStmt(tokens));
		}
		std::vector<std::unique_ptr<ExprAST>> params;
		if (tokens.peek() == LPAREN)
		{
			tokens.next();

			if (tokens.peek() != RPAREN)
				do
				{
					std::unique_ptr<ExprAST> param = std::move(jimpilier::assignStmt(tokens));
					if (param == NULL)
					{
						logError("Invalid parameter passed to function", tokens.peek());
						return NULL;
					}
					params.push_back(std::move(param));
				} while (!errored && (tokens.peek() == COMMA && tokens.next() == COMMA));
			if (tokens.next() != RPAREN)
			{
				logError("Expected a closing parethesis '(' here:", tokens.currentToken());
				return NULL;
			}
		}
		return std::make_unique<ObjectConstructorCallExprAST>(ty, params, heapVal);
	}

	/**
	 * @brief Alloocates space on the heap for a variable and returns a pointer to that variable. Like C++'s new operator or C's malloc()
	 *
	 * @param tokens
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> heapStmt(Stack<Token> &tokens)
	{
		if (tokens.peek() != HEAP)
		{
			return std::move(ConstructorCallStmt(tokens));
		}
		tokens.next();
		std::unique_ptr<ExprAST> retval = std::make_unique<HeapExprAST>();
		return ConstructorCallStmt(tokens, std::move(retval));
	}

	std::unique_ptr<ExprAST> sizeOfExpr(Stack<Token> &tokens)
	{
		std::unique_ptr<TypeExpr> tyval;
		std::unique_ptr<ExprAST> convertee;
		if (tokens.peek() != SIZEOF)
			return std::move(heapStmt(tokens));
		tokens.next();
		tyval = std::move(variableTypeStmt(tokens));
		if (tyval == NULL)
		{
			convertee = std::move(heapStmt(tokens));
			return std::make_unique<SizeOfExprAST>(convertee);
		}
		return std::make_unique<SizeOfExprAST>(tyval);
	}

	std::unique_ptr<ExprAST> typeAsExpr(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> convertee = std::move(sizeOfExpr(tokens));
		if (tokens.peek() != AS)
		{
			return convertee;
		}
		tokens.next();
		std::unique_ptr<TypeExpr> toconv = std::move(variableTypeStmt(tokens));
		return std::make_unique<TypeCastExprAST>(convertee, toconv);
	}

	std::unique_ptr<ExprAST> incDecExpr(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS;
		if (tokens.peek() == INCREMENT || tokens.peek() == DECREMENT)
		{
			Token t = tokens.next();
			char op = t.lex[0];
			LHS = std::move(typeAsExpr(tokens));
			return std::make_unique<IncDecExprAST>(true, op == '-', LHS);
		}
		LHS = std::move(typeAsExpr(tokens));
		if (LHS == NULL)
		{
			return NULL;
		}
		if (tokens.peek() == INCREMENT || tokens.peek() == DECREMENT)
		{
			Token t = tokens.next();
			char op = t.lex[0];
			return std::make_unique<IncDecExprAST>(false, op == '-', LHS);
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

		while (!errored && (tokens.peek() == POWERTO || tokens.peek() == LEFTOVER))
		{
			Token t2 = tokens.next();
			RHS = std::make_unique<PowModStmtAST>(std::move(RHS), std::move(term(tokens)), t2 == LEFTOVER);
			if (!LHS)
			{
				logError("Error parsing a term in raisedToExpr ", t);
				return NULL;
			}
		}
		return std::make_unique<PowModStmtAST>(std::move(LHS), std::move(RHS), t == LEFTOVER);
	}

	std::unique_ptr<ExprAST> multAndDivExpr(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS = std::move(raisedToExpr(tokens));
		if (tokens.peek() != MULT && tokens.peek() != DIV)
			return LHS;
		Token t = tokens.next();
		std::unique_ptr<ExprAST> RHS = std::move(raisedToExpr(tokens));

		while (!errored && (tokens.peek() == MULT || tokens.peek() == DIV))
		{
			t = tokens.next();
			RHS = std::make_unique<MultDivStmtAST>(std::move(RHS), std::move(raisedToExpr(tokens)), t == DIV);
			if (!LHS)
			{
				logError("Error parsing a term in multDivExpr ", t);
				return NULL;
			}
		}
		return std::make_unique<MultDivStmtAST>(std::move(LHS), std::move(RHS), t == DIV);
	}

	/**
	 * @brief Parses a mathematical expression. mathExpr itself only handles
	 * addition and subtraction, but calls multAndDivExpr & RaisedToExper for any multiplication or division.
	 * A mathExpr is approximately defined as:
	 *
	 * expr		-> <TERM> <join> <expr> | <TERM>
	 * join		-> "+" | "-" | "*" | "/" | "^"
	 * term is denfined in the function of the same name -> "std::unique_ptr<ExprAST> term(Stack<Token> &tokens)"
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

		while (!errored && (tokens.peek() == PLUS || tokens.peek() == MINUS))
		{
			t = tokens.next();
			RHS = std::make_unique<AddSubStmtAST>(std::move(RHS), std::move(multAndDivExpr(tokens)), t == MINUS);
			if (!LHS)
			{
				logError("Error parsing a term in mathExpr", t);
				return NULL;
			}
		}
		return std::make_unique<AddSubStmtAST>(std::move(LHS), std::move(RHS), t == MINUS);
	}

	std::unique_ptr<ExprAST> greaterLessStmt(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS = std::move(mathExpr(tokens));
		if (tokens.peek() != GREATER && tokens.peek() != LESS)
			return LHS;
		Token t = tokens.next();
		std::unique_ptr<ExprAST> RHS = std::move(mathExpr(tokens));
		while (!errored && (tokens.peek() == GREATER || tokens.peek() == LESS))
		{
			Token T = tokens.next();
			RHS = std::make_unique<GtLtStmtAST>(std::move(RHS), std::move(mathExpr(tokens)), T == LESS);
		}
		return std::make_unique<GtLtStmtAST>(std::move(LHS), std::move(RHS), t == LESS);
	}
	// TODO: Fix and/or statements, make them n-way like addition or subtraction
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
		while (!errored && (tokens.peek() == EQUALCMP || tokens.peek() == NOTEQUAL))
		{
			Token t2 = tokens.next();
			RHS = std::make_unique<CompareStmtAST>(std::move(RHS), std::move(greaterLessStmt(tokens)), t2 == NOTEQUAL);
		}
		return std::make_unique<CompareStmtAST>(std::move(LHS), std::move(RHS), t == NOTEQUAL);
	}

	std::unique_ptr<ExprAST> andStmt(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS = std::move(compareStmt(tokens));
		if (tokens.peek() != AND)
			return LHS;
		Token t = tokens.next();
		std::unique_ptr<ExprAST> RHS = std::move(compareStmt(tokens));
		while (!errored && tokens.peek() == AND)
		{
			tokens.next();
			RHS = std::make_unique<BinaryStmtAST>(t.lex, std::move(RHS), std::move(compareStmt(tokens)));
		}
		return std::make_unique<BinaryStmtAST>(t.lex, std::move(LHS), std::move(RHS));
	}
	// TODO: Plan out operator precidence so that you can write code without/minimally hitting shift
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
	 *
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
		while (!errored && tokens.peek() == OR)
		{
			tokens.next();
			RHS = std::make_unique<BinaryStmtAST>(t.lex, std::move(RHS), std::move(andStmt(tokens)));
		}
		return std::make_unique<BinaryStmtAST>(t.lex, std::move(LHS), std::move(RHS));
	}

	std::unique_ptr<ExprAST> debugPrintStmt(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> x = std::move(logicStmt(tokens));
		if (tokens.peek() != NOT)
			return x;
		return std::make_unique<DebugPrintExprAST>(std::move(x), tokens.next().ln);
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
		{
			std::unique_ptr<ExprAST> x = std::move(debugPrintStmt(tokens));
			if (tokens.peek() == SEMICOL)
				tokens.next();
			return x;
		}
		tokens.next();
		std::vector<std::unique_ptr<ExprAST>> contents;
		do
		{
			std::unique_ptr<ExprAST> LHS;
			if (tokens.peek() == OPENSQUARE)
				LHS = std::move(listExpr(tokens));
			else
				LHS = std::move(debugPrintStmt(tokens));
			if (LHS != NULL)
				contents.push_back(std::move(LHS));
			else
			{
				logError("Error when parsing list:", tokens.currentToken());
				return NULL;
			}
		} while (!errored && tokens.peek() == COMMA && tokens.next() == COMMA);
		if (tokens.peek() != CLOSESQUARE)
		{
			logError("Expected a closing square parenthesis here", tokens.currentToken());
			return NULL;
		}
		tokens.next();
		if (tokens.peek() == SEMICOL)
			tokens.next();
		return std::make_unique<ListExprAST>(contents);
	}
	/**
	 * @brief Parses a block of code encased by two curly braces, if possible.
	 * If no braces are found, it just parses a single expression. EBNF is approximately:
	 * <BLOCK>	-> '{' <EXPR>* '}' | <EXPR>
	 * <EXPR>	-> I'm not writing out the EBNF for every possible expression, you get the idea.
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
			if (errored)
				return NULL;
			if (LHS != NULL)
				contents.push_back(std::move(LHS));
		} while (!errored && tokens.peek() != CLOSECURL && !tokens.eof());
		if (tokens.peek() != CLOSECURL || tokens.eof())
		{
			logError("Unbalanced curly brace after this token:", tokens.currentToken());
			return NULL;
		}
		tokens.next();
		return std::move(std::make_unique<CodeBlockAST>(contents)); //*/
	}

	/**
	 * @brief Parses a constructor for this class
	 *
	 *
	 * @param tokens
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> construct(Stack<Token> &tokens, std::string obj)
	{
		Token t = tokens.next();
		if (t != CONSTRUCTOR)
		{
			logError("Expected a constructor token here:", t);
			return NULL;
		}
		t = tokens.next();
		if (t != LPAREN)
		{
			tokens.go_back(1);
			logError("Expecting a Left Parenthesis at this token:", t);
			return NULL;
		}
		std::vector<Variable> args = std::move(functionArgList(tokens));
		t = tokens.next();
		if (t != RPAREN)
		{
			tokens.go_back(1);
			logError("Expecting a right parenthesis at this token:", t);
			return NULL;
		}
		std::unique_ptr<ExprAST> body = std::move(codeBlockExpr(tokens));
		return std::make_unique<ConstructorExprAST>((args), body, obj);
	}
	// TODO: Get this fuction off the ground
	/**
	 * @brief Parses a destructor for a class
	 *
	 *
	 * @param tokens
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> destruct(Stack<Token> &tokens, std::string parentType)
	{
		Token t = tokens.next();
		std::unique_ptr<TypeExpr> v =std::move(std::make_unique<VoidTypeExpr>()); 
		std::vector<Variable> list;
		return std::make_unique<FunctionAST>(std::make_unique<PrototypeAST>("destructor@"+parentType, list, v, parentType), std::move(codeBlockExpr(tokens)), parentType);
	}

	void thisOrFunctionArg(Stack<Token> &tokens, Variable &out, std::string parentTy = "")
	{
		if (tokens.peek() == IDENT && tokens.peek().lex == "this")
		{
			std::string thisval = "this";
			std::unique_ptr<TypeExpr> ty = std::make_unique<StructTypeExpr>(parentTy);
			ty = std::make_unique<PointerToTypeExpr>(ty); //TODO Implement references
			out.name = thisval;
			out.ty = parentTy == "" ? NULL : std::move(ty);
			tokens.next();
			return;
		}
		functionArg(tokens, out);
	}

	std::unique_ptr<ExprAST> operatorOverloadStmt(Stack<Token> &tokens, std::unique_ptr<TypeExpr> ty, std::string parentName = "")
	{
		if (tokens.next() != OPERATOR)
		{
			return NULL;
		}
		bool hasParen = false;
		if (tokens.peek() == LPAREN)
			hasParen = tokens.next() == LPAREN;
		Variable placeholder;
		std::vector<Variable> vars;
		if (tokens.peek() == IDENT || tokens.peek().token >= INT)
		{
			thisOrFunctionArg(tokens, placeholder, parentName);
			if (placeholder.ty == NULL)
			{
				logError("Unknown type defined in operator:", tokens.peek());
				return NULL;
			}
			vars.push_back(std::move(placeholder));
		}
		bool unary = false; 
		Token op = tokens.next();
		switch (op.token)
		{
		case PLUS:
		case MINUS:
		case MULT:
		case DIV:
		case POWERTO:
		case LEFTOVER:
		case EQUALCMP:
		case NOTEQUAL:
		case GREATER:
		case GREATEREQUALS:
		case LESS:
		case LESSEQUALS:
		case INSERTION:
		case REMOVAL:
		case OPENSQUARE:
			thisOrFunctionArg(tokens, placeholder, parentName);
			vars.push_back(std::move(placeholder));
			if (op == OPENSQUARE && tokens.peek() != CLOSESQUARE)
			{
				errored = true;
				logError("Expected a closing square brace at this token:", tokens.currentToken());
				return NULL;
			}
			else if (op == OPENSQUARE && tokens.peek() == CLOSESQUARE)
			{
				tokens.next();
			}
			break;
		// case PERIOD:
		default:
			errored = true;
			logError("Unknown operator:", tokens.currentToken());
			return NULL;
		case NOT:
		case POINTERTO:
		case REFRENCETO:
		case PRINT:
		case PRINTLN:
			unary = true; 
			placeholder.ty = NULL; 
			vars.push_back(std::move(placeholder));
			thisOrFunctionArg(tokens, placeholder, parentName);
			vars.push_back(std::move(placeholder));
			break;
			case INCREMENT:
			case DECREMENT:
			if(vars.size() == 0){
				placeholder.ty = NULL; 
				vars.push_back(std::move(placeholder));
			}else if(vars.size() == 1){
				placeholder.ty = NULL; 
				vars.push_back(std::move(placeholder));
				break; 
			}
			thisOrFunctionArg(tokens, placeholder, parentName);
			vars.push_back(std::move(placeholder));
			if(vars.size() == 1){
				placeholder.ty = NULL; 
				vars.push_back(std::move(placeholder));
				break; 
			}
			break; 
		}
		if (hasParen && tokens.peek() != RPAREN)
		{
			logError("Missing closing parenthesis after this token:", tokens.currentToken());
			return NULL;
		}
		else if (hasParen)
			tokens.next();
		std::unique_ptr<ExprAST> body = std::move(codeBlockExpr(tokens));
		std::unique_ptr<ExprAST> retval;
		retval = std::make_unique<OperatorOverloadAST>(op.lex, ty, vars, (body), unary);
		return retval;
	}

	std::vector<std::unique_ptr<TypeExpr>> templateObjNames(Stack<Token> &tokens)
	{
		vector<std::unique_ptr<TypeExpr>> types;
		if (tokens.peek() != LESS)
		{
			return types;
		}
		tokens.next();
		do
		{
			Token name = tokens.next();
			if (name != IDENT)
			{
				logError("Expected a template identifier, instead got:", name);
				types.clear();
				return types;
			}
			types.push_back(std::make_unique<TemplateTypeExpr>(name.lex));
		} while (!errored && tokens.peek() == COMMA && tokens.next() == COMMA);
		return types;
	}
	/**
	 * @brief Parses an object/class blueprint
	 * @param tokens
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> obj(Stack<Token> &tokens)
	{
		std::vector<std::pair<std::string, std::unique_ptr<TypeExpr>>> objVars;
		std::vector<std::unique_ptr<ExprAST>> objFunctions;
		std::vector<std::unique_ptr<ExprAST>> overloadedOperators;
		std::unique_ptr<ExprAST> destructor = NULL;
		if (tokens.peek() == OBJECT)
			tokens.next();
		if (tokens.peek() != IDENT)
		{
			logError("Expected object name at this token:", tokens.peek());
			return NULL;
		}
		std::string name = tokens.next().lex;
		std::vector<std::unique_ptr<TypeExpr>> templates = std::move(templateObjNames(tokens));
		ObjectHeaderExpr objName(name);
		objName.codegen();
		if (tokens.next() != OPENCURL)
		{
			logError("Curly braces are required for object declarations. Please put a brace before this token:", tokens.currentToken());
			return NULL;
		}
		while (!errored && tokens.peek() != CLOSECURL)
		{
			if (tokens.peek() == CONSTRUCTOR)
			{
				std::unique_ptr<ExprAST> constructorast = std::move(construct(tokens, objName.name));
				objFunctions.push_back(std::move(constructorast));
				continue;
			}else if(tokens.peek() == DESTRUCTOR){
				destructor = std::move(destruct(tokens, objName.name)); 
				continue; 
			}
			std::unique_ptr<TypeExpr> ty = std::move(variableTypeStmt(tokens));
			if (ty == NULL)
			{
				logError("Invalid variable declaration found here:", tokens.peek());
				return NULL;
			}
			Token name = tokens.peek();
			if (name == OPERATOR)
			{
				std::unique_ptr<ExprAST> op = std::move(operatorOverloadStmt(tokens, std::move(ty), objName.name));
				overloadedOperators.push_back(std::move(op));
				continue;
			}
			else if (name != IDENT)
			{
				logError("Expected identifier at this token:", name);
				return NULL;
			}
			tokens.next();
			if (tokens.peek() == LPAREN)
			{
				std::unique_ptr<ExprAST> func = std::move(functionDecl(tokens, ty, name.lex, objName.name));
				objFunctions.push_back(std::move(func));
				continue;
			}
			if (tokens.peek() == SEMICOL)
				tokens.next();
			objVars.push_back(std::pair<std::string, std::unique_ptr<TypeExpr>>(name.lex, std::move(ty)));
		}
		tokens.next();
		return std::make_unique<ObjectExprAST>(objName, objVars, objFunctions, overloadedOperators, destructor);
	}
	std::unique_ptr<TypeExpr> variableTypeStmt(Stack<Token> &tokens)
	{
		Token t = tokens.next();
		std::unique_ptr<TypeExpr> type = NULL;
		switch (t.token)
		{
		case INT:
			type = std::make_unique<IntTypeExpr>();
			break;
		case SHORT:
			type = std::make_unique<ShortTypeExpr>();
			break;
		case LONG:
			type = std::make_unique<LongTypeExpr>();
			break;
		case FLOAT:
			type = std::make_unique<FloatTypeExpr>();
			break;
		case DOUBLE:
			type = std::make_unique<DoubleTypeExpr>();
			break;
		case STRING:
			type = std::make_unique<ByteTypeExpr>();
			type = std::make_unique<PointerToTypeExpr>(type);
			break;
		case BOOL:
			type = std::make_unique<BoolTypeExpr>();
			break;
		case CHAR:
		case BYTE:
			type = std::make_unique<ByteTypeExpr>();
			break;
		case IDENT:
			type = std::make_unique<StructTypeExpr>(t.lex);
			if (type->codegen(true) == NULL)
			{
				tokens.go_back();
				return NULL;
			}
			break;
		default:
			tokens.go_back();
			return NULL;
		}
		while (tokens.peek() == POINTER || tokens.peek() == MULT)
		{
			tokens.next();
			type = std::make_unique<PointerToTypeExpr>(type);
		}
		if(tokens.peek() == REFRENCETO) type = std::make_unique<ReferenceToTypeExpr>(type); 
		return type;
	}
	/**
	 * @brief Called whenever a modifier keyword are seen.
	 * Parses modifier keywords and changes the modifiers set while advancing through
	 * the tokens list. After this function is called a variable or function is expected
	 * to be created, at which point you should use the modifiers list that was
	 * edited by this to determine the modifiers of that variable. Clear that lists after use.
	 * @see variable modifiers (TBI)
	 * @param tokens
	 * @return true if a valid function
	 */
	std::map<KeyToken, bool> variableModStmt(Stack<Token> &tokens)
	{
		// clear variable modifier memory if possible
		// TBI
		Token t = tokens.peek();
		std::map<KeyToken, bool> modifiers =
			{
				{CONST, false},
				{SINGULAR, false},
				{VOLATILE, false},
				{PUBLIC, false},
				{PRIVATE, false},
				{PROTECTED, false},
			};
		while ((t = tokens.peek()).token <= PROTECTED && t.token >= CONST)
		{
			modifiers[t.token] = true;
			t = tokens.next();
		}
		// if(tokens.peek() == COLON){
		// tokens.next();
		// keepMods = true;
		// }else{
		// keepMods = false;
		// }
		return modifiers;
	}

	/**
	 * @brief Takes in a name as an identifier, an equals sign,
	 * then calls the lowest precidence operation in the AST.
	 * @param tokens
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> assignStmt(Stack<Token> &tokens, std::unique_ptr<ExprAST> LHS)
	{
		if (LHS == NULL)
			LHS = std::move(listExpr(tokens));
		std::unique_ptr<ExprAST> RHS = NULL;
		if (tokens.peek() == EQUALS)
		{
			tokens.next();
			RHS = std::move(assignStmt(tokens));
		}
		else
			return LHS;

		return std::make_unique<AssignStmtAST>(LHS, RHS);
	}

	std::unique_ptr<ExprAST> declareStmt(Stack<Token> &tokens)
	{
		std::map<KeyToken, bool> mods = variableModStmt(tokens);
		std::unique_ptr<TypeExpr> dtype = std::move(variableTypeStmt(tokens));
		if (dtype == NULL)
		{
			return std::move(assignStmt(tokens));
		}
		std::vector<std::unique_ptr<ExprAST>> vars;
		do
		{
			Token name = tokens.peek();
			if (name == OPERATOR)
			{
				return operatorOverloadStmt(tokens, std::move(dtype));
			}
			else if (name != IDENT)
			{
				tokens.go_back();
				return std::move(assignStmt(tokens)); // Does this case ever really come up? I gotta double check C's EBNF
			}
			tokens.next();
			if (tokens.peek() == LPAREN)
			{
				return std::move(functionDecl(tokens, dtype, name.lex));
			}
			std::unique_ptr<ExprAST> declval = std::make_unique<DeclareExprAST>(name.lex, std::move(dtype->clone()), false);
			declval = std::move(assignStmt(tokens, std::move(declval)));
			vars.push_back(std::move(declval));
		} while (tokens.peek() == COMMA && tokens.next() == COMMA);
		return std::make_unique<CodeBlockAST>(vars);
	}

	/**
	 * Parses a single function argument
	 * Function arguments are approximately equal to a data type declaration followed by an identifier
	 */
	void functionArg(Stack<Token> &tokens, Variable &out)
	{
		std::unique_ptr<TypeExpr> dtype = std::move(jimpilier::variableTypeStmt(tokens));
		if (dtype == NULL)
		{
			logError("Unknown type when declaring a variable:", tokens.peek());
			out.name = "ERROR";
			out.ty = NULL;
			return;
		}
		else if (tokens.peek() != IDENT)
		{
			logError("Expected identifier after variable type here:", tokens.currentToken());
			dtype.release();
			dtype = nullptr;
			out.ty = nullptr;
			out.name = "ERROR";
		}
		Token t = tokens.next();
		out.name = t.lex;
		out.ty = std::move(dtype);
	}

	std::vector<Variable> functionArgList(Stack<Token> &tokens)
	{
		std::vector<Variable> args;
		if (tokens.peek() != RPAREN)
			do
			{
				Variable v;
				functionArg(tokens, v);
				if (v.ty != nullptr)
					args.push_back(std::move(v));
			} while (!errored && tokens.peek() == COMMA && tokens.next() == COMMA);
		return args;
	}

	/**
	 * @brief Parses the declaration for a function from a stack of tokens,
	 * including the next body, header/prototype and argument list.
	 * @param tokens
	 * @return std::unique_ptr<FunctionAST>
	 */
	std::unique_ptr<FunctionAST> functionDecl(Stack<Token> &tokens, std::unique_ptr<TypeExpr> &dtype, std::string name, std::string objBase)
	{

		std::vector<Variable> args;

		tokens.next();
		args = std::move(functionArgList(tokens));
		if (tokens.peek() == RPAREN)
		{
			tokens.next();
			std::unique_ptr<PrototypeAST> proto = std::make_unique<PrototypeAST>(name, args, dtype, objBase);
			std::unique_ptr<ExprAST> body = std::move(codeBlockExpr(tokens));
			std::unique_ptr<FunctionAST> func = std::make_unique<FunctionAST>(std::move(proto), std::move(body));
			return func;
		}
		logError("Expected a closing parenthesis here:", tokens.currentToken());
		return NULL;
	}
	std::unique_ptr<ExprAST> doWhileStmt(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> condition, body;
		if (tokens.next() != DO)
		{
			logError("Somehow went looking for a do statement when there is none. wtf.", tokens.currentToken());
			return NULL;
		}
		body = std::move(jimpilier::codeBlockExpr(tokens));
		bool hasparen = false;
		if (tokens.peek() != WHILE)
		{
			return std::move(body);
		}
		tokens.next();
		if (tokens.peek() == LPAREN && tokens.next() == LPAREN)
			hasparen = true;
		condition = std::move(jimpilier::logicStmt(tokens));
		if (tokens.peek() == SEMICOL)
			tokens.next();
		if (hasparen && tokens.peek() != RPAREN)
		{
			logError("Unclosed parenthesis surrounding for statement after this token:", tokens.currentToken());
			return NULL;
		}
		else if (hasparen && tokens.peek() == RPAREN)
		{
			tokens.next();
		}

		return std::make_unique<ForExprAST>(std::move(condition), std::move(body), true);
	}
	std::unique_ptr<ExprAST> whileStmt(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> condition, body;
		bool hasparen = false;
		if (tokens.next() != WHILE)
		{
			logError("Somehow we ended up looking for a 'for' statement where there is none. wtf.", tokens.currentToken());
			return NULL;
		}
		if (tokens.peek() == LPAREN && tokens.next() == LPAREN)
			hasparen = true;
		condition = std::move(jimpilier::logicStmt(tokens));
		if (tokens.peek() == SEMICOL)
			tokens.next();
		if (hasparen && tokens.peek() != RPAREN)
		{
			logError("Unclosed parenthesis surrounding for statement after this token:", tokens.currentToken());
			return NULL;
		}
		else if (hasparen && tokens.peek() == RPAREN)
		{
			tokens.next();
		}
		body = std::move(jimpilier::codeBlockExpr(tokens));
		return std::make_unique<ForExprAST>(std::move(condition), std::move(body));
	}
	/**
	 * @brief Parses a for statement header, followed by a body statement/block

	 * @param tokens
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> forStmt(Stack<Token> &tokens)
	{
		std::vector<std::unique_ptr<ExprAST>> beginStmts, endStmts;
		std::unique_ptr<ExprAST> condition, body;
		bool hasparen = false;
		if (tokens.next() != FOR)
		{
			logError("Somehow we ended up looking for a 'for' statement where there is none. wtf.", tokens.currentToken());
			return NULL;
		}
		if (tokens.peek() == LPAREN && tokens.next() == LPAREN)
			hasparen = true;
		do
		{
			std::unique_ptr<ExprAST> x = std::move(declareStmt(tokens));
			if (x != NULL)
				beginStmts.push_back(std::move(x));
		} while (!errored && tokens.peek() == COMMA && tokens.next() == COMMA);
		if (errored)
			return NULL;
		if (tokens.currentToken() != SEMICOL && tokens.peek() != SEMICOL)
		{
			tokens.go_back(1);
			logError("Expecting a semicolon after this token:", tokens.currentToken());
			return NULL;
		}
		condition = std::move(jimpilier::logicStmt(tokens));
		if (tokens.currentToken() != SEMICOL && tokens.peek() != SEMICOL)
		{
			tokens.go_back(1);
			logError("Expecting a semicolon after this token:", tokens.currentToken());
			return NULL;
		}
		tokens.next();
		do
		{
			std::unique_ptr<ExprAST> x = std::move(getValidStmt(tokens));
			if (x != NULL)
				endStmts.push_back(std::move(x)); // TBI: variable checking
		} while (!errored && tokens.peek() == COMMA && tokens.next() == COMMA);
		if (errored)
			return NULL;
		if (tokens.peek() == SEMICOL)
			tokens.next();
		if (hasparen && tokens.peek() != RPAREN)
		{
			logError("Unclosed parenthesis surrounding for statement after this token:", tokens.currentToken());
			return NULL;
		}
		else if (hasparen && tokens.peek() == RPAREN)
		{
			tokens.next();
		}
		body = std::move(jimpilier::codeBlockExpr(tokens));
		return std::make_unique<ForExprAST>(beginStmts, std::move(condition), std::move(body), endStmts);
	}

	/**
	 * @brief Parses an if/else statement header and it's associated body
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
		std::vector<std::unique_ptr<ExprAST>> conds, bodies;
		do
		{
			if (tokens.peek() == IF)
			{
				tokens.next();
				b = std::move(logicStmt(tokens));
			}
			if (b != NULL)
				conds.push_back(std::move(b));
			std::unique_ptr<ExprAST> body = std::move(codeBlockExpr(tokens));
			if (body == NULL)
				return NULL;
			bodies.push_back(std::move(body));

		} while (!errored && tokens.peek() == ELSE && tokens.next() == ELSE);

		return std::make_unique<IfExprAST>(std::move(conds), std::move(bodies));
	}

	std::unique_ptr<ExprAST> caseStmt(Stack<Token> &tokens)
	{
		return NULL; // Leaving this for now because I have to figure the specifics out later...
	}

	/**
	 * @brief parses a switch stmt, alongside it's associated case stmts.
	 *
	 * @param tokens
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> switchStmt(Stack<Token> &tokens)
	{
		bool autobreak = false;
		if (tokens.next() != SWITCH)
		{
			logError("Somehow was expecting a switch stmt when there is no switch. Wtf.", tokens.currentToken());
			return NULL;
		}
		if (tokens.peek() == AUTO)
		{
			tokens.next();
			Token nextTok = tokens.next();
			if (nextTok == BREAK)
			{
				autobreak = true;
			}
			else
			{
				logError("Unknown option after 'auto'; did you mean 'break'?", tokens.currentToken());
				return NULL;
			}
		}
		std::unique_ptr<ExprAST> variable = std::move(logicStmt(tokens));
		if (variable == NULL)
			return NULL;
		if (tokens.next() != OPENCURL)
		{
			logError("Sorry! Switch Statements MUST have a set of curly braces after them; A switch without multiple switches is just pointless!", tokens.currentToken());
			return NULL;
		}
		std::vector<std::unique_ptr<ExprAST>> cases, conds;
		int defaultposition = -1, ctr = 0;
		do
		{
			Token nxt = tokens.next();
			std::unique_ptr<ExprAST> value;
			if (nxt == CASE)
			{
				value = std::move(logicStmt(tokens));
				conds.push_back(std::move(value));
			}
			else if (nxt == DEFAULT)
			{
				defaultposition = ctr;
				conds.push_back(NULL);
			}
			else
			{
				logError("Expected either 'case' or 'default' here:", tokens.currentToken());
				return NULL;
			}
			std::unique_ptr<ExprAST> body = std::move(codeBlockExpr(tokens));
			cases.push_back(std::move(body));
			ctr++;
		} while (!errored && (tokens.peek() == CASE || tokens.peek() == DEFAULT));

		tokens.next();
		// if(autobreak)
		return std::make_unique<SwitchExprAST>(std::move(variable), std::move(conds), std::move(cases), autobreak, defaultposition);
		// return NULL;
	}

	std::unique_ptr<ExprAST> retStmt(Stack<Token> &tokens)
	{
		if (tokens.next() != RET)
		{
			logError("Somehow was looking for a return statement when there was no return. Wtf.", tokens.currentToken());
			return NULL;
		}
		std::unique_ptr<ExprAST> val = NULL; 
		if(!(tokens.peek() == SEMICOL || tokens.peek() == CLOSECURL))
			val = std::move(jimpilier::listExpr(tokens));
		return std::make_unique<RetStmtAST>(val);
	}

	std::unique_ptr<ExprAST> printStmt(Stack<Token> &tokens)
	{
		bool isline = tokens.peek() == PRINTLN;
		if (tokens.peek() != PRINT && !isline)
		{
			logError("Somehow was looking for a print statement when there was no print. Wtf.", tokens.currentToken());
			return NULL;
		}
		tokens.next();
		std::vector<std::unique_ptr<ExprAST>> args;
		do
		{
			std::unique_ptr<ExprAST> x = std::move(jimpilier::listExpr(tokens));
			if (x != NULL)
				args.push_back(std::move(x));
		} while (!errored && tokens.peek() == COMMA && tokens.next() == COMMA);
		return std::make_unique<PrintStmtAST>(args, isline);
	}

	std::unique_ptr<ExprAST> throwStmt(Stack<Token> &tokens)
	{
		if (tokens.next() != THROW)
		{
			logError("No throw statement found", tokens.currentToken());
			return NULL;
		}
		std::unique_ptr<ExprAST> obj = heapStmt(tokens);
		return std::make_unique<ThrowStmtAST>(obj);
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
			jimpilier::import(tokens);
			return NULL;
		case FOR:
			return std::move(jimpilier::forStmt(tokens));
		case DO:
			return std::move(jimpilier::doWhileStmt(tokens));
		case WHILE:
			return std::move(jimpilier::whileStmt(tokens));
		case IF:
			return std::move(jimpilier::ifStmt(tokens));
		case SWITCH:
			return std::move(jimpilier::switchStmt(tokens));
		case CASE:
			return std::move(jimpilier::caseStmt(tokens));
		case OBJECT:
			return obj(tokens);
		case SEMICOL:
			tokens.next();
			return NULL;
		case OPENCURL:
			return std::move(jimpilier::codeBlockExpr(tokens));
		case CLOSECURL:
			logError("Unbalanced curly brackets here:", tokens.peek());
			return NULL;
		case RET:
			return std::move(jimpilier::retStmt(tokens));
		case PRINTLN:
		case PRINT:
			return std::move(jimpilier::printStmt(tokens));
		case BREAK:
			tokens.next();
			return std::make_unique<BreakExprAST>();
		case CONTINUE:
			tokens.next();
			return std::make_unique<ContinueExprAST>();
		case THROW:
			return std::move(jimpilier::throwStmt(tokens));
		default:
			return std::move(declareStmt(tokens));
		}
	}
};
#endif