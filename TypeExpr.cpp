#include <iostream>
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
#ifndef typeExpr
#define typeExpr
#include "globals.cpp"

namespace jimpilier{
    	
	class TypeExpr
	{
	public:
		virtual ~TypeExpr(){};
		virtual llvm::Type *codegen(bool testforval = false) = 0;
		virtual std::unique_ptr<TypeExpr> clone() = 0;
		virtual bool isReference(){
			return false; 
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

	class VoidTypeExpr : public TypeExpr
	{
	public:
		VoidTypeExpr() {}
		llvm::Type *codegen(bool testforval = false) { return llvm::Type::getVoidTy(*ctxt); };
		std::unique_ptr<TypeExpr> clone()
		{
			return std::unique_ptr<TypeExpr>(new VoidTypeExpr());
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
				std::cout << "Unknown object of name: " << name << std::endl;
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

	class ReferenceToTypeExpr : public TypeExpr
	{
		std::unique_ptr<TypeExpr> ty;

	public:
		ReferenceToTypeExpr(std::unique_ptr<TypeExpr> &type) : ty(std::move(type)) {}
		llvm::Type *codegen(bool testforval = false)
		{
			llvm::Type *t = ty->codegen();
			return t == NULL ? NULL : t->getPointerTo();
		}
		std::unique_ptr<TypeExpr> clone()
		{
			std::unique_ptr<TypeExpr> encasedType = std::move(ty->clone());
			return std::make_unique<ReferenceToTypeExpr>(encasedType);
		}

		bool isReference(){
			return true; 
		}
	};
	class Variable
	{
	public:
		std::string name;
		std::unique_ptr<TypeExpr> ty;
		Variable(const std::string &ident, std::unique_ptr<TypeExpr> &type) : name(ident), ty(std::move(type)) {}
		Variable() : name("")
		{
			ty = NULL;
		}
		std::string toString()
		{
			return "{ " + name + ", " + AliasMgr.getTypeName(ty->codegen()) + " }";
		}
	};
    }
    #endif