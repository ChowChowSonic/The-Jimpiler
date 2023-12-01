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
	/*
	 * General Naming rule:
	 * "___Stmt" = anything that does more than it may appear on the back end
	 * "___Expr" = Anything that does what it can in a relatively straightforward way
	 */
	std::unique_ptr<llvm::LLVMContext> ctxt;
	std::unique_ptr<llvm::IRBuilder<>> builder;
	std::unique_ptr<llvm::Module> GlobalVarsAndFunctions;
	std::map<std::string, llvm::Value *> variables;
	std::map<std::string, llvm::Type *> structTypes;
	std::vector<std::string> importedFiles;
	llvm::Function *currentFunction;
	/**
	 * @brief In the event that a Break statement is called (I.E. in a for loop or switch statements),
	 * the compilier will automatically jump to the top llvm::BasicBlock in this stack
	 *
	 */
	std::stack<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>> escapeBlock;
	/* Strictly for testing purposes, not meant for releases*/
	const bool DEBUGGING = false;
	bool errored = false;
	/// ExprAST - Base class for all expression nodes.
	class ExprAST
	{
	public:
		virtual ~ExprAST(){};
		virtual llvm::Value *codegen(bool autoDeref = true) = 0;
	};

	/// NumberExprAST - Expression class for numeric literals like "1.0".
	class NumberExprAST : public ExprAST
	{
		double Val;
		bool isInt = false;
		bool isBool = false;

	public:
		NumberExprAST(double Val) : Val(Val) {}
		NumberExprAST(int Val) : Val(Val) { isInt = true; }
		NumberExprAST(bool Val) : Val(Val) { isBool = true; }
		llvm::Value *codegen(bool autoDeref = true)
		{
			if (DEBUGGING)
				std::cout << Val;
			if (isInt)
				return llvm::ConstantInt::get(*ctxt, llvm::APInt(32, static_cast<long>(Val), true));
			if (isBool)
				return llvm::ConstantInt::get(*ctxt, llvm::APInt(1, Val == 1, true));
			return llvm::ConstantFP::get(*ctxt, llvm::APFloat((float)Val));
		}
	};

	class StringExprAST : public ExprAST
	{
		std::string Val;

	public:
		StringExprAST(std::string val) : Val(val) {}
		llvm::Value *codegen(bool autoDeref = true)
		{
			if (DEBUGGING)
				std::cout << Val;
			// if(Val.length() > 1)
			return builder->CreateGlobalStringPtr(Val, "Sconst");
			// return llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctxt), llvm::APInt(8, Val[0]));
		}
	};

	/// VariableExprAST - Expression class for referencing a variable, like "a".
	class VariableExprAST : public ExprAST
	{
		string Name;

	public:
		VariableExprAST(const string &Name) : Name(Name) {}
		llvm::Value *codegen(bool autoDeref = true)
		{
			if (DEBUGGING)
				std::cout << Name;
			llvm::Value *V = variables[Name];
			if (!V)
			{
				std::cout << "Unknown variable name: " << Name << endl;
				return nullptr;
			}
			if (autoDeref)
				return builder->CreateLoad(V->getType()->getNonOpaquePointerElementType(), V, "loadtmp");
			return V;
		}
	};

	class DeclareExprAST : public ExprAST
	{
		const std::string name;
		llvm::Type *ty;
		int size;
		bool lateinit;

	public:
		DeclareExprAST(const std::string Name, llvm::Type *type, bool isLateInit = false, int ArrSize = 1) : name(Name), ty(type), size(ArrSize), lateinit(isLateInit) {}

		llvm::Value *codegen(bool autoDeref = true)
		{
			llvm::Value *sizeval = llvm::ConstantInt::getIntegerValue(llvm::Type::getInt64Ty(*ctxt), llvm::APInt(64, (long)size, false));
			variables.emplace(std::pair<std::string, llvm::Value *>(name, builder->CreateAlloca(ty, sizeval, name)));
			if (!lateinit)
				builder->CreateStore(llvm::ConstantAggregateZero::get(ty), variables[name]);
			return variables[name];
		}
	};
	// TODO: Add non-null dot operator using a question mark; "objptr?func()" == "if objptr != null (@objptr).func()"
	// TODO: Add implicit type casting (maybe)
	// TODO: Completely revamp data type system to be almost exclusively front-end
	// TODO: Find a better way to differentiate between char* and string (use structs maybe?)
	// TODO: Add objects (God help me)
	// TODO: Add arrays (God help me)
	// TODO: Add a way to put things on the heap
	// TODO: Add a way to delete things from the heap
	// TODO: Add other modifier keywords (volatile, extern, etc...)
	// TODO: Make "auto" keyword work like C/C++
	// TODO: Add pointer arithmatic
	// TODO: Make strings safer

	class IncDecExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> val;
		bool prefix, decrement;

	public:
		IncDecExprAST(bool isPrefix, bool isDecrement, std::unique_ptr<ExprAST> &uniary) : prefix(isPrefix), decrement(isDecrement), val(std::move(uniary)) {}
		llvm::Value *codegen(bool autoDeref = true)
		{
			llvm::Value *v = val->codegen(false);
			if (prefix)
			{
				llvm::Value *tmpval = builder->CreateLoad(v->getType()->getContainedType(0), v, "incOrDecDerefTemp");
				tmpval = builder->CreateAdd(tmpval, llvm::ConstantInt::get(tmpval->getType(), llvm::APInt(tmpval->getType()->getIntegerBitWidth(), decrement ? -1 : 1, true)));
				builder->CreateStore(tmpval, v);
				return tmpval;
			}
			else
			{
				llvm::Value *oldval = builder->CreateLoad(v->getType()->getContainedType(0), v, "incOrDecDerefTemp");
				llvm::Value *tmpval = builder->CreateAdd(oldval, llvm::ConstantInt::get(oldval->getType(), llvm::APInt(oldval->getType()->getIntegerBitWidth(), decrement ? -1 : 1, true)));
				builder->CreateStore(tmpval, v);
				return oldval;
			}
		}
	};

	class NotExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> val;

	public:
		NotExprAST(std::unique_ptr<ExprAST> Val) : val(std::move(Val)){};
		llvm::Value *codegen(bool autoDeref = true)
		{
			llvm::Value *v = val->codegen();
			if (v->getType()->getTypeID() == llvm::Type::IntegerTyID && v->getType()->getIntegerBitWidth() == 1)
				return builder->CreateNot(v, "negationtmp");
			if (v->getType()->isStructTy())
			{
				// TODO: Implement operator overloading
			}
			std::cout << "Error: you're trying to take a negation of a non-boolean value!" << endl;
			return NULL;
		}
	};

	class RefrenceExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> val;

	public:
		RefrenceExprAST(std::unique_ptr<ExprAST> &val) : val(std::move(val)){};
		llvm::Value *codegen(bool autoDeref = false)
		{
			return val->codegen(false);
		}
	};

	class DeRefrenceExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> val;

	public:
		DeRefrenceExprAST(std::unique_ptr<ExprAST> &val) : val(std::move(val)){};
		llvm::Value *codegen(bool autoDeref = true)
		{
			llvm::Value *v = val->codegen(autoDeref);
			if (v->getType()->isPointerTy())
				return builder->CreateLoad(v->getType()->getContainedType(0), v, "derefrencetmp");
			else
			{
				std::cout << "Attempt to derefrence non-pointer type found: Aborting..." << endl;
				return NULL;
			}
		}
	};

	class IndexExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> bas, offs;

	public:
		IndexExprAST(std::unique_ptr<ExprAST> &base, std::unique_ptr<ExprAST> &offset) : bas(std::move(base)), offs(std::move(offset)){};
		llvm::Value *codegen(bool autoDeref = true)
		{
			llvm::Value *bsval = bas->codegen(), *offv = offs->codegen();
			if (!bsval->getType()->isPointerTy())
			{
				std::cout << "Error: You tried to take an offset of a non-pointer, non-array type! " << endl
						  << "Make sure that if you say 'variable[0]' (or similar), the type of 'variable' is a pointer or array!" << endl;
				errored = true;
				return NULL;
			}
			else if (!offv->getType()->isIntegerTy())
			{
				std::cout << "Error when trying to take an offset: The value provided must be an integer. Cast it to an int if possible" << endl;
				errored = true;
				return NULL;
			}
			llvm::Value *indextmp = builder->CreateGEP(bsval->getType()->getContainedType(0), bsval, offv, "offsetval");
			if (autoDeref)
				return builder->CreateLoad(bsval->getType()->getNonOpaquePointerElementType(), indextmp, "loadtmp");
			return indextmp;
		}
	};
	class TypeCastExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> from;
		llvm::Type *to;

	public:
		TypeCastExprAST(std::unique_ptr<ExprAST> &fromval, llvm::Type *toVal) : from(std::move(fromval)), to(toVal){};

		llvm::Value *codegen(bool autoDeref = true)
		{
			llvm::Value *init = from->codegen();
			llvm::Instruction::CastOps op;
			switch (init->getType()->getTypeID())
			{
			case (llvm::Type::IntegerTyID):
				if (to->getTypeID() == llvm::Type::FloatTyID)
				{
					op = llvm::Instruction::CastOps::SIToFP;
				}
				else if (to->getTypeID() == llvm::Type::HalfTyID)
				{
					op = llvm::Instruction::CastOps::Trunc;
				}
				else if (to->getTypeID() == llvm::Type::IntegerTyID)
				{
					if (to->getIntegerBitWidth() == init->getType()->getIntegerBitWidth())
						return init;
					op = to->getIntegerBitWidth() > init->getType()->getIntegerBitWidth() ? llvm::Instruction::CastOps::SExt : llvm::Instruction::CastOps::Trunc;
				}
				else if (to->getTypeID() == llvm::Type::PointerTyID)
				{
					op = llvm::Instruction::CastOps::IntToPtr;
				}
				else
				{
					std::cout << "Attempted conversion failed: "
							  << "Provided data type cannot be cast to other primitive/struct, and no overloaded operator exists that would do that for us" << endl;
					errored = true;
				}
				break;
			case (llvm::Type::HalfTyID):
				if (to->getTypeID() == llvm::Type::FloatTyID)
				{
					op = llvm::Instruction::CastOps::SIToFP;
				}
				else if (to->getTypeID() == llvm::Type::IntegerTyID)
				{
					if (to->getIntegerBitWidth() == init->getType()->getIntegerBitWidth())
						return init;
					op = to->getIntegerBitWidth() > init->getType()->getIntegerBitWidth() ? llvm::Instruction::CastOps::SExt : llvm::Instruction::CastOps::Trunc;
				}
				else if (to->getTypeID() == llvm::Type::PointerTyID)
				{
					op = llvm::Instruction::CastOps::IntToPtr;
				}
				else
				{
					std::cout << "Attempted conversion failed: "
							  << "Provided data type cannot be cast to other primitive/struct, and no overloaded operator exists that would do that for us" << endl;
					errored = true;
				}
				break;
			case (llvm::Type::DoubleTyID):
				if (to->getTypeID() == llvm::Type::FloatTyID)
				{
					op = llvm::Instruction::CastOps::FPTrunc;
					break;
				}
				else if (to->getTypeID() == llvm::Type::DoubleTyID)
					return init;
			case (llvm::Type::FloatTyID):
				if (to->getTypeID() == llvm::Type::DoubleTyID)
					op = llvm::Instruction::CastOps::FPExt;
				else if (to->getTypeID() == llvm::Type::IntegerTyID)
					op = llvm::Instruction::CastOps::FPToSI;
				else
				{
					std::cout << "Attempted conversion failed: "
							  << "Provided data type cannot be cast to other primitive/struct, and no overloaded operator exists that would do that for us" << endl;
					errored = true;
				}
				break;
			case (llvm::Type::PointerTyID):
				if (to->getTypeID() == llvm::Type::IntegerTyID)
					op = llvm::Instruction::CastOps::PtrToInt;
				else
				{
					std::cout << "Attempted conversion failed: "
							  << "Provided data type cannot be cast to other primitive/struct, and no overloaded operator exists that would do that for us" << endl;
					errored = true;
				}
				break;
			}
			return builder->CreateCast(op, init, to);
		}
	};

	class IfExprAST : public ExprAST
	{
	public:
		std::vector<std::unique_ptr<ExprAST>> condition, body;
		IfExprAST(std::vector<std::unique_ptr<ExprAST>> cond, std::vector<std::unique_ptr<ExprAST>> bod) : condition(std::move(cond)), body(std::move(bod)) {}
		llvm::Value *codegen(bool autoDeref = true)
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

	class SwitchExprAST : public ExprAST
	{
	public:
		std::vector<std::unique_ptr<ExprAST>> condition, body;
		std::unique_ptr<ExprAST> comp;
		int def;
		bool autoBr;
		SwitchExprAST(std::unique_ptr<ExprAST> comparator, std::vector<std::unique_ptr<ExprAST>> cond, std::vector<std::unique_ptr<ExprAST>> bod, bool autoBreak, int defaultLoc = -1) : condition(std::move(cond)), body(std::move(bod)), comp(std::move(comparator)), autoBr(autoBreak), def(defaultLoc) {}
		llvm::Value *codegen(bool autoDeref = true)
		{
			std::vector<llvm::BasicBlock *> bodBlocks;
			llvm::BasicBlock *glblend = llvm::BasicBlock::Create(*ctxt, "glblswitchend", currentFunction), *lastbody = glblend, *isDefault;
			llvm::SwitchInst *val = builder->CreateSwitch(comp->codegen(), glblend, body.size());
			escapeBlock.push(std::pair<llvm::BasicBlock *, llvm::BasicBlock *>(glblend, lastbody));
			int i = body.size() - 1;
			do
			{
				llvm::BasicBlock *currentbody = llvm::BasicBlock::Create(*ctxt, "body", currentFunction, lastbody);
				bodBlocks.push_back(currentbody);
				builder->SetInsertPoint(currentbody);
				body[i]->codegen();
				builder->CreateBr(autoBr ? glblend : lastbody);
				if (condition[i] != NULL)
					val->addCase((llvm::ConstantInt *)condition[i]->codegen(), currentbody);
				lastbody = currentbody;
				i -= 1;
				escapeBlock.pop();
				escapeBlock.push(std::pair<llvm::BasicBlock *, llvm::BasicBlock *>(glblend, lastbody));
			} while (i >= 0);
			if (def >= 0)
				val->setDefaultDest(bodBlocks[bodBlocks.size() - 1 - def]);
			builder->SetInsertPoint(glblend);
			escapeBlock.pop();
			return val;
		}
	};
	// TODO: Get labels working if possible
	/**
	 * @brief represents a break statement with optional label.
	 * Simply creates a break to the most recent escapeblock, or returns a constantInt(0) if there is none
	 *
	 */
	class BreakExprAST : public ExprAST
	{

		string labelVal;

	public:
		BreakExprAST(string label = "") : labelVal(label) {}
		llvm::Value *codegen(bool autoDeref = true)
		{
			if (escapeBlock.empty())
				return llvm::ConstantInt::get(*ctxt, llvm::APInt(32, 0, true));
			// if (labelVal != "")
			return builder->CreateBr(escapeBlock.top().first);
		}
	};
	/**
	 * @brief represents a break statement with optional label.
	 * Simply creates a break to the most recent escapeblock, or returns a constantInt(0) if there is none
	 *
	 */
	class ContinueExprAST : public ExprAST
	{

		string labelVal;

	public:
		ContinueExprAST(string label = "") : labelVal(label) {}
		llvm::Value *codegen(bool autoDeref = true)
		{
			if (escapeBlock.empty())
				return llvm::ConstantInt::get(*ctxt, llvm::APInt(32, 0, true));
			// if (labelVal != "")
			return builder->CreateBr(escapeBlock.top().second);
		}
	};
	// TODO: Implement forEach statements
	/**
	 * @brief Represents a for loop in LLVM IR, currently does not support for each statements
	 *
	 */
	class ForExprAST : public ExprAST
	{
	public:
		std::vector<std::unique_ptr<ExprAST>> prefix, postfix;
		std::unique_ptr<ExprAST> condition, body;
		bool dowhile;

		ForExprAST(std::vector<std::unique_ptr<ExprAST>> &init,
				   std::unique_ptr<ExprAST> cond,
				   std::unique_ptr<ExprAST> bod,
				   std::vector<std::unique_ptr<ExprAST>> &edit, bool isdoWhile = false) : prefix(std::move(init)), condition(std::move(cond)), body(std::move(bod)), postfix(std::move(edit)), dowhile(isdoWhile) {}
		/**
		 * @brief Construct a ForExprAST object, but with no bound variable or modifier; I.E. A while loop rather than a for loop
		 *
		 * @param cond
		 * @param bod
		 */
		ForExprAST(std::unique_ptr<ExprAST> cond, std::unique_ptr<ExprAST> bod, bool isDoWhile = false) : condition(std::move(cond)), body(std::move(bod)), dowhile(isDoWhile) {}
		// I have a feeling this function needs to be revamped.
		llvm::Value *codegen(bool autoDeref = true)
		{
			llvm::Value *retval;
			llvm::BasicBlock *start = llvm::BasicBlock::Create(*ctxt, "loopstart", currentFunction), *end = llvm::BasicBlock::Create(*ctxt, "loopend", currentFunction);
			escapeBlock.push(std::pair<llvm::BasicBlock *, llvm::BasicBlock *>(end, start));
			for (int i = 0; i < prefix.size(); i++)
			{
				llvm::Value *startval = prefix[i]->codegen();
			}
			if (!dowhile)
			{
				builder->CreateCondBr(condition->codegen(), start, end);
			}
			else
			{
				builder->CreateBr(start);
			}
			builder->SetInsertPoint(start);
			body->codegen();
			for (int i = 0; i < postfix.size(); i++)
			{
				retval = postfix[i]->codegen();
			}
			builder->CreateCondBr(condition->codegen(), start, end);
			builder->SetInsertPoint(end);
			escapeBlock.pop();
			return retval;
		}
	};
	// TODO: Finish implementing list objects (Implement as a fat pointer struct)
	/**
	 * Represents a list of items in code, usually represented by a string such as "[1, 2, 3, 4, 5]" alongside an optional semicolon on the end
	 */
	class ListExprAST : public ExprAST
	{
	public:
		std::vector<std::unique_ptr<ExprAST>> Contents;
		ListExprAST(std::vector<std::unique_ptr<ExprAST>> &Args) : Contents(std::move(Args)) {}
		ListExprAST() {}

		llvm::Value *codegen(bool autoDeref = true)
		{
			llvm::Value *ret = NULL;
			if (DEBUGGING)
				std::cout << "[ ";
			for (auto i = Contents.end(); i < Contents.begin(); i--)
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

	class DebugPrintExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> val;
		int ln;

	public:
		DebugPrintExprAST(std::unique_ptr<ExprAST> printval, int line) : val(std::move(printval)), ln(line) {}

		llvm::Value *codegen(bool autoDeref = true)
		{
			llvm::Value *data = val->codegen();
			std::string placeholder = "Debug value (Line " + to_string(ln) + "): ";

			switch (data->getType()->getTypeID())
			{
			case (llvm::PointerType::PointerTyID):
				if (data->getType() == llvm::Type::getInt8PtrTy(*ctxt))
					placeholder += "%s\n";
				else
					placeholder += "%p\n";
				break;
			case (llvm::Type::TypeID::FloatTyID):
				data = builder->CreateCast(llvm::Instruction::CastOps::FPExt, data, llvm::Type::getDoubleTy(*ctxt));
				placeholder += "%f\n";
				break;
			case (llvm::Type::TypeID::IntegerTyID):
				if (data->getType()->getIntegerBitWidth() == 64)
				{
					placeholder += "%p\n";
				}
				else if (data->getType()->getIntegerBitWidth() == 16)
				{
					placeholder += "%hu\n";
				}
				else if (data->getType() == llvm::Type::getInt8Ty(*ctxt))
				{
					placeholder += "%c\n";
				}
				else
				{
					placeholder += "%d\n";
				}
				break;
			default:
				placeholder += "%p\n";
			}
			llvm::Constant *globalString = builder->CreateGlobalStringPtr(placeholder);
			// Initialize a function with no body to refrence C std libraries
			llvm::FunctionCallee printfunc = GlobalVarsAndFunctions->getOrInsertFunction("printf",
																						 llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(*ctxt), llvm::PointerType::get(llvm::Type::getInt8Ty(*ctxt), false), true));
			builder->CreateCall(printfunc, {globalString, data}, "printftemp");
			return data;
		}
	};

	/**
	 * @brief Represents a return statement for a function
	 */
	class RetStmtAST : public ExprAST
	{
		unique_ptr<ExprAST> ret;

	public:
		RetStmtAST(unique_ptr<ExprAST> &val) : ret(std::move(val)) {}
		llvm::Value *codegen(bool autoDeref = true)
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
		std::unique_ptr<ExprAST> lhs, rhs;

	public:
		AssignStmtAST(std::unique_ptr<ExprAST> &LHS, std::unique_ptr<ExprAST> &RHS)
			: lhs(std::move(LHS)), rhs(std::move(RHS))
		{
		}

		llvm::Value *codegen(bool autoDeref = true)
		{
			llvm::Value *lval, *rval;
			// std::cout << std::hex << LHS << RHS <<endl;
			lval = lhs->codegen(false);
			rval = rhs->codegen();
			builder->CreateStore(rval, lval);
			return rval;
		}
	};
	// TODO: Fix type management with the BinaryStmtAST::codegen() function
	/** BinaryStmtAST - Expression class for a binary operator.
	 *
	 * "x AS string"
	 *
	 */
	class BinaryStmtAST : public ExprAST
	{
		string Op;
		unique_ptr<ExprAST> LHS, RHS;

	public:
		BinaryStmtAST(string op, /* llvm::Type* type,*/ unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
			: Op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
		llvm::Value *codegen(bool autoDeref = true)
		{
			if (DEBUGGING)
				std::cout << Op << "( ";
			llvm::Value *L = LHS->codegen();
			if (DEBUGGING)
				std::cout << ", ";
			llvm::Value *R = RHS->codegen();

			if (DEBUGGING)
				std::cout << " )";

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
				if (L->getType() == llvm::Type::getInt1Ty(*ctxt) || R->getType() == llvm::Type::getInt1Ty(*ctxt))
					return builder->CreateICmpEQ(L, R, "cmptmp");
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
			case '!':
				if (L->getType() == llvm::Type::getFloatTy(*ctxt) || R->getType() == llvm::Type::getFloatTy(*ctxt))
					return builder->CreateFCmpONE(L, R, "cmptmp");
				if (L->getType() == llvm::Type::getInt32Ty(*ctxt) || R->getType() == llvm::Type::getInt32Ty(*ctxt))
					return builder->CreateICmpNE(L, R, "cmptmp");
				if (L->getType() == llvm::Type::getInt1Ty(*ctxt) || R->getType() == llvm::Type::getInt1Ty(*ctxt))
					return builder->CreateICmpNE(L, R, "cmptmp");
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

	// TODO: Finalize boolean support
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

		llvm::Value *codegen(bool autoDeref = true)
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
					if (data->getType() == llvm::Type::getInt8PtrTy(*ctxt))
						placeholder += "%s ";
					else
						placeholder += "%p ";
					break;
				case (llvm::Type::TypeID::FloatTyID):
					data = builder->CreateCast(llvm::Instruction::CastOps::FPExt, data, llvm::Type::getDoubleTy(*ctxt));
					placeholder += "%f ";
					break;
				case (llvm::Type::TypeID::IntegerTyID):
					if (data->getType()->getIntegerBitWidth() == 64)
					{
						placeholder += "%p ";
					}
					else if (data->getType()->getIntegerBitWidth() == 16)
					{
						placeholder += "%hu ";
					}
					else if (data->getType() == llvm::Type::getInt8Ty(*ctxt))
					{
						placeholder += "%c ";
					}
					else
					{
						placeholder += "%d ";
					}
					break;
				default:
					placeholder += "%p ";
				}
				if (data != NULL)
				{
					vals.push_back(data);
				}
			}
			// Create global string constant(s) for newline characters and the placeholder constant where needed.
			if (isLine)
				placeholder += "\n";
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

		CodeBlockAST() {}
		llvm::Value *codegen(bool autoDeref = true)
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
		llvm::Value *codegen(bool autoDeref = true)
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

	class ObjectHeaderExpr
	{
	public:
		std::string name;
		ObjectHeaderExpr(std::string nameval) : name(nameval) {}

		llvm::StructType *codegen(bool autoderef = false)
		{
			// TODO: This could cause bugs later. Find a better (non-bandaid) solution
			llvm::StructType *ty = llvm::StructType::getTypeByName(*ctxt, name);
			return ty == NULL ? llvm::StructType::create(*ctxt, NULL, name) : ty;
		}
	};
	class ConstructorExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> bod;
		std::pair<std::vector<std::string>, std::vector<llvm::Type *>> args;
		std::string objName;

	public:
		ConstructorExprAST(
			std::pair<std::vector<std::string>, std::vector<llvm::Type *>> argList,
			std::unique_ptr<ExprAST> &body,
			std::string objName) : bod(std::move(body)), args(argList), objName(objName){};

		llvm::Value *codegen(bool autoderef = false)
		{
			llvm::Type *retType = llvm::StructType::getTypeByName(*ctxt, objName);
			args.first.emplace(args.first.begin(), "this");
			args.second.emplace(args.second.begin(), retType);

			llvm::FunctionType *FT =
				llvm::FunctionType::get(retType, args.second, false);

			llvm::Function * thisfunc =
				llvm::Function::Create(FT, llvm::Function::ExternalLinkage, objName+"_constructor", GlobalVarsAndFunctions.get());
			unsigned Idx = 0;
			for (auto &Arg : thisfunc->args())
				Arg.setName(args.first[Idx++]);

			llvm::Function *lastfunc = currentFunction;
			currentFunction = thisfunc;
			llvm::BasicBlock *entry = llvm::BasicBlock::Create(*ctxt, "entry", thisfunc);

			builder->SetInsertPoint(entry);
			if (
				llvm::Value *RetVal = bod->codegen())
			{
				builder->CreateRet(currentFunction->args().begin()); 
				// Validate the generated code, checking for consistency.
				verifyFunction(*currentFunction);
				if (DEBUGGING)
					std::cout << "//end of " << objName << "'s constructor" << endl;
				// remove the arguments now that they're out of scope
				for (auto &Arg : currentFunction->args())
				{
					variables[std::string(Arg.getName())] = NULL;
					// dtypes[std::string(Arg.getName())] = NULL;
				}
				currentFunction = lastfunc;
				return currentFunction;
			}
			else
				std::cout << "Error in body of function constructor for " << objName << endl;
			currentFunction->eraseFromParent();
			currentFunction = lastfunc;
			return nullptr;
		}
	};

	class ObjectExprAST : public ExprAST
	{
		ObjectHeaderExpr base;
		std::vector<std::pair<std::string, llvm::Type *>> vars;
		std::vector<std::unique_ptr<ExprAST>> functions;

	public:
		ObjectExprAST(ObjectHeaderExpr name,
					  std::vector<std::pair<std::string, llvm::Type *>> &varArg,
					  std::vector<std::unique_ptr<ExprAST>> &funcList) : base(name), vars(std::move(varArg)), functions(std::move(funcList)){};

		llvm::Value *codegen(bool autoDeref = true)
		{
			llvm::StructType *ty = base.codegen();
			std::vector<llvm::Type *> types;
			for (auto x : vars)
			{
				types.push_back(x.second);
				// Do something here to keep track of member positions later
			}
			ty->setBody(types);
			structTypes[(std::string)ty->getName()] = ty;
			for (auto &func : functions)
			{
				func->codegen();
			}
			return NULL;
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
		llvm::Function *codegen(bool autoDeref = true)
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
	class FunctionAST : public ExprAST
	{
		std::unique_ptr<PrototypeAST> Proto;
		std::unique_ptr<ExprAST> Body;

	public:
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
					std::unique_ptr<ExprAST> Body)
			: Proto(std::move(Proto)), Body(std::move(Body)) {}

		llvm::Value *codegen(bool autoDeref = true)
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

			// Record the function arguments in the Named Values map.
			for (auto &Arg : currentFunction->args())
			{
				llvm::Value *storedvar = builder->CreateAlloca(Arg.getType(), NULL, Arg.getName());
				builder->CreateStore(&Arg, storedvar);
				std::string name = std::string(Arg.getName());
				variables[name] = storedvar;
				// dtypes[name] = Arg.getType();
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
					// dtypes[std::string(Arg.getName())] = NULL;
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

	// <-- BEGINNING OF UTILITY FUNCTIONS -->

	void logError(string s, Token t)
	{
		errored = true;
		std::cout << s << ' ' << t.toString() << endl;
	}
	// <-- BEGINNING OF AST GENERATING FUNCTIONS -->

	std::unique_ptr<ExprAST> analyzeFile(string fileDir);
	std::unique_ptr<ExprAST> getValidStmt(Stack<Token> &tokens);
	std::unique_ptr<ExprAST> debugPrintStmt(Stack<Token> &tokens);
	llvm::Type *variableTypeStmt(Stack<Token> &tokens);
	std::pair<std::vector<std::string>, std::vector<llvm::Type *>> functionArgList(Stack<Token> &tokens);

	std::unique_ptr<ExprAST> import(Stack<Token> &tokens)
	{
		if (tokens.next() != IMPORT)
			return NULL;
		Token t;
		std::unique_ptr<ExprAST> b = std::make_unique<NumberExprAST>(1);
		while (!errored && (t = tokens.next()).token != SEMICOL && (t.token == IDENT || t.token == COMMA) && b)
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
	std::map<KeyToken, bool> variableModStmt(Stack<Token> &tokens);
	llvm::Type *variableTypeStmt(Stack<Token> &tokens);
	std::unique_ptr<ExprAST> logicStmt(Stack<Token> &tokens);
	std::unique_ptr<ExprAST> mathExpr(Stack<Token> &tokens);
	std::unique_ptr<ExprAST> listExpr(Stack<Token> &tokens);
	std::unique_ptr<ExprAST> assignStmt(Stack<Token> &tokens);
	std::unique_ptr<FunctionAST> functionDecl(Stack<Token> &tokens, llvm::Type *dtype, std::string name);
	// TODO: Holy Fuckles It's Knuckles I need to revamp this function entirely.
	/**
	 * @brief using EBNF notation, a term is roughly defined as:
	 *   <term> = (["+"|"-"]["++"|"--"] | "->" | "@")<IDENT>["++"|"--"] | "true" | "NULL" | "(" <expr> ")";
	 * However the ending [++|--] are only allowed if there is no (++|--) at the beginning of the term.
	 *
	 * @param tokens
	 * @return true if the syntax is valid
	 * @return NULL if syntax is not valid
	 */
	std::unique_ptr<ExprAST> term(Stack<Token> &tokens)
	{
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
		return std::make_unique<CallExprAST>(t.lex, params);
	}

	std::unique_ptr<ExprAST> indexExpr(Stack<Token> &tokens, std::unique_ptr<ExprAST> base = NULL)
	{
		if (base == NULL)
			base = std::move(functionCallExpr(tokens));
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
			return std::move(indexExpr(tokens, std::move(thisval)));
		}
		else
			return std::make_unique<IndexExprAST>(base, index);
	}

	std::unique_ptr<ExprAST> pointerToExpr(Stack<Token> &tokens)
	{
		if (tokens.peek() != POINTERTO)
		{
			return std::move(indexExpr(tokens));
		}
		tokens.next();
		std::unique_ptr<ExprAST> refval;
		refval = std::move(indexExpr(tokens));
		return std::make_unique<RefrenceExprAST>(refval);
	}

	std::unique_ptr<ExprAST> valueAtExpr(Stack<Token> &tokens)
	{
		if (tokens.peek() != REFRENCETO)
		{
			return std::move(pointerToExpr(tokens));
		}
		tokens.next();
		std::unique_ptr<ExprAST> drefval;
		if (tokens.peek() == REFRENCETO)
		{
			drefval = std::move(valueAtExpr(tokens));
		}
		else
		{
			drefval = std::move(pointerToExpr(tokens));
		}
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

	std::unique_ptr<ExprAST> typeAsExpr(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> convertee = std::move(notExpr(tokens));
		if (tokens.peek() != AS)
		{
			return convertee;
		}
		tokens.next();
		llvm::Type *toconv = variableTypeStmt(tokens);
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
			t = tokens.next();
			RHS = std::make_unique<BinaryStmtAST>(t.lex, std::move(RHS), std::move(term(tokens)));
			if (!LHS)
			{
				logError("Error parsing a term in raisedToExpr ", t);
				return NULL;
			}
		}
		return std::make_unique<BinaryStmtAST>(t.lex, std::move(LHS), std::move(RHS));
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
			RHS = std::make_unique<BinaryStmtAST>(t.lex, std::move(RHS), std::move(raisedToExpr(tokens)));
			if (!LHS)
			{
				logError("Error parsing a term in multDivExpr ", t);
				return NULL;
			}
		}
		return std::make_unique<BinaryStmtAST>(t.lex, std::move(LHS), std::move(RHS));
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
			RHS = std::make_unique<BinaryStmtAST>(t.lex, std::move(RHS), std::move(multAndDivExpr(tokens)));
			if (!LHS)
			{
				logError("Error parsing a term in mathExpr", t);
				return NULL;
			}
		}
		return std::make_unique<BinaryStmtAST>(t.lex, std::move(LHS), std::move(RHS));
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
			tokens.next();
			RHS = std::make_unique<BinaryStmtAST>(t.lex, std::move(RHS), std::move(mathExpr(tokens)));
		}
		return std::make_unique<BinaryStmtAST>(t.lex, std::move(LHS), std::move(RHS));
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
			tokens.next();
			RHS = std::make_unique<BinaryStmtAST>(t.lex, std::move(RHS), std::move(greaterLessStmt(tokens)));
		}
		return std::make_unique<BinaryStmtAST>(t.lex, std::move(LHS), std::move(RHS));
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

	// TODO: Get this function off the ground
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
		std::pair<std::vector<std::string>, std::vector<llvm::Type *>> args = functionArgList(tokens);
		t = tokens.next();
		if (t != RPAREN)
		{
			tokens.go_back(1);
			logError("Expecting a right parenthesis at this token:", t);
			return NULL;
		}
		std::unique_ptr<ExprAST> body = std::move(codeBlockExpr(tokens));
		return std::make_unique<ConstructorExprAST>(args, body, obj);
	}
	// TODO: Get this fuction off the ground
	/**
	 * @brief Parses a destructor for a class
	 *
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

	// TODO: Get this function off the ground
	/**
	 * @brief Parses an object/class blueprint
	 *
	 *
	 * @param tokens
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> obj(Stack<Token> &tokens)
	{
		std::vector<std::pair<std::string, llvm::Type *>> objVars;
		std::vector<std::unique_ptr<ExprAST>> objFunctions;
		if (tokens.peek() == OBJECT)
			tokens.next();
		if (tokens.peek() != IDENT)
		{
			logError("Expected object name at this token:", tokens.peek());
			return NULL;
		}
		ObjectHeaderExpr objName(tokens.next().lex);
		if (tokens.next() != OPENCURL)
		{
			logError("Curly braces are required for object declarations. Please put a brace before this token:", tokens.currentToken());
			return NULL;
		}
		while (tokens.peek() != CLOSECURL)
		{
			if (tokens.peek() == CONSTRUCTOR)
			{
				std::unique_ptr<ExprAST> constructorast = std::move(construct(tokens, objName.name));
				objFunctions.push_back(std::move(constructorast));
				continue;
			}
			llvm::Type *ty = variableTypeStmt(tokens);
			if (ty == NULL)
			{
				logError("Invalid variable declaration found here:", tokens.peek());
				return NULL;
			}
			Token name = tokens.peek();
			if (name != IDENT)
			{
				logError("Expected identifier at this token:", name);
				return NULL;
			}
			tokens.next();
			if (tokens.peek() == SEMICOL)
				tokens.next();
			objVars.push_back(std::pair<std::string, llvm::Type *>(name.lex, ty));
		}
		tokens.next();
		return std::make_unique<ObjectExprAST>(objName, objVars, objFunctions);
	}
	llvm::Type *variableTypeStmt(Stack<Token> &tokens)
	{
		Token t = tokens.peek();
		llvm::Type *type = NULL;
		if (t.token >= INT || structTypes[t.lex] != NULL)
		{
			t = tokens.next();
			switch (t.token)
			{
			case INT:
				type = llvm::Type::getInt32Ty(*ctxt);
				break;
			case SHORT:
				type = llvm::Type::getInt16Ty(*ctxt);
				break;
			case LONG:
				type = llvm::Type::getInt64Ty(*ctxt);
				break;
			case FLOAT:
				type = llvm::Type::getFloatTy(*ctxt);
				break;
			case DOUBLE:
				type = llvm::Type::getDoubleTy(*ctxt);
				break;
			case STRING:
				type = llvm::Type::getInt8PtrTy(*ctxt);
				break;
			case BOOL:
				type = llvm::Type::getInt1Ty(*ctxt);
				break;
			case CHAR:
			case BYTE:
				type = llvm::Type::getInt8Ty(*ctxt);
				break;
			default:
				type = structTypes[t.lex];
				if (type == NULL)
					return NULL;
			}
			while (tokens.peek() == POINTER)
			{
				tokens.next();
				type = type->getPointerTo();
			}
		}
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

	std::unique_ptr<ExprAST> declareStmt(Stack<Token> &tokens)
	{
		std::map<KeyToken, bool> mods = variableModStmt(tokens);
		llvm::Type *dtype = variableTypeStmt(tokens);
		if (dtype == NULL)
		{
			return std::move(listExpr(tokens));
		}
		Token name = tokens.peek();
		if (name != IDENT)
		{
			logError("Expected identifier for new variable in place of this token:", tokens.peek());
			return NULL;
		}
		tokens.next();
		if (tokens.peek() == LPAREN)
		{
			return std::move(functionDecl(tokens, dtype, name.lex));
		}
		return std::make_unique<DeclareExprAST>(name.lex, dtype);
	}

	/**
	 * @brief Takes in a name as an identifier, an equals sign,
	 * then calls the lowest precidence operation in the AST.
	 * @param tokens
	 * @return std::unique_ptr<ExprAST>
	 */
	std::unique_ptr<ExprAST> assignStmt(Stack<Token> &tokens)
	{
		std::unique_ptr<ExprAST> LHS = std::move(declareStmt(tokens)), RHS = NULL;
		if (tokens.peek() == EQUALS)
		{
			tokens.next();
			RHS = std::move(assignStmt(tokens));
		}
		else
			return LHS;

		return std::make_unique<AssignStmtAST>(LHS, RHS);
	}

	std::pair<std::vector<std::string>, std::vector<llvm::Type *>> functionArgList(Stack<Token> &tokens)
	{
		std::vector<std::string> argnames;
		std::vector<llvm::Type *> argtypes;
		do
		{
			llvm::Type *dtype = jimpilier::variableTypeStmt(tokens);
			if (dtype == NULL)
			{
				break;
			}
			else if (tokens.peek() != IDENT)
			{
				logError("Expected identifier after variable type here:", tokens.currentToken());
				break;
			}
			Token t = tokens.next();
			argnames.push_back(t.lex);
			argtypes.push_back(dtype);
		} while (!errored && tokens.peek() == COMMA && tokens.next() == COMMA);
		return std::pair<std::vector<std::string>, std::vector<llvm::Type *>>(argnames, argtypes);
	}

	/**
	 * @brief Parses the declaration for a function, from a stack of tokens,
	 * including the header prototype and argument list.
	 * @param tokens
	 * @return std::unique_ptr<FunctionAST>
	 */
	std::unique_ptr<FunctionAST> functionDecl(Stack<Token> &tokens, llvm::Type *dtype, std::string name)
	{

		std::vector<std::string> argnames;
		std::vector<llvm::Type *> argtypes;

		tokens.next();
		if (tokens.peek().token >= INT)
		{
			std::pair<std::vector<std::string>, std::vector<llvm::Type *>> retval = functionArgList(tokens);
			argnames = retval.first;
			argtypes = retval.second;
		}

		if (tokens.peek() == RPAREN)
		{
			tokens.next();
			std::unique_ptr<PrototypeAST> proto = std::make_unique<PrototypeAST>(name, argnames, argtypes, dtype);
			std::unique_ptr<ExprAST> body = std::move(codeBlockExpr(tokens));
			for (auto arg : argnames)
				variables[arg] = NULL;
			if (body == NULL)
			{
				return NULL;
			}
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
			std::unique_ptr<ExprAST> x = std::move(assignStmt(tokens));
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
		std::unique_ptr<ExprAST> val = std::move(jimpilier::listExpr(tokens));
		if (val == NULL)
		{
			return NULL;
		}
		return std::make_unique<RetStmtAST>(val);
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
		default:
			return std::move(assignStmt(tokens));
		}
		logError("Returning null on error?", tokens.currentToken());
		return NULL;
	}
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

		ifstream file;
		file.open(fileDir);
		if (!file)
		{
			std::cout << "File '" << fileDir << "' does not exist" << std::endl;
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
	// TODO: Fix this whole damn function oh Lord help me
	/**
	 * @brief Soon-to-be depreciated
	 *
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
