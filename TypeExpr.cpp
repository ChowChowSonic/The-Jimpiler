#pragma once
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
#include "globals.cpp"
#include "TypeExpr.h"
namespace jimpilier
{

		llvm::Type *DoubleTypeExpr::codegen(bool testforval) { return llvm::Type::getDoubleTy(*ctxt); };
		std::unique_ptr<TypeExpr> DoubleTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new DoubleTypeExpr());
		}

		llvm::Type *FloatTypeExpr::codegen(bool testforval) { return llvm::Type::getFloatTy(*ctxt); };
		std::unique_ptr<TypeExpr> FloatTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new FloatTypeExpr());
		}

		llvm::Type *LongTypeExpr::codegen(bool testforval) { return llvm::Type::getInt64Ty(*ctxt); };
		std::unique_ptr<TypeExpr> LongTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new LongTypeExpr());
		}

		llvm::Type *IntTypeExpr::codegen(bool testforval) { return llvm::Type::getInt32Ty(*ctxt); };
		std::unique_ptr<TypeExpr> IntTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new IntTypeExpr());
		}

		llvm::Type *ShortTypeExpr::codegen(bool testforval) { return llvm::Type::getInt16Ty(*ctxt); };
		std::unique_ptr<TypeExpr> ShortTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new ShortTypeExpr());
		}
		llvm::Type *ByteTypeExpr::codegen(bool testforval) { return llvm::Type::getInt8Ty(*ctxt); };
		std::unique_ptr<TypeExpr> ByteTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new ByteTypeExpr());
		}

		llvm::Type *BoolTypeExpr::codegen(bool testforval) { return llvm::Type::getInt1Ty(*ctxt); };
		std::unique_ptr<TypeExpr> BoolTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new BoolTypeExpr());
		}

		llvm::Type *VoidTypeExpr::codegen(bool testforval) { return llvm::Type::getVoidTy(*ctxt); };
		std::unique_ptr<TypeExpr> VoidTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new VoidTypeExpr());
		}

		TemplateTypeExpr::TemplateTypeExpr(const std::string name) : name(name) {}
		llvm::Type *TemplateTypeExpr::codegen(bool testforval) { return llvm::StructType::create(name, {}); };
		std::unique_ptr<TypeExpr> TemplateTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new TemplateTypeExpr(name));
		}

		StructTypeExpr::StructTypeExpr(const std::string &structname) : name(structname) {}
		llvm::Type *StructTypeExpr::codegen(bool testforval)
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
		std::unique_ptr<TypeExpr> StructTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new StructTypeExpr(name));
		}

		llvm::Type *PointerToTypeExpr::codegen(bool testforval)
		{
			llvm::Type *t = ty->codegen();
			return t == NULL ? NULL : t->getPointerTo();
		}
		std::unique_ptr<TypeExpr> PointerToTypeExpr::clone()
		{
			std::unique_ptr<TypeExpr> encasedType = std::move(ty->clone());
			return std::make_unique<PointerToTypeExpr>(encasedType);
		}

		llvm::Type *ReferenceToTypeExpr::codegen(bool testforval)
		{
			llvm::Type *t = ty->codegen();
			return t == NULL ? NULL : t->getPointerTo();
		}
		std::unique_ptr<TypeExpr> ReferenceToTypeExpr::clone()
		{
			std::unique_ptr<TypeExpr> encasedType = std::move(ty->clone());
			return std::make_unique<ReferenceToTypeExpr>(encasedType);
		}

		Variable::Variable(const std::string &ident, std::unique_ptr<TypeExpr> &type) : name(ident), ty(std::move(type)) {}
		Variable::Variable() : name("")
		{
			ty = NULL;
		}
		std::string Variable::toString()
		{
			return "{ " + name + ", " + AliasMgr.getTypeName(ty->codegen()) + " }";
		}
}