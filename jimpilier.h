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
	std::unique_ptr<llvm::LLVMContext> ctxt;
	std::unique_ptr<llvm::IRBuilder<>> builder;
	std::unique_ptr<llvm::Module> GlobalVarsAndFunctions;
	std::map<std::string, llvm::Value *> variables;
	std::map<std::string, llvm::Type *> dtypes;
	std::vector<std::string> importedFiles;
	llvm::Function *currentFunction;
	/* Strictly for testing purposes, not meant for releases*/
	const bool DEBUGGING = false;
	bool errored = false;
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
		bool isInt = false;

	public:
		NumberExprAST(double Val) : Val(Val) {}
		NumberExprAST(int Val) : Val(Val) { isInt = true; }
		llvm::Value *codegen()
		{
			if (DEBUGGING)
				std::cout << Val;
			if (isInt)
				return llvm::ConstantInt::get(*ctxt, llvm::APInt(32, static_cast<long>(Val), true));
			return llvm::ConstantFP::get(*ctxt, llvm::APFloat((float)Val));
		}
	};
	class StringExprAST : public ExprAST
	{
		std::string Val;

	public:
		StringExprAST(std::string val) : Val(val) {}
		llvm::Value *codegen()
		{
			if (DEBUGGING)
				std::cout << Val;
			return builder->CreateGlobalStringPtr(Val, "Sconst");
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
			if (DEBUGGING)
				std::cout << Name;
			llvm::Value *V = variables[Name];
			if (!V || !dtypes[Name])
			{
				std::cout << "Unknown variable name: " << Name << endl;
				return nullptr;
			}
			return builder->CreateLoad(dtypes[Name], V, Name);
		}
	};
	/**
	 * @brief Represents a return statement for a function
	 * TODO: Add type casting here
	 */
	class RetExprAST : public ExprAST
	{
		unique_ptr<ExprAST> ret;

	public:
		RetExprAST(unique_ptr<ExprAST> &val) : ret(std::move(val)) {}
		llvm::Value *codegen()
		{
			if (DEBUGGING)
				std::cout << "return ";
			llvm::Value *retval = ret->codegen();
			if (retval->getType() != currentFunction->getReturnType())
			{
				// builder->CreateCast() //Add type casting here
			}
			if (retval == NULL)
				return NULL;
			return builder->CreateRet(retval);
		}
	};

	class AssignStmtAST : public ExprAST
	{
	public:
		std::string variable;
		std::unique_ptr<ExprAST> val;
		llvm::Type *dtype;

		AssignStmtAST(std::string var, std::unique_ptr<ExprAST> &value, llvm::Type *dtype = NULL) : variable(var), val(std::move(value)), dtype(dtype)
		{
			if (dtype == NULL)
				dtype = dtypes[variable];
		}

		llvm::Value *codegen()
		{
			if (variables[variable] == NULL)
			{
				if (dtype == NULL)
				{
					std::cout << "Data type for variable " << variable << " is not defined yet!" << endl;
					errored = true;
					return NULL;
				}
				variables[variable] = builder->CreateAlloca(dtype, nullptr, variable);
				dtypes[variable] = dtype;
			}
			llvm::Value* endres = val->codegen(); 
			if(endres->getType()->getTypeID() == llvm::Type::getInt8PtrTy(*ctxt)->getTypeID()){//&& !isConst //TBI
				llvm::FunctionCallee strlenfunc = GlobalVarsAndFunctions->getOrInsertFunction("strlen", llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(*ctxt), llvm::PointerType::get(llvm::Type::getInt8Ty(*ctxt), false), false));
				llvm::FunctionCallee strcpyfunc = GlobalVarsAndFunctions->getOrInsertFunction("strcpy", llvm::FunctionType::get(llvm::PointerType::getInt8PtrTy(*ctxt), {llvm::PointerType::get(llvm::Type::getInt8Ty(*ctxt), false),  llvm::PointerType::get(llvm::Type::getInt8Ty(*ctxt), false)}, false));
				llvm::Value* strlen =  builder->CreateCall(strlenfunc, endres, "strconstlen");
				strlen = builder->CreateAdd(strlen, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctxt), 1), "addtmp"); 
				llvm::Value* args[] = {builder->CreateAlloca(llvm::ArrayType::getInt8Ty(*ctxt),strlen, "strconstcpy") , endres};
				endres = builder->CreateCall(strcpyfunc, args, "strcpytmp"); 
			}
			// if(endres->getType()->getTypeID() != dtypes[variable]->getTypeID())
			return builder->CreateStore(endres, variables[variable]);
		}
	};

	class IfExprAST : public ExprAST
	{
	public:
		std::vector<std::unique_ptr<ExprAST>> condition, body;
		IfExprAST(std::vector<std::unique_ptr<ExprAST>> cond, std::vector<std::unique_ptr<ExprAST>> bod) : condition(std::move(cond)), body(std::move(bod)) {}
		llvm::Value *codegen()
		{
			llvm::BasicBlock *start, *end;
			llvm::BasicBlock *glblend = llvm::BasicBlock::Create(*ctxt, "glblifend", currentFunction);
			int i = 0;
			for (i = 0; i < min(condition.size(), body.size()); i++)
			{
				start = llvm::BasicBlock::Create(*ctxt, "ifstart", currentFunction, glblend);
				end = llvm::BasicBlock::Create(*ctxt, "ifend", currentFunction, glblend);
				builder->CreateCondBr(condition[i]->codegen(), start, end);
				builder->SetInsertPoint(start);
				body[i]->codegen();
				builder->CreateBr(glblend);
				builder->SetInsertPoint(end);
			}

			while (i < body.size())
			{
				body[i]->codegen();
				i++;
			}
			builder->CreateBr(glblend);
			builder->SetInsertPoint(glblend);
			return glblend;
		}
	};

	class ForExprAST : public ExprAST
	{
	public:
		std::vector<std::unique_ptr<ExprAST>> prefix, postfix;
		std::unique_ptr<ExprAST> condition, body;

		ForExprAST(std::vector<std::unique_ptr<ExprAST>> &init,
				   std::unique_ptr<ExprAST> cond,
				   std::unique_ptr<ExprAST> bod,
				   std::vector<std::unique_ptr<ExprAST>> &edit) : prefix(std::move(init)), condition(std::move(cond)), body(std::move(bod)), postfix(std::move(edit)) {}

		// I have a feeling this function needs to be revamped.
		llvm::Value *codegen()
		{
			llvm::Value *retval;
			llvm::BasicBlock *start = llvm::BasicBlock::Create(*ctxt, "loopstart", currentFunction), *end = llvm::BasicBlock::Create(*ctxt, "loopend", currentFunction);
			for (int i = 0; i < prefix.size(); i++)
			{
				llvm::Value *startval = prefix[i]->codegen();
			}
			builder->CreateCondBr(condition->codegen(), start, end);
			builder->SetInsertPoint(start);
			body->codegen();
			for (int i = 0; i < postfix.size(); i++)
			{
				retval = postfix[i]->codegen();
			}
			builder->CreateCondBr(condition->codegen(), start, end);
			builder->SetInsertPoint(end);
			return retval;
		}
	};

	/** BinaryExprAST - Expression class for a binary operator.
	 * TODO: Add type casting here through the 'AS' operator; I.E.
	 * "x AS string"
	 * TODO: Fix type management with the codegen() function
	 */
	class BinaryExprAST : public ExprAST
	{
		string Op;
		unique_ptr<ExprAST> LHS, RHS;

	public:
		BinaryExprAST(string op, /* llvm::Type* type,*/ unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
			: Op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
		llvm::Value *codegen()
		{
			if (DEBUGGING)
				std::cout << Op << "( ";
			llvm::Value *L = LHS->codegen();
			if (DEBUGGING)
				std::cout << ", ";
			llvm::Value *R = RHS->codegen();
			if (DEBUGGING)
				std::cout << " )";

			if (L->getType()->getTypeID() != R->getType()->getTypeID())
			{
				llvm::Instruction::CastOps castop;
				llvm::Type *dest = NULL;
				if (L->getType()->getTypeID() == llvm::Type::FloatTyID || R->getType()->getTypeID() == llvm::Type::FloatTyID)
				{
					castop = llvm::Instruction::SIToFP;
					dest = llvm::Type::getFloatTy(*ctxt);
				}
				if (dest != NULL)
				{
					llvm::Value *v = (L->getType()->getTypeID() == dest->getTypeID()) ? R : L;
					if (L->getType()->getTypeID() == dest->getTypeID())
					{
						R = builder->CreateCast(castop, v, dest, "typeCastTmp");
					}
					else
					{
						L = builder->CreateCast(castop, v, dest, "typeCastTmp");
					}
				}
			}

			switch (Op[0])
			{
			case '+':
				if (L->getType() == llvm::Type::getFloatTy(*ctxt) || R->getType() == llvm::Type::getFloatTy(*ctxt))
					return builder->CreateFAdd(L, R, "addtmp");
				if (L->getType() == llvm::Type::getInt32Ty(*ctxt) || R->getType() == llvm::Type::getInt32Ty(*ctxt))
					return builder->CreateAdd(L, R, "addtmp");
				return builder->CreateAdd(L, R, "addtmp");
			case '-':
				if (L->getType() == llvm::Type::getFloatTy(*ctxt) || R->getType() == llvm::Type::getFloatTy(*ctxt))
					return builder->CreateFSub(L, R, "addtmp");
				if (L->getType() == llvm::Type::getInt32Ty(*ctxt) || R->getType() == llvm::Type::getInt32Ty(*ctxt))
					return builder->CreateSub(L, R, "addtmp");
				return builder->CreateFSub(L, R, "subtmp");
			case '*':
				if (L->getType() == llvm::Type::getFloatTy(*ctxt) || R->getType() == llvm::Type::getFloatTy(*ctxt))
					return builder->CreateFMul(L, R, "addtmp");
				if (L->getType() == llvm::Type::getInt32Ty(*ctxt) || R->getType() == llvm::Type::getInt32Ty(*ctxt))
					return builder->CreateMul(L, R, "addtmp");
				return builder->CreateFMul(L, R, "multmp");
			case '/':
					return builder->CreateFDiv(L, R, "divtmp");
			case '^': // x^y == 2^(y*log2(x)) //Find out how to do this in LLVM
				return builder->CreateFMul(L, R, "multmp");
			case '=':
				if (L->getType() == llvm::Type::getFloatTy(*ctxt) || R->getType() == llvm::Type::getFloatTy(*ctxt))
					return builder->CreateFCmpOEQ(L, R, "cmptmp");
				if (L->getType() == llvm::Type::getInt32Ty(*ctxt) || R->getType() == llvm::Type::getInt32Ty(*ctxt))
					return builder->CreateICmpEQ(L, R, "cmptmp");
				return builder->CreateFCmpOEQ(L, R, "cmptmp");
			case '>':
				if (L->getType() == llvm::Type::getFloatTy(*ctxt) || R->getType() == llvm::Type::getFloatTy(*ctxt))
					return builder->CreateFCmpOGT(L, R, "cmptmp");
				if (L->getType() == llvm::Type::getInt32Ty(*ctxt) || R->getType() == llvm::Type::getInt32Ty(*ctxt))
					return builder->CreateICmpSGT(L, R, "cmptmp");
				return builder->CreateFCmpOGT(L, R, "cmptmp");
			case '<':
				if (L->getType() == llvm::Type::getFloatTy(*ctxt) || R->getType() == llvm::Type::getFloatTy(*ctxt))
					return builder->CreateFCmpOLT(L, R, "cmptmp");
				if (L->getType() == llvm::Type::getInt32Ty(*ctxt) || R->getType() == llvm::Type::getInt32Ty(*ctxt))
					return builder->CreateICmpSLT(L, R, "cmptmp");
				return builder->CreateFCmpOLT(L, R, "cmptmp");
			case '&':
			case 'a':
				return builder->CreateLogicalAnd(L, R, "andtmp");
			case '|':
			case 'o':
				return builder->CreateLogicalOr(L, R, "ortmp");
			default:
				std::cout << "Error: Unknown operator" << Op;
				return NULL;
			}
		}
	};

	/**
	 * Represents a list of items in code, usually represented by a string such as "[1, 2, 3, 4, 5]" alongside an optional semicolon on the end
	 */
	class ListExprAST : public ExprAST
	{
	public:
		std::vector<std::unique_ptr<ExprAST>> Contents;
		ListExprAST(std::vector<std::unique_ptr<ExprAST>> &Args) : Contents(std::move(Args)) {}
		ListExprAST() {}

		llvm::Value *codegen()
		{
			llvm::Value *ret = NULL;
			if (DEBUGGING)
				std::cout << "[ ";
			for (auto i = Contents.begin(); i < Contents.end(); i++)
			{
				ret = i->get()->codegen();
				if (i != Contents.end() - 1 && DEBUGGING)
					std::cout << ", ";
			};
			if (DEBUGGING)
				std::cout << " ]";
			return ret;
		}
	};
	/**
	 * Class representing a print statement in the Abstract Syntax Tree (AST)
	 * Inherits from ExprAST base class
	 */
	class PrintStmtAST : public ExprAST
	{
	public:
		std::vector<std::unique_ptr<ExprAST>> Contents;
		bool isLine = false;
		PrintStmtAST(std::vector<std::unique_ptr<ExprAST>> &Args, bool isLn)
			: Contents(std::move(Args)), isLine(isLn) {}

		llvm::Value *codegen()
		{
			// Initialize values for the arguments and the types for the arguments
			std::vector<llvm::Value *> vals;
			std::string placeholder = "";
			for (auto &x : Contents)
			{
				llvm::Value *data = x->codegen();
				
				switch (data->getType()->getTypeID())
				{
				case (llvm::PointerType::PointerTyID):
					placeholder += "%s ";
					break;
				case (llvm::Type::TypeID::FloatTyID):
					data = builder->CreateCast(llvm::Instruction::CastOps::FPExt, data, llvm::Type::getDoubleTy(*ctxt)); 
					placeholder += "%f ";
					break;
				default:
					placeholder += "%d ";
				}
				if (data != NULL)
				{
					vals.push_back(data);
				}
			}
			// Create global string constant(s) for newline characters and the placeholder constant where needed.
			if (isLine)
			{
				llvm::Constant *newline = builder->CreateGlobalStringPtr("\n", "newlineStr");
				vals.push_back(newline);
				placeholder += "%s";
			}
			llvm::Constant *globalString = builder->CreateGlobalStringPtr(placeholder);
			vals.insert(vals.begin(), globalString);
			// Initialize a function with no body to refrence C std libraries
			llvm::FunctionCallee printfunc = GlobalVarsAndFunctions->getOrInsertFunction("printf",
				llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(*ctxt), llvm::PointerType::get(llvm::Type::getInt8Ty(*ctxt), false), true));
			return builder->CreateCall(printfunc, vals, "printftemp");
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
			llvm::Value *ret = NULL;
			for (int i = 0; i < Contents.size(); i++)
			{
				ret = Contents[i]->codegen();
				if (DEBUGGING)
					std::cout << endl;
			};
			return ret;
		}
	};

	/// CallExprAST - Expression class for function calls.
	class CallExprAST : public ExprAST
	{
		string Callee;
		vector<std::unique_ptr<ExprAST>> Args;

	public:
		CallExprAST(const string callee, vector<std::unique_ptr<ExprAST>> &Arg) : Callee(callee), Args(std::move(Arg))
		{
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

			return builder->CreateCall(CalleeF, ArgsV, "calltmp");
		}
	};

	/// PrototypeAST - This class represents the "prototype" for a function,
	/// which captures its name, and its argument names (thus implicitly the number
	/// of arguments the function takes).
	class PrototypeAST
	{
		string Name;
		llvm::Type *retType;
		vector<std::string> Args;
		vector<llvm::Type *> Argt;

	public:
		PrototypeAST(const std::string &name, std::vector<std::string> &args, std::vector<llvm::Type *> &argtypes, llvm::Type *ret)
			: Name(name), Args(std::move(args)), Argt(std::move(argtypes)), retType(ret) {}

		const std::string &getName() const { return Name; }
		llvm::Function *codegen()
		{
			llvm::FunctionType *FT =
				llvm::FunctionType::get(retType, Argt, false);

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
			if (DEBUGGING)
				std::cout << Proto->getName();
			llvm::Function *prevFunction = currentFunction;
			currentFunction = GlobalVarsAndFunctions->getFunction(Proto->getName());

			if (!currentFunction)
				currentFunction = Proto->codegen();

			if (!currentFunction)
			{
				currentFunction = prevFunction;
				return nullptr;
			}

			if (!currentFunction->empty())
			{
				std::cout << "Function Cannot be redefined" << endl;
				currentFunction = prevFunction;
				return (llvm::Function *)NULL;
			}

			llvm::BasicBlock *BB = llvm::BasicBlock::Create(*ctxt, "entry", currentFunction);
			builder->SetInsertPoint(BB);

			// Record the function arguments in the NamedValues map.
			for (auto &Arg : currentFunction->args())
			{
				llvm::Value *storedvar = builder->CreateAlloca(Arg.getType(), NULL, Arg.getName());
				builder->CreateStore(&Arg, storedvar);
				std::string name = std::string(Arg.getName());
				variables[name] = storedvar;
				dtypes[name] = Arg.getType();
			}
			if (
				llvm::Value *RetVal = Body->codegen())
			{
				// Validate the generated code, checking for consistency.
				verifyFunction(*currentFunction);
				if (DEBUGGING)
					std::cout << "//end of " << Proto->getName() << endl;
				// remove the arguments now that they're out of scope
				for (auto &Arg : currentFunction->args())
				{
					variables[std::string(Arg.getName())] = NULL;
					dtypes[std::string(Arg.getName())] = NULL;
				}
				currentFunction = prevFunction;
				return currentFunction;
			}
			else
				std::cout << "Error in body of function " << Proto->getName() << endl;
			currentFunction->eraseFromParent();
			currentFunction = prevFunction;
			return nullptr;
		}
	};

	void logError(string s, Token t)
	{
		errored = true;
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
	std::unique_ptr<FunctionAST> functionDecl(Stack<Token> &tokens, llvm::Type *dtype);

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

		if (tokens.peek() == SCONST)
		{
			Token s = tokens.next();
			return std::make_unique<StringExprAST>(s.lex);
		}
		else if (tokens.peek() == IDENT)
		{
			Token s = tokens.next();
			return std::make_unique<VariableExprAST>(s.lex);
		}
		else if (tokens.peek() == NUMCONST)
		{
			string x = tokens.peek().lex;
			if (std::find(x.begin(), x.end(), '.') == x.end())
			{
				return std::make_unique<NumberExprAST>(stoi(tokens.next().lex));
			}
			return std::make_unique<NumberExprAST>(stod(tokens.next().lex));
		}

		logError("Invalid term:", tokens.currentToken());
		return NULL;
	}

	/**
	 * @brief Parses a function call statement, returns std::move(term(tokens)) if it does not find one
	 * @param tokens
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> functionCallExpr(Stack<Token> &tokens)
	{
		Token t = tokens.next();
		if (t != IDENT || GlobalVarsAndFunctions->getFunction(t.lex) == NULL)
		{
			tokens.go_back();
			return std::move(term(tokens));
		}
		std::vector<std::unique_ptr<ExprAST>> params;
		if (tokens.next() != LPAREN)
		{
			logError("Expected an opening parethesis '(' here:", tokens.currentToken());
			return NULL;
		}
		if (tokens.peek() != RPAREN)
			do
			{
				std::unique_ptr<ExprAST> param = std::move(jimpilier::listExpr(tokens));
				if (param == NULL)
				{
					logError("Invalid parameter passed to function", t);
					return NULL;
				}
				params.push_back(std::move(param));
			} while (tokens.peek() == COMMA && tokens.next() == COMMA);
		if (tokens.next() != RPAREN)
		{
			logError("Expected a closing parethesis '(' here:", tokens.currentToken());
			return NULL;
		}
		return std::make_unique<CallExprAST>(t.lex, params);
	}

	std::unique_ptr<ExprAST> incDecExpr(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS;
		if (tokens.peek() == INCREMENT || tokens.peek() == DECREMENT)
		{
			Token t = tokens.next();
			std::string op = &t.lex[0];
			LHS = std::move(functionCallExpr(tokens));
			return std::make_unique<BinaryExprAST>(op, std::move(LHS), std::make_unique<NumberExprAST>(1));
		}
		LHS = std::move(functionCallExpr(tokens));
		if (LHS == NULL)
		{
			return NULL;
		}
		if (tokens.peek() == INCREMENT || tokens.peek() == DECREMENT)
		{
			Token t = tokens.next();
			std::string op = &t.lex[0];
			return std::make_unique<BinaryExprAST>(op, std::move(LHS), std::make_unique<NumberExprAST>(1));
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
			RHS = std::make_unique<BinaryExprAST>(t.lex, std::move(RHS), std::move(term(tokens)));
			if (!LHS)
			{
				logError("Error parsing a term in raisedToExpr ", t);
				return NULL;
			}
		}
		return std::make_unique<BinaryExprAST>(t.lex, std::move(LHS), std::move(RHS));
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
			RHS = std::make_unique<BinaryExprAST>(t.lex, std::move(RHS), std::move(raisedToExpr(tokens)));
			if (!LHS)
			{
				logError("Error parsing a term in multDivExpr ", t);
				return NULL;
			}
		}
		return std::make_unique<BinaryExprAST>(t.lex, std::move(LHS), std::move(RHS));
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
			RHS = std::make_unique<BinaryExprAST>(t.lex, std::move(RHS), std::move(multAndDivExpr(tokens)));
			if (!LHS)
			{
				logError("Error parsing a term in mathExpr", t);
				return NULL;
			}
		}
		return std::make_unique<BinaryExprAST>(t.lex, std::move(LHS), std::move(RHS));
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
			RHS = std::make_unique<BinaryExprAST>(t.lex, std::move(RHS), std::move(mathExpr(tokens)));
		}
		return std::make_unique<BinaryExprAST>(t.lex, std::move(LHS), std::move(RHS));
	}

	/**
	 * @brief checks for a basic <IDENT> (["=="|"!="] <IDENT>)* However,
	 * I am torn between enabling N-way comparison (I.E.: "x == y == z == a == b").
	 * No other programming languages that I know of include this, and I have a
	 * feeling it's for good reason... But at the same time, I want to add it as I
	 * feel that is extremely convienent in the situations it comes up in.
	 * TODO: Fix and/or statements
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
			RHS = std::make_unique<BinaryExprAST>(t.lex, std::move(RHS), std::move(greaterLessStmt(tokens)));
		}
		return std::make_unique<BinaryExprAST>(t.lex, std::move(LHS), std::move(RHS));
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
			RHS = std::make_unique<BinaryExprAST>(t.lex, std::move(RHS), std::move(compareStmt(tokens)));
		}
		return std::make_unique<BinaryExprAST>(t.lex, std::move(LHS), std::move(RHS));
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
			RHS = std::make_unique<BinaryExprAST>(t.lex, std::move(RHS), std::move(andStmt(tokens)));
		}
		return std::make_unique<BinaryExprAST>(t.lex, std::move(LHS), std::move(RHS));
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
			std::unique_ptr<ExprAST> x = std::move(logicStmt(tokens));
			if (tokens.peek() == SEMICOL)
				tokens.next();
			return x;
		}
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
		return NULL;
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
	llvm::Type *variableModStmt(Stack<Token> &tokens)
	{
		// clear variable modifier memory if possible
		// TBI
		Token t;
		while (((t = tokens.peek()).token >= CONST && t.token <= PROTECTED) || t.token >= INT)
		{
			t = tokens.next();
			if (t.token >= INT)
			{
				switch (t.token)
				{
				case INT:
					if (tokens.peek() == POINTER)
					{
						tokens.next();
						return llvm::Type::getInt32PtrTy(*ctxt);
						break;
					}
					return llvm::Type::getInt32Ty(*ctxt);
					break;
				case SHORT:
					if (tokens.peek() == POINTER)
					{
						tokens.next();
						return llvm::Type::getInt32PtrTy(*ctxt);
						break;
					}
					return llvm::Type::getInt32Ty(*ctxt);
					break;
				case LONG:
					if (tokens.peek() == POINTER)
					{
						tokens.next();
					}
					return llvm::Type::getInt128Ty(*ctxt);
					break;
				case FLOAT:
					if (tokens.peek() == POINTER)
					{
						tokens.next();
						return llvm::Type::getFloatPtrTy(*ctxt);
						break;
					}
					return llvm::Type::getFloatTy(*ctxt);
					break;
				case DOUBLE:
					if (tokens.peek() == POINTER)
					{
						tokens.next();
						return llvm::Type::getDoublePtrTy(*ctxt);
						break;
					}
					return llvm::Type::getDoubleTy(*ctxt);
					break;
				case STRING:
					if (tokens.peek() == POINTER)
					{
						tokens.next();
						return llvm::Type::getInt64PtrTy(*ctxt);
						break;
					}
					return llvm::Type::getInt8PtrTy(*ctxt);
					break;
				case BOOL:
					if (tokens.peek() == POINTER)
					{
						tokens.next();
						return llvm::Type::getInt1PtrTy(*ctxt);
						break;
					}
					return llvm::Type::getInt1Ty(*ctxt);
					break;
				case CHAR:
					if (tokens.peek() == POINTER)
					{
						tokens.next();
						return llvm::Type::getInt8PtrTy(*ctxt);
						break;
					}
					return llvm::Type::getInt8Ty(*ctxt);
					break;
				case BYTE:
					if (tokens.peek() == POINTER)
					{
						tokens.next();
						return llvm::Type::getInt8PtrTy(*ctxt);
						break;
					}
					return llvm::Type::getInt8Ty(*ctxt);
					break;
				}
			}
			// Operators.push_back(t) //Add this to the variable modifier memory
			// return variableModStmt(tokens);
		} //*/
		// std::unique_ptr<ExprAST> retval = std::move(assignStmt(tokens));
		logError("Error when parsing variable/function modifiers & return types", tokens.currentToken());
		return NULL;
	}

	/**
	 * @brief Takes in a name as an identifier, an equals sign,
	 * then calls the lowest precidence operation in the AST.
	 * @param tokens
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> assignStmt(Stack<Token> &tokens)
	{
		Token name = tokens.peek();
		if (name != IDENT)
		{
			logError("Expected identifier in place of this token:", tokens.peek());
			return NULL;
		}
		else if (dtypes[name.lex] == NULL)
		{
			logError("Unknown variable name:", tokens.peek());
			return NULL;
		}
		tokens.next();
		std::unique_ptr<ExprAST> value;
		switch (tokens.peek().token)
		{
		case (EQUALS):
			tokens.next();
			value = std::move(listExpr(tokens));
			break;
		case INCREMENT:
		case DECREMENT:
			tokens.go_back();
			value = std::move(incDecExpr(tokens));
			break;
		case LPAREN:
			tokens.go_back();
			return NULL;
		default:
			logError("Unexpected token found here:", tokens.peek());
			return NULL;
		}
		if (value == NULL)
		{
			return NULL;
		}
		return std::make_unique<AssignStmtAST>(name.lex, value, dtypes[name.lex]);
	}

	std::unique_ptr<ExprAST> declareStmt(Stack<Token> &tokens)
	{
		llvm::Type *dtype = variableModStmt(tokens);
		if (dtype == NULL)
		{
			return NULL;
		}
		Token name = tokens.peek();
		if (name != IDENT)
		{
			logError("Expected identifier for new variable in place of this token:", tokens.peek());
			return NULL;
		}
		tokens.next();
		std::unique_ptr<ExprAST> value = NULL;
		std::unique_ptr<FunctionAST> func = NULL;
		switch (tokens.peek().token)
		{
		case (EQUALS):
			tokens.next();
			value = std::move(listExpr(tokens));
			break;
		case INCREMENT:
		case DECREMENT:
			tokens.go_back();
			value = std::move(incDecExpr(tokens));
			break;
		case LPAREN:
			tokens.go_back();
			// Function declaration goes here:
			func = std::move(functionDecl(tokens, dtype));
			if (func == NULL)
				return NULL;
			func->codegen();
			return NULL;
		default:
			if (dtype == llvm::Type::getInt8PtrTy(*ctxt))
				value = std::make_unique<StringExprAST>("");
			else if (dtype == llvm::Type::getFloatTy(*ctxt))
				value = std::make_unique<NumberExprAST>(0.0f);
			else
				value = std::make_unique<NumberExprAST>(0);
		}
		if (value == NULL || dtype == NULL)
		{
			return NULL;
		}
		dtypes[name.lex] = dtype;
		return std::make_unique<AssignStmtAST>(name.lex, value, dtype);
	}

	std::unique_ptr<ExprAST> declareOrAssign(Stack<Token> &tokens)
	{
		if (tokens.peek() == IDENT)
			return std::move(assignStmt(tokens));
		return std::move(declareStmt(tokens));
	}

	/**
	 * @brief Parses the declaration for a function, from a stack of tokens,
	 * including the header prototype and argument list.
	 * @param tokens
	 * @return std::unique_ptr<FunctionAST>
	 */
	std::unique_ptr<FunctionAST> functionDecl(Stack<Token> &tokens, llvm::Type *dtype = NULL)
	{
		if (dtype == NULL) // try parsing dtypes, assuming the optional variable was simply not provided
			dtype = variableModStmt(tokens);
		if (dtype == NULL) // if it fails a second time assume it's invalid
			return NULL;
		Token name = tokens.next();
		dtypes[name.lex] = dtype;
		std::vector<std::string> argnames;
		std::vector<llvm::Type *> argtypes;
		if (tokens.next() != LPAREN)
		{
			logError("Expected parenthesis here:", tokens.currentToken());
			dtypes[name.lex] = NULL;
			return NULL;
		}
		if (tokens.peek().token >= INT)
			do
			{
				llvm::Type *dtype = jimpilier::variableModStmt(tokens);
				if (tokens.peek() != IDENT)
				{
					logError("Expected identifier here:", tokens.currentToken());
					dtypes[name.lex] = NULL;
					return NULL;
				}
				else if (dtype == NULL)
				{
					logError("Invalid identifier modifiers here:", tokens.currentToken());
					return NULL;
				}
				Token t = tokens.next();
				argnames.push_back(t.lex);
				argtypes.push_back(dtype);
				dtypes[t.lex] = dtype;
			} while (tokens.peek() == COMMA && tokens.next() == COMMA);
		if (tokens.peek() == RPAREN)
		{
			tokens.next();
			std::unique_ptr<PrototypeAST> proto = std::make_unique<PrototypeAST>(name.lex, argnames, argtypes, dtype);
			std::unique_ptr<ExprAST> body = std::move(codeBlockExpr(tokens));
			for (auto arg : argnames)
				dtypes[arg] = NULL;
			if (body == NULL)
			{
				logError("Error in the body of function:", name);
				dtypes[name.lex] = NULL;
				return NULL;
			}
			std::unique_ptr<FunctionAST> func = std::make_unique<FunctionAST>(std::move(proto), std::move(body));
			return func;
		}
		logError("Expected a closing parenthesis here:", tokens.currentToken());
		dtypes[name.lex] = NULL;
		return NULL;
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
		return NULL;
	}

	/**
	 * @brief Parses a for statement header, followed by a body statement/block
	 * TODO: Add support for do-while stmts
	 * TODO: Add support for while stmts
	 * @param tokens
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> forStmt(Stack<Token> &tokens)
	{
		std::vector<std::unique_ptr<ExprAST>> beginStmts, endStmts;
		std::unique_ptr<ExprAST> condition, body;
		if (tokens.next() != FOR)
		{
			logError("Somehow we ended up looking for a 'for' statement where there is none. wtf.", tokens.currentToken());
			return NULL;
		}
		do
		{
			std::unique_ptr<ExprAST> x = std::move(declareOrAssign(tokens));
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

		} while (tokens.peek() == ELSE && tokens.next() == ELSE);

		return std::make_unique<IfExprAST>(std::move(conds), std::move(bodies));
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
	std::unique_ptr<ExprAST> retStmt(Stack<Token> &tokens)
	{
		if (tokens.next() != RET)
		{
			logError("Somehow was looking for a return statement when there was no return. Wtf.", tokens.currentToken());
			return NULL;
		}
		std::unique_ptr<ExprAST> val = std::move(jimpilier::listExpr(tokens));
		if (val == NULL)
		{
			return NULL;
		}
		return std::make_unique<RetExprAST>(val);
	}

	std::unique_ptr<ExprAST> printStmt(Stack<Token> &tokens)
	{
		bool isline = tokens.peek() == PRINTLN;
		if (tokens.peek() != PRINT && tokens.peek() != PRINTLN)
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
			return std::move(declareStmt(tokens));
		case IDENT:
		{
			std::unique_ptr<ExprAST> retval = std::move(assignStmt(tokens));
			if (retval == NULL)
			{
				llvm::Function *functionExists = GlobalVarsAndFunctions->getFunction(tokens.peek().lex);
				if (!functionExists)
				{
					logError("Unknown function here:", tokens.peek());
					return NULL;
				}
				else
				{
					return std::move(functionCallExpr(tokens));
				}
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