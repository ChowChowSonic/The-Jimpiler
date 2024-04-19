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
#include "./AliasManager.cpp"
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
	std::unique_ptr<llvm::DataLayout> DataLayout;
	AliasManager AliasMgr;
	std::vector<std::string> importedFiles;
	llvm::Function *currentFunction;
	llvm::Function *STATIC = nullptr;
	/**
	 * @brief In the event that a Break statement is called (I.E. in a for loop or switch statements),
	 * the compilier will automatically jump to the top llvm::BasicBlock in this stack
	 *
	 */
	std::stack<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>> escapeBlock;
	std::map<llvm::Type*, std::map<std::string, std::map<llvm::Type*, llvm::Function*>>> operators; 
	/* Strictly for testing purposes, not meant for releases*/
	const bool DEBUGGING = false;
	bool errored = false;

	class TypeExpr
	{
	public:
		virtual ~TypeExpr(){};
		virtual llvm::Type *codegen(bool testforval = false) = 0;
		virtual std::unique_ptr<TypeExpr> clone() = 0;
	};

	class Variable
	{
	public:
		std::string name;
		std::unique_ptr<TypeExpr> ty;
		Variable(const std::string &ident, std::unique_ptr<TypeExpr> &type) : name(ident), ty(std::move(type)) {}
		Variable() : name(""){
			ty = NULL; 
		}
	};

	class DoubleTypeExpr : public TypeExpr
	{
	public:
		DoubleTypeExpr() {}
		llvm::Type *codegen(bool testforval = false) { return llvm::Type::getDoubleTy(*ctxt); };
		std::unique_ptr<TypeExpr> clone()
		{
			return std::unique_ptr<TypeExpr>(new DoubleTypeExpr());
		}
	};

	class FloatTypeExpr : public TypeExpr
	{
	public:
		FloatTypeExpr() {}
		llvm::Type *codegen(bool testforval = false) { return llvm::Type::getFloatTy(*ctxt); };
		std::unique_ptr<TypeExpr> clone()
		{
			return std::unique_ptr<TypeExpr>(new FloatTypeExpr());
		}
	};

	class LongTypeExpr : public TypeExpr
	{
	public:
		LongTypeExpr() {}
		llvm::Type *codegen(bool testforval = false) { return llvm::Type::getInt64Ty(*ctxt); };
		std::unique_ptr<TypeExpr> clone()
		{
			return std::unique_ptr<TypeExpr>(new LongTypeExpr());
		}
	};

	class IntTypeExpr : public TypeExpr
	{
	public:
		IntTypeExpr() {}
		llvm::Type *codegen(bool testforval = false) { return llvm::Type::getInt32Ty(*ctxt); };
		std::unique_ptr<TypeExpr> clone()
		{
			return std::unique_ptr<TypeExpr>(new IntTypeExpr());
		}
	};

	class ShortTypeExpr : public TypeExpr
	{
	public:
		ShortTypeExpr() {}
		llvm::Type *codegen(bool testforval = false) { return llvm::Type::getInt16Ty(*ctxt); };
		std::unique_ptr<TypeExpr> clone()
		{
			return std::unique_ptr<TypeExpr>(new ShortTypeExpr());
		}
	};

	class ByteTypeExpr : public TypeExpr
	{
	public:
		ByteTypeExpr() {}
		llvm::Type *codegen(bool testforval = false) { return llvm::Type::getInt8Ty(*ctxt); };
		std::unique_ptr<TypeExpr> clone()
		{
			return std::unique_ptr<TypeExpr>(new ByteTypeExpr());
		}
	};

	class BoolTypeExpr : public TypeExpr
	{
	public:
		BoolTypeExpr() {}
		llvm::Type *codegen(bool testforval = false) { return llvm::Type::getInt1Ty(*ctxt); };
		std::unique_ptr<TypeExpr> clone()
		{
			return std::unique_ptr<TypeExpr>(new BoolTypeExpr());
		}
	};

	class TemplateTypeExpr : public TypeExpr
	{
		std::string name;

	public:
		TemplateTypeExpr(const std::string name) : name(name) {}
		llvm::Type *codegen(bool testforval = false) { return llvm::StructType::create(name, {}); };
		std::unique_ptr<TypeExpr> clone()
		{
			return std::unique_ptr<TypeExpr>(new TemplateTypeExpr(name));
		}
	};

	class StructTypeExpr : public TypeExpr
	{
		std::string name;

	public:
		StructTypeExpr(const std::string &structname) : name(structname) {}
		llvm::Type *codegen(bool testforval = false)
		{
			llvm::Type *ty = AliasMgr(name);
			if (!testforval && ty == NULL)
			{
				std::cout << "Unknown object of name: " << name << endl;
				errored = true;
				return NULL;
			}
			return ty;
		}
		std::unique_ptr<TypeExpr> clone()
		{
			return std::unique_ptr<TypeExpr>(new StructTypeExpr(name));
		}
	};

	class PointerToTypeExpr : public TypeExpr
	{
		std::unique_ptr<TypeExpr> ty;

	public:
		PointerToTypeExpr(std::unique_ptr<TypeExpr> &type) : ty(std::move(type)) {}
		llvm::Type *codegen(bool testforval = false)
		{
			llvm::Type *t = ty->codegen();
			return t == NULL ? NULL : t->getPointerTo();
		}
		std::unique_ptr<TypeExpr> clone()
		{
			std::unique_ptr<TypeExpr> encasedType = std::move(ty->clone());
			return std::make_unique<PointerToTypeExpr>(encasedType);
		}
	};

	/// ExprAST - Base class for all expression nodes.
	class ExprAST
	{
	public:
		virtual ~ExprAST(){};
		virtual llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL) = 0;
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
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
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
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
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
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			if (DEBUGGING)
				std::cout << Name;
			llvm::Value *V = AliasMgr[Name];
			if (!V)
			{
				std::cout << "Unknown variable name: " << Name << endl;
				return nullptr;
			}
			if (autoDeref && currentFunction != NULL)
				return builder->CreateLoad(V->getType()->getNonOpaquePointerElementType(), V, "loadtmp");
			return V;
		}
	};

	class DeclareExprAST : public ExprAST
	{
		const std::string name;
		std::unique_ptr<TypeExpr> type;
		int size;
		bool lateinit;

	public:
		DeclareExprAST(const std::string Name, std::unique_ptr<TypeExpr> type, bool isLateInit = false, int ArrSize = 1) : name(Name), type(std::move(type)), size(ArrSize), lateinit(isLateInit) {}

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			llvm::Type *ty = this->type->codegen();
			llvm::Value *sizeval = llvm::ConstantInt::getIntegerValue(llvm::Type::getInt64Ty(*ctxt), llvm::APInt(64, (long)size, false));
			if (AliasMgr[name] != NULL)
			{
				std::cout << "Warning: The variable '" << name << "' was already defined. Overwriting the previous value..." << endl;
				std::cout << "If this was not intentional, please make use of semicolons to better denote the end of each statement" << endl;
			}
			if (currentFunction == NULL || currentFunction == STATIC)
			{
				GlobalVarsAndFunctions->getOrInsertGlobal(name, ty);
				AliasMgr[name] = GlobalVarsAndFunctions->getNamedGlobal(name);
				GlobalVarsAndFunctions->getNamedGlobal(name)->setInitializer(llvm::ConstantAggregateZero::get(ty));
			}
			else
			{
				AliasMgr[name] = (llvm::Value *)builder->CreateAlloca(ty, sizeval, name);
			}
			if (!lateinit && currentFunction != NULL)
				builder->CreateStore(llvm::ConstantAggregateZero::get(ty), AliasMgr[name]);
			return AliasMgr[name];
		}
	};
	// TODO: Add non-null dot operator using a question mark; "objptr?func()" == "if objptr != null (@objptr).func()"
	// TODO: Add implicit type casting (maybe)
	// TODO: Completely revamp data type system to be almost exclusively front-end
	// TODO: Find a better way to differentiate between char* and string (use structs maybe?)
	// TODO: Add Template objects (God help me)
	// TODO: Add arrays (God help me)
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
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			llvm::Value *v = val->codegen(false);
			if (prefix)
			{
				llvm::Value *tmpval = builder->CreateLoad(v->getType()->getContainedType(0), v, "incOrDecDerefTemp");
				tmpval = builder->CreateAdd(tmpval, llvm::ConstantInt::get(tmpval->getType(), llvm::APInt(tmpval->getType()->getIntegerBitWidth(), decrement ? -1 : 1, true)), "incdectemp");
				builder->CreateStore(tmpval, v);
				return tmpval;
			}
			else
			{
				llvm::Value *oldval = builder->CreateLoad(v->getType()->getContainedType(0), v, "incOrDecDerefTemp");
				llvm::Value *tmpval = builder->CreateAdd(oldval, llvm::ConstantInt::get(oldval->getType(), llvm::APInt(oldval->getType()->getIntegerBitWidth(), decrement ? -1 : 1, true)), "incdectemp");
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
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
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
		llvm::Value *codegen(bool autoDeref = false, llvm::Value *other = NULL)
		{
			return val->codegen(false);
		}
	};

	class DeRefrenceExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> val;

	public:
		DeRefrenceExprAST(std::unique_ptr<ExprAST> &val) : val(std::move(val)){};
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			llvm::Value *v = val->codegen(autoDeref);
			if (v != NULL && v->getType()->isPointerTy())
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
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
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
		std::unique_ptr<TypeExpr> totype;

	public:
		TypeCastExprAST(std::unique_ptr<ExprAST> &fromval, std::unique_ptr<TypeExpr> &toVal) : from(std::move(fromval)), totype(std::move(toVal)){};

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			llvm::Type *to = this->totype->codegen();
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
				else if (to->getTypeID() == llvm::Type::PointerTyID)
					op = llvm::Instruction::CastOps::BitCast;
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
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
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
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
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
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
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
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			if (escapeBlock.empty())
				return llvm::ConstantInt::get(*ctxt, llvm::APInt(32, 0, true));
			// if (labelVal != "")
			return builder->CreateBr(escapeBlock.top().second);
		}
	};
	class SizeOfExprAST : public ExprAST
	{
		std::unique_ptr<TypeExpr> type = NULL;
		std::unique_ptr<ExprAST> target = NULL;

	public:
		SizeOfExprAST(std::unique_ptr<TypeExpr> &SizeOfTarget) : type(std::move(SizeOfTarget)) {}
		SizeOfExprAST(std::unique_ptr<ExprAST> &SizeOfTarget) : target(std::move(SizeOfTarget)) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			if (!type && !target)
			{
				errored = true;
				std::cout << "Invalid target for sizeof: The expression you're taking the size of doesn't do what you think it does" << endl;
				return NULL;
			}
			llvm::Type *ty = this->type == NULL ? target->codegen()->getType() : this->type->codegen();
			switch (ty->getTypeID())
			{
			case llvm::Type::IntegerTyID:
				return llvm::ConstantInt::getIntegerValue(llvm::Type::getInt32Ty(*ctxt), llvm::APInt(32, ty->getIntegerBitWidth() / 8));
			case llvm::Type::FloatTyID:
				return llvm::ConstantInt::getIntegerValue(llvm::Type::getInt32Ty(*ctxt), llvm::APInt(32, 4));
			case llvm::Type::PointerTyID:
			case llvm::Type::DoubleTyID:
				return llvm::ConstantInt::getIntegerValue(llvm::Type::getInt32Ty(*ctxt), llvm::APInt(32, 8));
			case llvm::Type::ArrayTyID:
				return llvm::ConstantInt::getIntegerValue(llvm::Type::getInt32Ty(*ctxt), llvm::APInt(32, DataLayout->getTypeAllocSize(ty) * ty->getArrayNumElements()));
			case llvm::Type::StructTyID:
				llvm::StructType *castedval = (llvm::StructType *)ty;
				return llvm::ConstantInt::getIntegerValue(llvm::Type::getInt32Ty(*ctxt), llvm::APInt(32, DataLayout->getStructLayout(castedval)->getSizeInBytes()));
			}
			return NULL;
		}
	};
	// TODO: Add constructors for the primitive types
	class HeapExprAST : public ExprAST
	{

	public:
		HeapExprAST() {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			// initalize calloc(i64, i64) as the primary ways to allocate heap memory
			GlobalVarsAndFunctions->getOrInsertFunction("calloc", llvm::FunctionType::get(
																	  llvm::Type::getInt8Ty(*ctxt)->getPointerTo(),
																	  {llvm::Type::getInt64Ty(*ctxt), llvm::Type::getInt64Ty(*ctxt)},
																	  false));

			llvm::Value *alloc = builder->CreateCall(GlobalVarsAndFunctions->getFunction("calloc"), {other, llvm::ConstantInt::getIntegerValue(llvm::Type::getInt64Ty(*ctxt), llvm::APInt(64, 1))}, "clearalloctmp");
			return alloc;
		}
	};

	class DeleteExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> val;

	public:
		DeleteExprAST(std::unique_ptr<ExprAST> &deleteme) : val(std::move(deleteme)) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			llvm::Value *freefunc = GlobalVarsAndFunctions->getOrInsertFunction("free", {llvm::Type::getInt8PtrTy(*ctxt)}, llvm::Type::getInt8PtrTy(*ctxt)).getCallee();
			llvm::Value *deletedthing = val->codegen(true);

			if (!deletedthing->getType()->isPointerTy())
			{
				std::cout << "Remember: Objects on the stack are accessed directly, you only need to delete pointers that point to the heap" << endl;
				std::cout << "You might've tried to delete an object (directly) by mistake rather than a pointer to that object" << endl;
				errored = true;
				return NULL;
			}

			deletedthing = builder->CreateBitCast(deletedthing, llvm::Type::getInt8PtrTy(*ctxt), "bitcasttmp");
			builder->CreateCall((llvm::Function *)freefunc, deletedthing, "freedValue");
			return NULL;
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
		 * @brief Construct a ForExprAST, but with no bound variable or modifier; I.E. A while loop rather than a for loop
		 *
		 * @param cond
		 * @param bod
		 */
		ForExprAST(std::unique_ptr<ExprAST> cond, std::unique_ptr<ExprAST> bod, bool isDoWhile = false) : condition(std::move(cond)), body(std::move(bod)), dowhile(isDoWhile) {}
		// I have a feeling this function needs to be revamped.
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
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

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
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

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
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
			case (llvm::Type::TypeID::DoubleTyID):
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
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
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

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			llvm::Value *lval, *rval;
			// std::cout << std::hex << LHS << RHS <<endl;
			lval = lhs->codegen(false);
			rval = rhs->codegen(true, lval);
			if (lval != NULL && rval != NULL)
			{
				if (currentFunction == NULL)
				{
					if (!llvm::isa<llvm::GlobalVariable>(lval))
					{
						errored = true;
						std::cout << "Error: Global variable not found " << lval->getName().str();
						return NULL;
					}
					else if (!llvm::isa<llvm::Constant>(rval) || lval->getType() != rval->getType()->getPointerTo())
					{
						errored = true;
						std::cout << "Error: Global variable " << lval->getName().str() << " is not being set to a constant value" << endl;
						return NULL;
					}
					llvm::dyn_cast<llvm::GlobalVariable>(lval)->setInitializer(llvm::dyn_cast<llvm::Constant>(rval));
				}
				else
				{
					if (lval->getType() != rval->getType()->getPointerTo())
					{
						std::cout << "Error when attempting to assign a value: The type of the right side (" << AliasMgr.getTypeName(lval->getType()) << ") does not match the left side (" << AliasMgr.getTypeName(rval->getType()) << ")." << endl;
						errored = true;
						return NULL;
					}
					builder->CreateStore(rval, lval);
				}
			}
			return rval;
		}
	};
	/**
	 * Handles == and != operators
	 */
	class CompareStmtAST : public ExprAST
	{
		unique_ptr<ExprAST> LHS, RHS;
		bool notval;

	public:
		CompareStmtAST(unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS, bool neq = false)
			: LHS(std::move(LHS)), RHS(std::move(RHS)), notval(neq) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			llvm::Value *lhs = LHS->codegen();
			llvm::Value *rhs = RHS->codegen();
			if (lhs->getType()->getTypeID() == rhs->getType()->getTypeID())
			{
				switch (lhs->getType()->getTypeID())
				{
				case llvm::Type::IntegerTyID:
				{
					llvm::Value *larger = lhs->getType()->getIntegerBitWidth() >= rhs->getType()->getIntegerBitWidth() ? lhs : rhs;
					lhs = builder->CreateSExtOrBitCast(lhs, larger->getType(), "signExtendTmp");
					rhs = builder->CreateSExtOrBitCast(rhs, larger->getType(), "signExtendTmp");
					return notval ? builder->CreateICmpNE(lhs, rhs, "cmptmp") : builder->CreateICmpEQ(lhs, rhs, "cmptmp");
				}
				case llvm::Type::PointerTyID:
				{
					lhs = builder->CreatePtrToInt(lhs, llvm::Type::getInt64Ty(*ctxt), "ptrcasttmp");
					rhs = builder->CreatePtrToInt(rhs, llvm::Type::getInt64Ty(*ctxt), "ptrcasttmp");
					return notval ? builder->CreateICmpNE(lhs, rhs, "cmptmp") : builder->CreateICmpEQ(lhs, rhs, "cmptmp");
				}
				case llvm::Type::FloatTyID:
				case llvm::Type::DoubleTyID:
				case llvm::Type::HalfTyID:
				{
					llvm::Type *larger = DataLayout->getTypeSizeInBits(lhs->getType()).getFixedSize() > DataLayout->getTypeSizeInBits(rhs->getType()).getFixedSize() ? lhs->getType() : rhs->getType();
					lhs = builder->CreateFPExt(lhs, larger, "floatExtendTmp");
					rhs = builder->CreateFPExt(rhs, larger, "floatExtendTmp");
					return notval ? builder->CreateFCmpONE(lhs, rhs, "cmptmp") : builder->CreateFCmpOEQ(lhs, rhs, "cmptmp");
				}
				default:
				if(operators[lhs->getType()][notval ? "!=" : "=="][rhs->getType()] == nullptr){
					errored = true; 
					std::cout << "Operator "<< (notval? "!=" : "==") <<" never overloaded to support " << AliasMgr.getTypeName(lhs->getType()) << " and " << AliasMgr.getTypeName(rhs->getType()) <<endl;
					return NULL; 
				}
				return builder->CreateCall(operators[lhs->getType()][notval ? "!=" : "=="][rhs->getType()], {rhs, lhs},"operatorcalltmp");

				}
			}
			else
			{
				errored = true;
				std::cout << "Error when attempting to compare two types (these should match): " << AliasMgr.getTypeName(lhs->getType()) << " and " << AliasMgr.getTypeName(rhs->getType());
				return NULL;
			}
		}
	};
	/**
	 * Handles multiplication and division (* and /) statments
	 */
	class MultDivStmtAST : public ExprAST
	{
		unique_ptr<ExprAST> LHS, RHS;
		bool div;

	public:
		MultDivStmtAST(unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS, bool isDiv)
			: LHS(std::move(LHS)), RHS(std::move(RHS)), div(isDiv) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			llvm::Value *lhs = LHS->codegen();
			llvm::Value *rhs = RHS->codegen();
			if (lhs->getType()->getTypeID() == rhs->getType()->getTypeID())
			{
				switch (lhs->getType()->getTypeID())
				{
				case llvm::Type::IntegerTyID:
				{
					llvm::Value *larger = lhs->getType()->getIntegerBitWidth() >= rhs->getType()->getIntegerBitWidth() ? lhs : rhs;
					lhs = builder->CreateSExtOrBitCast(lhs, larger->getType(), "signExtendTmp");
					rhs = builder->CreateSExtOrBitCast(rhs, larger->getType(), "signExtendTmp");
					return div ? builder->CreateSDiv(lhs, rhs, "divtmp") : builder->CreateMul(lhs, rhs, "multemp");
				}
				case llvm::Type::PointerTyID:
				{
					lhs = builder->CreatePtrToInt(lhs, llvm::Type::getInt64Ty(*ctxt), "ptrcasttmp");
					rhs = builder->CreatePtrToInt(rhs, llvm::Type::getInt64Ty(*ctxt), "ptrcasttmp");
					return div ? builder->CreateUDiv(lhs, rhs, "divtmp") : builder->CreateMul(lhs, rhs, "multmp");
				}
				case llvm::Type::FloatTyID:
				case llvm::Type::DoubleTyID:
				case llvm::Type::HalfTyID:
				{
					llvm::Type *larger = DataLayout->getTypeSizeInBits(lhs->getType()).getFixedSize() > DataLayout->getTypeSizeInBits(rhs->getType()).getFixedSize() ? lhs->getType() : rhs->getType();
					lhs = builder->CreateFPExt(lhs, larger, "floatExtendTmp");
					rhs = builder->CreateFPExt(rhs, larger, "floatExtendTmp");
					return div ? builder->CreateFDiv(lhs, rhs, "divtmp") : builder->CreateFMul(lhs, rhs, "multmp");
				}
				default:
				if(operators[lhs->getType()][div ? "/" : "*"][rhs->getType()] == nullptr){
					errored = true; 
					std::cout << "Operator "<< (div? '/' : '*') <<" never overloaded to support " << AliasMgr.getTypeName(lhs->getType()) << " and " << AliasMgr.getTypeName(rhs->getType()) <<endl;
					return NULL; 
				}
					return builder->CreateCall(operators[lhs->getType()][div ? "/" : "*"][rhs->getType()], {rhs, lhs},"operatorcalltmp");
				}
			}
			else
			{
				errored = true;
				std::cout << "Error when attempting to multiply two types (these should match): " << AliasMgr.getTypeName(lhs->getType()) << " and " << AliasMgr.getTypeName(rhs->getType());
				return NULL;
			}
		}
	};
	/**
	 * Handles addition and subtraction (+ and -) statments
	 */
	class AddSubStmtAST : public ExprAST
	{
		unique_ptr<ExprAST> LHS, RHS;
		bool sub;

	public:
		AddSubStmtAST(unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS, bool isSub)
			: LHS(std::move(LHS)), RHS(std::move(RHS)), sub(isSub) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			llvm::Value *lhs = LHS->codegen();
			llvm::Value *rhs = RHS->codegen();
			if (lhs->getType()->getTypeID() == rhs->getType()->getTypeID())
			{
				switch (lhs->getType()->getTypeID())
				{
				case llvm::Type::IntegerTyID:
				{
					llvm::Value *larger = lhs->getType()->getIntegerBitWidth() >= rhs->getType()->getIntegerBitWidth() ? lhs : rhs;
					lhs = builder->CreateSExtOrBitCast(lhs, larger->getType(), "signExtendTmp");
					rhs = builder->CreateSExtOrBitCast(rhs, larger->getType(), "signExtendTmp");
					return sub ? builder->CreateSub(lhs, rhs, "subtmp") : builder->CreateAdd(lhs, rhs, "addtemp");
				}
				case llvm::Type::PointerTyID:
				{
					lhs = builder->CreatePtrToInt(lhs, llvm::Type::getInt64Ty(*ctxt), "ptrcasttmp");
					rhs = builder->CreatePtrToInt(rhs, llvm::Type::getInt64Ty(*ctxt), "ptrcasttmp");
					return sub ? builder->CreateSub(lhs, rhs, "subtmp") : builder->CreateAdd(lhs, rhs, "addtmp");
				}
				case llvm::Type::FloatTyID:
				case llvm::Type::DoubleTyID:
				case llvm::Type::HalfTyID:
				{
					llvm::Type *larger = DataLayout->getTypeSizeInBits(lhs->getType()).getFixedSize() > DataLayout->getTypeSizeInBits(rhs->getType()).getFixedSize() ? lhs->getType() : rhs->getType();
					lhs = builder->CreateFPExt(lhs, larger, "floatExtendTmp");
					rhs = builder->CreateFPExt(rhs, larger, "floatExtendTmp");
					return sub ? builder->CreateFSub(lhs, rhs, "subtmp") : builder->CreateFAdd(lhs, rhs, "addtmp");
				}
				default:
				if(operators[lhs->getType()][sub ? "-" : "+"][rhs->getType()] == nullptr){
					errored = true; 
					std::cout << "Operator "<< (sub? '-' : '+') <<" never overloaded to support " << AliasMgr.getTypeName(lhs->getType()) << " and " << AliasMgr.getTypeName(rhs->getType()) <<endl;
					return NULL; 
				}
					return builder->CreateCall(operators[lhs->getType()][sub ? "-" : "+"][rhs->getType()], {rhs, lhs},"operatorcalltmp");
				}
			}
			else
			{
				errored = true;
				std::cout << "Error when attempting to multiply two types (these should match): " << AliasMgr.getTypeName(lhs->getType()) << " and " << AliasMgr.getTypeName(rhs->getType());
				return NULL;
			}
		}
	};

	/**
	 * Handles greater than and less than (< and >) statments
	 */
	class GtLtStmtAST : public ExprAST
	{
		unique_ptr<ExprAST> LHS, RHS;
		bool sub;

	public:
		GtLtStmtAST(unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS, bool isSub)
			: LHS(std::move(LHS)), RHS(std::move(RHS)), sub(isSub) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			llvm::Value *lhs = LHS->codegen();
			llvm::Value *rhs = RHS->codegen();
			if (lhs->getType()->getTypeID() == rhs->getType()->getTypeID())
			{
				switch (lhs->getType()->getTypeID())
				{
				case llvm::Type::IntegerTyID:
				{
					llvm::Value *larger = lhs->getType()->getIntegerBitWidth() >= rhs->getType()->getIntegerBitWidth() ? lhs : rhs;
					lhs = builder->CreateSExtOrBitCast(lhs, larger->getType(), "signExtendTmp");
					rhs = builder->CreateSExtOrBitCast(rhs, larger->getType(), "signExtendTmp");
					return sub ? builder->CreateICmpSLT(lhs, rhs, "subtmp") : builder->CreateICmpSGT(lhs, rhs, "addtemp");
				}
				case llvm::Type::PointerTyID:
				{
					lhs = builder->CreatePtrToInt(lhs, llvm::Type::getInt64Ty(*ctxt), "ptrcasttmp");
					rhs = builder->CreatePtrToInt(rhs, llvm::Type::getInt64Ty(*ctxt), "ptrcasttmp");
					return sub ? builder->CreateICmpULT(lhs, rhs, "subtmp") : builder->CreateICmpUGT(lhs, rhs, "addtmp");
				}
				case llvm::Type::FloatTyID:
				case llvm::Type::DoubleTyID:
				case llvm::Type::HalfTyID:
				{
					llvm::Type *larger = DataLayout->getTypeSizeInBits(lhs->getType()).getFixedSize() > DataLayout->getTypeSizeInBits(rhs->getType()).getFixedSize() ? lhs->getType() : rhs->getType();
					lhs = builder->CreateFPExt(lhs, larger, "floatExtendTmp");
					rhs = builder->CreateFPExt(rhs, larger, "floatExtendTmp");
					return sub ? builder->CreateFCmpOLT(lhs, rhs, "subtmp") : builder->CreateFCmpOGT(lhs, rhs, "addtmp");
				}
				default:
				if(operators[lhs->getType()][sub ? "<" : ">"][rhs->getType()] == nullptr){
					errored = true; 
					std::cout << "Operator "<< (sub? '<' : '>') <<" never overloaded to support " << AliasMgr.getTypeName(lhs->getType()) << " and " << AliasMgr.getTypeName(rhs->getType()) <<endl;
					return NULL; 
				}
					return builder->CreateCall(operators[lhs->getType()][sub ? "<" : ">"][rhs->getType()], {rhs, lhs},"operatorcalltmp");
				}
			}
			else
			{
				errored = true;
				std::cout << "Error when attempting to compare two types (these should match): " << AliasMgr.getTypeName(lhs->getType()) << " and " << AliasMgr.getTypeName(rhs->getType());
				return NULL;
			}
		}
	};

	/**
	 * Handles raised-to and modulo statements
	 */
	class PowModStmtAST : public ExprAST
	{
		unique_ptr<ExprAST> LHS, RHS;
		bool mod;

	public:
		PowModStmtAST(unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS, bool ismod)
			: LHS(std::move(LHS)), RHS(std::move(RHS)), mod(ismod) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			llvm::Type *longty = llvm::IntegerType::getInt32Ty(*ctxt);
			llvm::Type *doublety = llvm::Type::getDoubleTy(*ctxt);
			llvm::Value *lhs = LHS->codegen();
			llvm::Value *rhs = RHS->codegen();
			if (!mod && (rhs->getType()->isFloatingPointTy() || rhs->getType()->isIntegerTy()))
			{
				GlobalVarsAndFunctions->getOrInsertFunction("pow",
															llvm::FunctionType::get(doublety, {doublety, doublety}, false));
			}
			llvm::Function *powfunc = GlobalVarsAndFunctions->getFunction("pow");
			switch (lhs->getType()->getTypeID())
			{
			case llvm::Type::IntegerTyID:
				lhs = builder->CreateCast(llvm::Instruction::CastOps::SIToFP, lhs, llvm::Type::getDoubleTy(*ctxt), "floatConversionTmp");
				if (rhs->getType()->isIntegerTy())
				{
					rhs = builder->CreateCast(llvm::Instruction::CastOps::SIToFP, rhs, llvm::Type::getDoubleTy(*ctxt), "floatConversionTmp");
				}
			case llvm::Type::FloatTyID:
			case llvm::Type::DoubleTyID:
			case llvm::Type::HalfTyID:
			{
				if (!lhs->getType()->isDoubleTy())
				{
					lhs = builder->CreateFPExt(lhs, doublety, "floatExtendTmp");
				}
				if (!(rhs->getType()->isFloatingPointTy() || rhs->getType()->isIntegerTy()))
				{
					errored = true;
					std::cout << "Error when attempting to raise a number to a power: the right-hand-side is not an integer or float" << endl;
					return NULL;
				}
				if (rhs->getType()->isIntegerTy())
				{
					int bitWidth = rhs->getType()->getIntegerBitWidth();
					rhs = bitWidth != 32 ? builder->CreateSExtOrTrunc(rhs, longty, "IntBitWidthModifierTmp") : rhs;
				}
				else if (rhs->getType()->getTypeID() != llvm::Type::DoubleTyID)
				{
					rhs = builder->CreateFPExt(rhs, doublety, "floatExtendTmp");
				}
				return builder->CreateCall(powfunc, {lhs, rhs}, "powtmp");
			}
			default:
			if(operators[lhs->getType()][mod ? "%" : "^"][rhs->getType()] == nullptr){
					errored = true; 
					std::cout << "Operator "<< (mod ? '%' : '^') <<" never overloaded to support " << AliasMgr.getTypeName(lhs->getType()) << " and " << AliasMgr.getTypeName(rhs->getType()) <<endl;
					return NULL; 
				}
				return builder->CreateCall(operators[lhs->getType()][mod ? "%" : "^"][rhs->getType()], {rhs, lhs},"operatorcalltmp");
			}
			errored = true;
			std::cout << "Error when attempting to multiply two types (these should match): " << AliasMgr.getTypeName(lhs->getType()) << " and " << AliasMgr.getTypeName(rhs->getType());
			return NULL;
		}
	};

	// TODO: Break this up into several ExprAST objects for each operator
	/** BinaryStmtAST - Expression class for a binary operator.
	 *
	 *
	 */
	class BinaryStmtAST : public ExprAST
	{
		string Op;
		unique_ptr<ExprAST> LHS, RHS;

	public:
		BinaryStmtAST(string op, /* llvm::Type* type,*/ unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
			: Op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
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

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
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
				case (llvm::Type::DoubleTyID):
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
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
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
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			if (!AliasMgr.functions.hasAlias(Callee))
			{
				std::cout << "A function was never declared: " << Callee << endl;
				return NULL;
			}
			std::vector<llvm::Value *> ArgsV;
			std::vector<llvm::Type *> ArgsT;
			for (unsigned i = 0, e = Args.size(); i != e; ++i)
			{
				ArgsV.push_back(Args[i]->codegen());
				if (!ArgsV.back())
				{
					std::cout << "Error saving function args" << endl;
					return nullptr;
				}
				ArgsT.push_back(ArgsV.back()->getType());
			}

			// Look up the name in the global module table.

			llvm::Function *CalleeF = AliasMgr.functions.getFunction(Callee, ArgsT);
			if (!CalleeF)
			{

				std::cout << "Unknown function referenced, or incorrect arg types were passed: " << Callee << '(';
				int ctr = 0;
				for (auto &x : ArgsT)
				{
					std::cout << AliasMgr.getTypeName(x);
					if (ctr < ArgsT.size() - 1)
						std::cout << ", ";
				}
				std::cout << ')' << endl;
				return NULL;
			}

			return builder->CreateCall(CalleeF, ArgsV, "calltmp");
		}
	};

	class MemberAccessExprAST : public ExprAST
	{
		std::string member;
		std::unique_ptr<ExprAST> base;
		bool dereferenceParent;

	public:
		MemberAccessExprAST(std::unique_ptr<ExprAST> &base, std::string offset, bool deref = false) : base(std::move(base)), member(offset), dereferenceParent(deref) {}

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			// return NULL;
			llvm::Value *lhs = base->codegen(dereferenceParent);
			auto returnTy = AliasMgr(lhs->getType()->getContainedType(0), member);
			if (returnTy.index == -1)
			{
				std::cout << "No object member or function with name " << member << endl;
				errored = true;
				return NULL;
			}
			llvm::Constant *offset = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctxt), llvm::APInt(32, returnTy.index));
			llvm::Constant *objIndex = llvm::ConstantInt::get(*ctxt, llvm::APInt(32, 0, false));
			llvm::Value *gep = builder->CreateInBoundsGEP(lhs->getType()->getContainedType(0), lhs, llvm::ArrayRef<llvm::Value *>({objIndex, offset}), "memberaccess");
			return autoDeref ? builder->CreateLoad(returnTy.type, gep, "LoadTmp") : gep;
		}
	};

	/**
	 * @brief ObjectFunctionCallExprAST - Expression class for constructor calls from objects. This acts just like regular function calls,
	 * but this adds an implicit pointer to the object refrencing the call, so it needs its own ExprAST to function properly
	 * @example vector<T> obj;
	 * obj.push_back(x); //"obj" is the object refrencing the call, so push_back() becomes: push_back(&obj, x)
	 */
	class ObjectFunctionCallExprAST : public ExprAST
	{
		string Callee;
		vector<std::unique_ptr<ExprAST>> Args;
		std::unique_ptr<ExprAST> parent;
		bool dereferenceParent;

	public:
		ObjectFunctionCallExprAST(const string callee, vector<std::unique_ptr<ExprAST>> &Arg, std::unique_ptr<ExprAST> &parent, bool deref) : dereferenceParent(deref), Callee(callee), Args(std::move(Arg)), parent(std::move(parent))
		{
		}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			llvm::Value *parval = parent->codegen(dereferenceParent);
			std::vector<llvm::Value *> ArgsV;
			std::vector<llvm::Type *> ArgsT;
			ArgsV.push_back(parval);
			ArgsT.push_back(parval->getType());

			for (unsigned i = 0, e = Args.size(); i != e; ++i)
			{
				ArgsV.push_back(Args[i]->codegen());
				if (!ArgsV.back())
				{
					std::cout << "Error saving function args";
					return nullptr;
				}
				ArgsT.push_back(ArgsV.back()->getType());
			}

			llvm::Function *CalleeF = AliasMgr.functions.getFunction(Callee, ArgsT);
			if (!CalleeF)
			{
				std::cout << "Unknown object function referenced, or incorrect arg types were passed: " << Callee << '(';
				int ctr = 0;
				for (auto &x : ArgsT)
				{
					std::cout << AliasMgr.getTypeName(x);
					if (ctr < ArgsT.size() - 1)
						std::cout << ", ";
				}
				std::cout << ')' << endl;
				return NULL;
			}

			return builder->CreateCall(CalleeF, ArgsV, "calltmp");
		}
	};

	// TODO: Double check this codegen function, make sure it works properly
	/**
	 * @brief ObjectConstructorCallExprAST - Expression class for constructor calls from objects. This acts just like regular function calls,
	 * but this adds an implicit pointer to the object refrencing the call, so it needs its own ExprAST to function properly
	 * @example vector<T> obj;
	 * obj.push_back(x); //"obj" is the object refrencing the call, so push_back() becomes: push_back(&obj, x)
	 */
	class ObjectConstructorCallExprAST : public ExprAST
	{
		string Callee;
		vector<std::unique_ptr<ExprAST>> Args;
		std::unique_ptr<ExprAST> target = NULL;
		std::unique_ptr<TypeExpr> CalledTyConstructor;

	public:
		ObjectConstructorCallExprAST(const string &callee, vector<std::unique_ptr<ExprAST>> &Arg) : Callee(callee), Args(std::move(Arg)) {}
		ObjectConstructorCallExprAST(const string &callee, vector<std::unique_ptr<ExprAST>> &Arg, std::unique_ptr<ExprAST> &target) : target(std::move(target)), Callee(callee), Args(std::move(Arg)) {}
		ObjectConstructorCallExprAST(std::unique_ptr<TypeExpr> &callee, vector<std::unique_ptr<ExprAST>> &Arg) : CalledTyConstructor(std::move(callee)), Args(std::move(Arg)) {}
		ObjectConstructorCallExprAST(std::unique_ptr<TypeExpr> &callee, vector<std::unique_ptr<ExprAST>> &Arg, std::unique_ptr<ExprAST> &target) : target(std::move(target)), CalledTyConstructor(std::move(callee)), Args(std::move(Arg)) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			// Get data type by name or by generating it directly
			llvm::Type *TargetType = CalledTyConstructor == NULL ? AliasMgr(Callee) : this->CalledTyConstructor->codegen();

			// target = void* where the object will be initialized, usually a heap allocation
			// other = type* stack local variable
			if (target != NULL)
			{
				// call calloc() passing the size as a Value*
				llvm::Value *heapalloc = target->codegen(false, AliasMgr.getTypeSize(TargetType, ctxt, DataLayout));
				heapalloc = builder->CreateBitCast(heapalloc, TargetType->getPointerTo(), "bitcasttmp");
				// store pointer to calloc() allocation on stack
				builder->CreateStore(heapalloc, other);
				other = builder->CreateLoad(other->getType()->getNonOpaquePointerElementType(), other, "loadtmp");
			}
			if (other->getType() != TargetType->getPointerTo())
			{
				errored = true;
				std::cout << "Error when attempting to assign a constructor value: The type of the left side (" << AliasMgr.getTypeName(other->getType()) << ") does not match the right side (" << AliasMgr.getTypeName(target == NULL ? TargetType : TargetType->getPointerTo()) << ")." << endl;
				return NULL;
			}
			if (TargetType->getTypeID() != llvm::Type::StructTyID)
			{
				if (!Args.empty())
					builder->CreateStore(Args[0]->codegen(), other);
				return NULL;
			}
			std::vector<llvm::Value *> ArgsV;
			std::vector<llvm::Type *> ArgsT;
			ArgsV.push_back(other);
			ArgsT.push_back(TargetType->getPointerTo());
			// Look up the name in the global module table.
			for (unsigned i = 0, e = Args.size(); i != e; ++i)
			{
				ArgsV.push_back(Args[i]->codegen());
				if (!ArgsV.back())
				{
					std::cout << "Error saving function args";
					return nullptr;
				}
				ArgsT.push_back(ArgsV.back()->getType());
			}
			llvm::Function *CalleeF;
			CalleeF = Callee == "" ? AliasMgr(TargetType, ArgsT) : AliasMgr.functions.getFunction(Callee, ArgsT);
			if (!CalleeF)
			{
				std::cout << "Unknown object constructor referenced, or incorrect arg types were passed: " << Callee << endl;
				errored = true;
				return NULL;
			}

			builder->CreateCall(CalleeF, ArgsV, "calltmp");
			return NULL;
		}
	};

	class ConstructorExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> bod;
		std::vector<Variable> argslist;
		std::string objName;

	public:
		ConstructorExprAST(
			std::vector<Variable> &argList,
			std::unique_ptr<ExprAST> &body,
			std::string objName) : argslist(std::move(argList)), bod(std::move(body)), objName(objName){};
		// TODO: Review this, double check that nothing is broken
		llvm::Value *codegen(bool autoderef = false, llvm::Value *other = NULL)
		{
			std::vector<std::string> argnames;
			std::vector<llvm::Type *> argtypes;
			llvm::StructType *retType = llvm::StructType::getTypeByName(*ctxt, objName); // I forget if this was intentional or legacy code
			argnames.push_back("this");
			argtypes.push_back(retType->getPointerTo());
			for (auto &x : argslist)
			{
				argnames.push_back(x.name);
				argtypes.push_back(x.ty->codegen());
			}
			llvm::FunctionType *FT =
				llvm::FunctionType::get(argtypes[0], argtypes, false);
			llvm::Function *lastfunc = currentFunction;
			currentFunction =
				llvm::Function::Create(FT, llvm::Function::ExternalLinkage, objName + "_constructor", GlobalVarsAndFunctions.get());
			llvm::BasicBlock *entry = llvm::BasicBlock::Create(*ctxt, "entry", currentFunction);
			builder->SetInsertPoint(entry);

			unsigned Idx = 0;
			for (auto &Arg : currentFunction->args())
			{
				std::string name = argnames[Idx++];
				Arg.setName(name);
				AliasMgr[name] = &Arg;
				if (Idx == 1)
					continue;
				llvm::Value *storedvar = builder->CreateAlloca(Arg.getType(), NULL, Arg.getName());
				builder->CreateStore(&Arg, storedvar);
				AliasMgr[name] = storedvar;
			}

			llvm::Value *RetVal = bod->codegen();
			builder->CreateRet(AliasMgr["this"]);
			// Validate the generated code, checking for consistency.
			verifyFunction(*currentFunction);
			if (DEBUGGING)
				std::cout << "//end of " << objName << "'s constructor" << endl;
			// remove the arguments now that they're out of scope
			for (auto &Arg : currentFunction->args())
			{
				AliasMgr[std::string(Arg.getName())] = NULL;
				// dtypes[std::string(Arg.getName())] = NULL;
			}
			// for (auto x : AliasMgr.structTypes[objName].members)
			// {
			// 	AliasMgr[x.first] = NULL; //builder->CreateGEP(x.second.second, (llvm::Value *)thisfunc->getArg(0), offset, "ObjMemberAccessTmp");
			// }
			AliasMgr.functions.addFunction(objName, currentFunction, argtypes);
			AliasMgr.objects.addConstructor(AliasMgr(objName), currentFunction, argtypes);
			llvm::Function *thisfunc = currentFunction;
			currentFunction = lastfunc;
			if (currentFunction != NULL)
				builder->SetInsertPoint(&currentFunction->getBasicBlockList().back());
			return thisfunc;
		}
	};

	class ObjectHeaderExpr
	{
	public:
		std::string name;
		ObjectHeaderExpr(const std::string &nameval) : name(nameval) {}

		llvm::StructType *codegen(bool autoderef = false, llvm::Value *other = NULL)
		{
			// TODO: This could cause bugs later. Find a better (non-bandaid) solution
			llvm::StructType *ty = llvm::StructType::getTypeByName(*ctxt, name);
			return ty == NULL ? llvm::StructType::create(*ctxt, name) : ty;
		}
	};
	/**
	 * The class definition for an object (struct) in memory
	 */
	class ObjectExprAST : public ExprAST
	{
		ObjectHeaderExpr base;
		std::vector<std::pair<std::string, std::unique_ptr<TypeExpr>>> vars;
		std::vector<std::unique_ptr<ExprAST>> functions;

	public:
		ObjectExprAST(ObjectHeaderExpr name,
					  std::vector<std::pair<std::string, std::unique_ptr<TypeExpr>>> &varArg,
					  std::vector<std::unique_ptr<ExprAST>> &funcList) : base(name), vars(std::move(varArg)), functions(std::move(funcList)){};

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			llvm::StructType *ty = base.codegen();
			std::vector<llvm::Type *> types;
			std::vector<std::string> names;
			for (auto &x : vars)
			{
				types.push_back(x.second->codegen());
				names.push_back(x.first);
			}
			if (!types.empty())
				ty->setBody(types);

			AliasMgr.objects.addObject(base.name, ty, types, names);
			if (!functions.empty())
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
	public:
		std::string Name, parent;
		std::unique_ptr<TypeExpr> retType;
		std::vector<Variable> Args;
		PrototypeAST(){};
		PrototypeAST(const std::string &name,
					 std::vector<Variable> &args,
					 std::unique_ptr<TypeExpr> &ret, const std::string &parent = "")
			: Name(name), Args(std::move(args)), retType(std::move(ret)), parent(parent) {}

		const std::string &getName() const { return Name; }
		std::vector<llvm::Type *> getArgTypes() const
		{
			std::vector<llvm::Type *> ret;
			for (auto &x : Args)
				ret.push_back(x.ty->codegen());
			// if(retType != NULL)ret.emplace(ret.begin(), retType->codegen());
			return ret;
		}

		llvm::Function *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			std::vector<std::string> Argnames;
			std::vector<llvm::Type *> Argt;
			if (AliasMgr(parent) != NULL)
			{
				Argnames.push_back("this");
				Argt.push_back(AliasMgr(parent)->getPointerTo());
			}
			for (auto &x : Args)
			{
				Argnames.push_back(x.name);
				Argt.push_back(x.ty->codegen());
			}
			llvm::FunctionType *FT =
				llvm::FunctionType::get(retType->codegen(), Argt, false);
			int ctr = 1;
			std::string internalName = Name;
			while (GlobalVarsAndFunctions->getFunction(internalName) != NULL)
			{
				ctr++;
				internalName = Name + (std::to_string(ctr));
			}
			llvm::Function *F =
				llvm::Function::Create(FT, llvm::Function::ExternalLinkage, internalName, GlobalVarsAndFunctions.get());
			unsigned Idx = 0;
			for (auto &Arg : F->args())
			{
				Arg.setName(Argnames[Idx++]);
			}
			AliasMgr.functions.addFunction(Name, F, Argt);
			return F;
		}
	};
	// TODO: Improve object function solution. Current Implementation feels wrong
	/// FunctionAST - This class represents a function definition itself.
	class FunctionAST : public ExprAST
	{
		std::unique_ptr<PrototypeAST> Proto;
		std::unique_ptr<ExprAST> Body;

	public:
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
					std::unique_ptr<ExprAST> Body, std::string parentType = "")
			: Proto(std::move(Proto)), Body(std::move(Body)) {}

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{
			if (DEBUGGING)
				std::cout << Proto->getName();
			llvm::Function *prevFunction = currentFunction;
			std::vector<llvm::Type *> argtypes = (Proto->getArgTypes());
			currentFunction = AliasMgr.functions.getFunction(Proto->Name, argtypes);
			if (!currentFunction)
				currentFunction = Proto->codegen();

			if (!currentFunction)
			{
				currentFunction = prevFunction;
				return NULL;
			}
			if (!currentFunction->empty())
			{
				errored = true;
				std::cout << "Function Cannot be redefined: " << currentFunction->getName().str() << endl;
				currentFunction = prevFunction;
				return NULL;
			}

			llvm::BasicBlock *BB = llvm::BasicBlock::Create(*ctxt, "entry", currentFunction);
			builder->SetInsertPoint(BB);
			// Record the function arguments in the Named Values map.
			for (auto &Arg : currentFunction->args())
			{
				// TODO: Bandaid fix, find a better solution. This will cause bugs later
				if (Arg.getArgNo() == 0 && Proto->parent != "")
				{
					AliasMgr[Arg.getName().str()] = &Arg;
					continue;
				}
				llvm::Value *storedvar = builder->CreateAlloca(Arg.getType(), NULL, Arg.getName());
				builder->CreateStore(&Arg, storedvar);
				std::string name = std::string(Arg.getName());
				AliasMgr[name] = storedvar;
				// dtypes[name] = Arg.getType();
			}
			llvm::Instruction *currentEntry = &BB->getIterator()->back();
			llvm::Value *RetVal = Body == NULL ? NULL : Body->codegen();

			if (RetVal != NULL && (!RetVal->getType()->isPointerTy() || !RetVal->getType()->getNonOpaquePointerElementType()->isFunctionTy()))
			{
				// Validate the generated code, checking for consistency.
				verifyFunction(*currentFunction);
				if (DEBUGGING)
					std::cout << "//end of " << Proto->getName() << endl;
				// remove the arguments now that they're out of scope
				for (auto &Arg : currentFunction->args())
				{
					AliasMgr[std::string(Arg.getName())] = NULL;
					// dtypes[std::string(Arg.getName())] = NULL;
				}
			}
			else
			{
				currentFunction->deleteBody();
			}
			currentFunction = prevFunction;
			if (currentFunction != NULL)
				builder->SetInsertPoint(&currentFunction->getBasicBlockList().back());
			return AliasMgr.functions.getFunction(Proto->Name, argtypes);
		}
	};

	class OperatorOverloadAST : public ExprAST
	{
		PrototypeAST Proto;
		std::unique_ptr<ExprAST> Body;
		std::vector<Variable> args; 
		std::unique_ptr<TypeExpr> retType; 		
		std::string name = "operator_", oper; 
	public:
		OperatorOverloadAST(std::string &oper,std::unique_ptr<TypeExpr> &ret, std::vector<Variable> &args, 
					std::unique_ptr<ExprAST> &Body)
			: oper(oper), Body(std::move(Body)), args(std::move(args)),  retType(std::move(ret)) {}

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		{

			name+= oper+"_";
			llvm::raw_string_ostream stringstream(name); 
			for(auto t = args.begin(); t < args.end(); t++){
				t->ty->codegen()->print(stringstream);
				name+='_';   
			}
			Proto = PrototypeAST(name, args, retType); 
			llvm::Function *prevFunction = currentFunction;
			std::vector<llvm::Type *> argtypes = (Proto.getArgTypes());
			//currentFunction = AliasMgr.functions.getFunction(Proto.Name, argtypes);
			//if (!currentFunction)
			//	currentFunction = Proto.codegen();
			//Segment stolen from Proto.codegen()
			std::vector<std::string> Argnames;
			std::vector<llvm::Type *> Argt;
			for (auto &x : Proto.Args)
			{
				Argnames.push_back(x.name);
				Argt.push_back(x.ty->codegen());
			}
			llvm::FunctionType *FT =
				llvm::FunctionType::get(retType->codegen(), Argt, false);
			llvm::Function *F =
				llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Proto.Name, GlobalVarsAndFunctions.get());
			unsigned Idx = 0;
			for (auto &Arg : F->args())
			{
				Arg.setName(Argnames[Idx++]);
			}
			//end of segment stolen from Proto.codegen()

			if (!currentFunction)
			{
				currentFunction = prevFunction;
				return NULL;
			}
			if (!currentFunction->empty())
			{
				errored = true;
				std::cout << "Operator Cannot be redefined: " << currentFunction->getName().str() << endl;
				currentFunction = prevFunction;
				return NULL;
			}

			llvm::BasicBlock *BB = llvm::BasicBlock::Create(*ctxt, "entry", currentFunction);
			builder->SetInsertPoint(BB);
			// Record the function arguments in the Named Values map.
			for (auto &Arg : currentFunction->args())
			{
				llvm::Value *storedvar = builder->CreateAlloca(Arg.getType(), NULL, Arg.getName());
				builder->CreateStore(&Arg, storedvar);
				std::string name = std::string(Arg.getName());
				AliasMgr[name] = storedvar;
				// dtypes[name] = Arg.getType();
			}
			llvm::Instruction *currentEntry = &BB->getIterator()->back();
			llvm::Value *RetVal = Body == NULL ? NULL : Body->codegen();

			if (RetVal != NULL && (!RetVal->getType()->isPointerTy() || !RetVal->getType()->getNonOpaquePointerElementType()->isFunctionTy()))
			{
				// Validate the generated code, checking for consistency.
				verifyFunction(*currentFunction);
				if (DEBUGGING)
					std::cout << "//end of " << Proto.getName() << endl;
				// remove the arguments now that they're out of scope
				for (auto &Arg : currentFunction->args())
				{
					AliasMgr[std::string(Arg.getName())] = NULL;
					// dtypes[std::string(Arg.getName())] = NULL;
				}
			}
			else
			{
				currentFunction->deleteBody();
			}
			currentFunction = prevFunction;
			if (currentFunction != NULL)
				builder->SetInsertPoint(&currentFunction->getBasicBlockList().back());
			return operators[argtypes[0]][oper][argtypes[1]];
		}
	};


	// TODO: Implement this fully with the new typing system
	//  class ArrayOfTypeExpr : public TypeExpr{
	//  	std::unique_ptr<TypeExpr> ty;
	//  	std::unique_ptr<ExprAST> len;
	//  	ArrayOfTypeExpr(std::unique_ptr<TypeExpr> &ty,std::unique_ptr<ExprAST> &len) : ty(std::move(ty)), len(std::move(len)){}
	//  	llvm::Type *codegen(bool testforval = false){
	//  		return llvm::ArrayType::get(ty->codegen(), );
	//  	}
	//  };

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
	void functionArg(Stack<Token> &tokens, Variable& out);

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

	void thisOrFunctionArg(Stack<Token> &tokens, Variable& out){
		if(tokens.peek() == IDENT && tokens.peek().lex == "this"){ 
			std::string thisval = "this"; 
			std::unique_ptr<TypeExpr> nullty = NULL; 
			out.name = thisval; 
			out.ty = std::move(nullty); 
			return; 
		}
		functionArg(tokens, out); 
	}

	std::unique_ptr<ExprAST> operatorOverloadStmt(Stack<Token> &tokens, std::unique_ptr<TypeExpr> ty){
		if(tokens.next() != OPERATOR){
			return NULL; 
		}
		Variable placeholder; 
		std::vector<Variable> vars; 
		if(tokens.peek() == IDENT || tokens.peek().token >= INT){
			thisOrFunctionArg(tokens, placeholder); 
			vars.push_back(std::move(placeholder));
		}
		Token op = tokens.peek(); 
		switch(op.token){
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
			thisOrFunctionArg(tokens, placeholder);
			vars.push_back(std::move(placeholder));  
			if(op == OPENSQUARE && tokens.peek() != CLOSESQUARE){
				errored = true; 
				logError("Expected a closing square brace at this token:", tokens.currentToken()); 
				return NULL; 
			}else if(op == OPENSQUARE) tokens.next(); 
			break;
			//case PERIOD: 
			default: 
				errored = true; 
				logError("Unknown operator:", tokens.currentToken()); 
				return NULL; 
			case NOT: 
			case INCREMENT:
			case DECREMENT:
				Variable placeholder; 
				thisOrFunctionArg(tokens, placeholder); 
				vars.push_back(std::move(placeholder)); 
				break;
		} 
		std::unique_ptr<ExprAST> body = std::move(codeBlockExpr(tokens)); 
		return std::make_unique<OperatorOverloadAST>(op.lex, ty,vars, (body)); 
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
			}
			std::unique_ptr<TypeExpr> ty = std::move(variableTypeStmt(tokens));
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
		return std::make_unique<ObjectExprAST>(objName, objVars, objFunctions);
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
			if (name != IDENT)
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
	void functionArg(Stack<Token> &tokens, Variable& out){
			std::unique_ptr<TypeExpr> dtype = std::move(jimpilier::variableTypeStmt(tokens));
			if (dtype == NULL)
			{
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
		do
		{
			Variable v; 
			functionArg(tokens, v); 
			if(v.ty != nullptr)
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
		default:
			return std::move(declareStmt(tokens));
		}
	}
};
