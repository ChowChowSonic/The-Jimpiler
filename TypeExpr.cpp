#pragma once
#include <iostream>
#include <spdlog/spdlog.h>
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
		spdlog::debug("Retrieving double type expression");
		return std::unique_ptr<TypeExpr>(new DoubleTypeExpr());
	}

	llvm::Type *FloatTypeExpr::codegen(bool testforval) { return llvm::Type::getFloatTy(*ctxt); };
	std::unique_ptr<TypeExpr> FloatTypeExpr::clone()
	{
		spdlog::debug("Retrieving float type expression");
		return std::unique_ptr<TypeExpr>(new FloatTypeExpr());
	}

	llvm::Type *LongTypeExpr::codegen(bool testforval) { return llvm::Type::getInt64Ty(*ctxt); };
	std::unique_ptr<TypeExpr> LongTypeExpr::clone()
	{
		spdlog::debug("Retrieving long type expression");
		return std::unique_ptr<TypeExpr>(new LongTypeExpr());
	}

	llvm::Type *IntTypeExpr::codegen(bool testforval) { return llvm::Type::getInt32Ty(*ctxt); };
	std::unique_ptr<TypeExpr> IntTypeExpr::clone()
	{
		spdlog::debug("Retrieving int type expression");
		return std::unique_ptr<TypeExpr>(new IntTypeExpr());
	}

	llvm::Type *ShortTypeExpr::codegen(bool testforval) { return llvm::Type::getInt16Ty(*ctxt); };
	std::unique_ptr<TypeExpr> ShortTypeExpr::clone()
	{
		spdlog::debug("Retrieving short type expression");
		return std::unique_ptr<TypeExpr>(new ShortTypeExpr());
	}
	llvm::Type *ByteTypeExpr::codegen(bool testforval) { return llvm::Type::getInt8Ty(*ctxt); };
	std::unique_ptr<TypeExpr> ByteTypeExpr::clone()
	{
		spdlog::debug("Retrieving byte type expression");
		return std::unique_ptr<TypeExpr>(new ByteTypeExpr());
	}

	llvm::Type *BoolTypeExpr::codegen(bool testforval) { return llvm::Type::getInt1Ty(*ctxt); };
	std::unique_ptr<TypeExpr> BoolTypeExpr::clone()
	{
		spdlog::debug("Retrieving boolean type expression");
		return std::unique_ptr<TypeExpr>(new BoolTypeExpr());
	}

	llvm::Type *VoidTypeExpr::codegen(bool testforval) { return llvm::Type::getVoidTy(*ctxt); };
	std::unique_ptr<TypeExpr> VoidTypeExpr::clone()
	{
		spdlog::debug("Retrieving void type expression");
		return std::unique_ptr<TypeExpr>(new VoidTypeExpr());
	}

	llvm::Type *TemplateObjectExpr::codegen(bool testforval)
	{
		auto &templ = TemplateMgr.getTemplate(name, types);
		// Check that the object doesn't already exist; if it does, return it
		std::string typenames;
		for (auto &x : types)
			typenames += x->getName() + ',';
		typenames = name + '<' + typenames.substr(0, typenames.size() - 1) + '>';
		if (AliasMgr(typenames))
		{
			spdlog::debug("Retrieving template type expression: {}", typenames);
			return AliasMgr(typenames);
		}
		spdlog::debug("Creating template type expression: {}", typenames);
		// Object doesn't already exist, create it.
		std::vector<llvm::Type *> generatedTypes;
		// Manditory to generate types early, in their own loop, to avoid bugs with recursive template types
		for (auto &x : types)
			generatedTypes.push_back(x->codegen());

		for (int i = 0; i < templ.templates.size(); i++)
		{
			auto &x = templ.templates[i];
			AliasMgr.objects.replaceObject(x->getName(), generatedTypes[i]);
		}

		std::vector<llvm::Type *> objectTypes;
		std::vector<std::string> objectNames;
		for (auto &x : templ.members)
		{
			llvm::Type *currentMember = x.ty->codegen();
			objectTypes.push_back(currentMember);
			objectNames.push_back(x.name);
		}
		llvm::Type *ret = llvm::StructType::create(*ctxt, objectTypes, typenames, false);
		AliasMgr.objects.addObject(typenames, ret);
		AliasMgr.objects.addObjectMembers(typenames, objectTypes, objectNames);

		for (auto &x : templ.functions)
		{
			x->replaceTemplate(typenames);
			x->codegen();
		}

		for (int i = 0; i < templ.templates.size(); i++)
		{
			auto &x = templ.templates[i];
			AliasMgr.objects.removeObject(x->getName());
		}
		return ret;
	}
	std::string TemplateObjectExpr::getName()
	{
		std::string names;
		for (auto &x : types)
			names += x->getName() + ',';
		return name + '<' + names.substr(0, names.size() - 1) + '>';
	}
	std::unique_ptr<TypeExpr> TemplateObjectExpr::clone()
	{
		std::vector<std::unique_ptr<TypeExpr>> cpy;
		for (auto &x : types)
		{
			cpy.push_back(std::move(x->clone()));
		}
		return std::make_unique<TemplateObjectExpr>(name, cpy);
	}
	StructTypeExpr::StructTypeExpr(const std::string &structname) : name(structname) {}
	llvm::Type *StructTypeExpr::codegen(bool testforval)
	{
		llvm::Type *ty = AliasMgr(name);
		if (!testforval && ty == NULL)
		{
			logError("Unknown object of name: " + name);
			return NULL;
		}
		spdlog::debug("Retrieving struct type expression: {}", name);
		return ty;
	}
	std::string StructTypeExpr::getName() { return name; }
	std::unique_ptr<TypeExpr> StructTypeExpr::clone()
	{
		return std::unique_ptr<TypeExpr>(new StructTypeExpr(name));
	}

	llvm::Type *PointerToTypeExpr::codegen(bool testforval)
	{
		llvm::Type *t = ty->codegen();
		spdlog::debug("Retrieving pointer to type: {}", AliasMgr.getTypeName(t));
		return t == NULL ? NULL : t->getPointerTo();
	}
	void generateArrayFunctions(llvm::StructType *arrayTy, std::unique_ptr<TypeExpr> &typeExp)
	{
		spdlog::debug("Creating array[{}] helper functions", typeExp->getName());
		llvm::BasicBlock *lastInsertPoint = builder->GetInsertBlock();
		llvm::Type *elementTy = typeExp->codegen();
		llvm::FunctionType *operatorType = llvm::FunctionType::get(
			elementTy->getPointerTo(), {arrayTy->getPointerTo(), builder->getInt32Ty()}, false);
		llvm::Function *indexOperator = llvm::Function::Create(
			operatorType, llvm::Function::ExternalLinkage,
			"operator_[_" + arrayTy->getName().str() + "_", GlobalVarsAndFunctions.get());

		// Destructor: Free allocated memory
		llvm::FunctionType *dtorType = llvm::FunctionType::get(
			builder->getVoidTy(), {arrayTy->getPointerTo()}, false);
		llvm::Function *dtor = llvm::Function::Create(
			dtorType, llvm::Function::ExternalLinkage,
			arrayTy->getName().str() + "::destructor", GlobalVarsAndFunctions.get());

		spdlog::debug("Implementing array append function");
		// Push_back with reallocation logic
		std::vector<llvm::Type *> pushBackParams = {arrayTy->getPointerTo(), elementTy};
		llvm::FunctionType *pushBackType = llvm::FunctionType::get(
			builder->getVoidTy(), pushBackParams, false);
		llvm::Function *pushBack = llvm::Function::Create(
			pushBackType, llvm::Function::ExternalLinkage,
			arrayTy->getName().str() + "::push_back", GlobalVarsAndFunctions.get());

		// Push_Back: Create basic blocks
		llvm::BasicBlock *entry = llvm::BasicBlock::Create(*ctxt, "entry", pushBack);
		llvm::BasicBlock *reallocBB = llvm::BasicBlock::Create(*ctxt, "realloc", pushBack);
		llvm::BasicBlock *storeBB = llvm::BasicBlock::Create(*ctxt, "store", pushBack);

		builder->SetInsertPoint(entry);

		// Push_Back: Get vector pointer and element value
		llvm::Value *vecPtr = pushBack->arg_begin();
		llvm::Value *element = pushBack->arg_begin() + 1;

		// Push_Back: Load current size and capacity
		llvm::Value *sizePtr = builder->CreateStructGEP(arrayTy, vecPtr, 1);
		llvm::Value *size = builder->CreateLoad(builder->getInt64Ty(), sizePtr);

		llvm::Value *capPtr = builder->CreateStructGEP(arrayTy, vecPtr, 2);
		llvm::Value *capacity = builder->CreateLoad(builder->getInt64Ty(), capPtr);

		// Push_Back: Check size == capacity
		llvm::Value *needsRealloc = builder->CreateICmpEQ(size, capacity);
		builder->CreateCondBr(needsRealloc, reallocBB, storeBB);

		// Push_Back: Reallocation block
		builder->SetInsertPoint(reallocBB);
		{
			// Push_Back: Double capacity
			llvm::Value *newCapacity = builder->CreateAdd(capacity, builder->getInt64(10));

			// Push_Back: Get element size using DataLayout
			uint64_t elemSize = DataLayout->getTypeAllocSize(elementTy);
			llvm::Value *elemSizeVal = builder->getInt64(elemSize);

			// Push_Back: Calculate new byte size
			llvm::Value *totalBytes = builder->CreateMul(newCapacity, elemSizeVal);

			// Push_Back: Allocate new memory
			llvm::Value *dataPtr = builder->CreateStructGEP(arrayTy, vecPtr, 0);
			llvm::Value *oldData = builder->CreateLoad(elementTy->getPointerTo(), dataPtr);

			llvm::Value *newDataRaw = builder->CreateCall(
				GlobalVarsAndFunctions->getOrInsertFunction("realloc", llvm::FunctionType::get(llvm::Type::getInt8Ty(*ctxt)->getPointerTo(), {builder->getInt8Ty()->getPointerTo(), builder->getInt32Ty()}, false)),
				{builder->CreateBitCast(oldData, llvm::Type::getInt8Ty(*ctxt)->getPointerTo()), builder->CreateTrunc(totalBytes, llvm::Type::getInt32Ty(*ctxt))});
			// Push_Back: Cast to element type pointer
			llvm::Value *newData = builder->CreateBitCast(newDataRaw, elementTy->getPointerTo());

			// Push_Back: Update vector fields
			builder->CreateStore(newData, dataPtr);
			builder->CreateStore(newCapacity, capPtr);

			builder->CreateBr(storeBB);
		}

		// Push_Back: Store element block
		builder->SetInsertPoint(storeBB);
		{
			// Push_Back: Get data pointer
			llvm::Value *dataPtr = builder->CreateStructGEP(arrayTy, vecPtr, 0);
			llvm::Value *data = builder->CreateLoad(elementTy->getPointerTo(), dataPtr);

			// Push_Back: Calculate element position
			llvm::Value *elemPtr = builder->CreateGEP(elementTy, data, size);
			builder->CreateStore(element, elemPtr);

			// Push_Back: Increment size
			llvm::Value *newSize = builder->CreateAdd(size, builder->getInt64(1));
			builder->CreateStore(newSize, sizePtr);

			builder->CreateRetVoid();
		}
		spdlog::debug("Creating array indexing operator");
		// Index Operator Implementation
		llvm::BasicBlock *indexBlock = llvm::BasicBlock::Create(*ctxt, "entry", indexOperator);
		builder->SetInsertPoint(indexBlock);
		llvm::Value *gep = builder->CreateStructGEP(arrayTy, indexOperator->getArg(0), 0, "indextmp");
		// gep = builder->CreateStructGEP(elementTy->getPointerTo(), gep, 0, "indextmp");
		gep = builder->CreateGEP(elementTy->getPointerTo(), gep, {builder->getInt32(0)}, "indextmp");
		gep = builder->CreateLoad(elementTy->getPointerTo(), gep);
		builder->CreateGEP(elementTy, gep, indexOperator->getArg(1), "indextmp");
		builder->CreateRet(gep);

		spdlog::debug("Implementing array destructor");
		// Destructor implementation
		entry = llvm::BasicBlock::Create(*ctxt, "entry", dtor);
		builder->SetInsertPoint(entry);
		gep = builder->CreateGEP(arrayTy, dtor->getArg(0), {builder->getInt32(0), builder->getInt32(0)}, "loadtmp");
		gep = builder->CreateBitCast(gep, builder->getInt8Ty()->getPointerTo());
		builder->CreateCall(GlobalVarsAndFunctions->getOrInsertFunction("free", llvm::FunctionType::get(builder->getVoidTy(), {builder->getInt8Ty()->getPointerTo()}, false)), {gep});
		// gep = builder->CreateBitCast(dtor->getArg(0), builder->getInt8Ty()->getPointerTo());
		// builder->CreateCall(GlobalVarsAndFunctions->getOrInsertFunction("free", llvm::FunctionType::get(builder->getVoidTy(), {builder->getInt8Ty()->getPointerTo()}, false)), {gep});
		builder->CreateRetVoid();

		spdlog::debug("Verifying array functions");
		// Verify functions
		llvm::verifyFunction(*dtor);
		llvm::verifyFunction(*pushBack);
		llvm::verifyFunction(*indexOperator);

		std::vector<Variable> args;
		std::unique_ptr<TypeExpr> t2 = std::make_unique<StructTypeExpr>(arrayTy->getName().str());
		t2 = std::make_unique<ReferenceToTypeExpr>(t2);
		args.push_back(Variable("this", t2));
		t2 = std::make_unique<IntTypeExpr>();
		args.push_back(Variable("value", t2));
		AliasMgr.functions.addFunction("append", pushBack, args, false);
		// t2 = std::make_unique<IntTypeExpr>();
		// args.push_back(Variable("count", t2));
		// AliasMgr.objects.addConstructor(arrayTy, ctor, args);
		t2 = std::make_unique<StructTypeExpr>(arrayTy->getName().str());
		t2 = std::make_unique<ReferenceToTypeExpr>(t2);
		args.push_back(Variable("this", t2));
		operators[NULL]["DELETE"][arrayTy] = FunctionHeader(args, dtor, false);
		t2 = std::make_unique<StructTypeExpr>(arrayTy->getName().str());
		t2 = std::make_unique<ReferenceToTypeExpr>(t2);
		args.push_back(Variable("this", t2));
		t2 = std::make_unique<IntTypeExpr>();
		args.push_back(Variable("offset", t2));
		operators[arrayTy]["["][builder->getInt32Ty()] = FunctionHeader(args, indexOperator, true);
		builder->SetInsertPoint(lastInsertPoint);
		spdlog::debug("Completed implementation of array functions");
	}

	llvm::Type *ArrayOfTypeExpr::codegen(bool testforval)
	{
		std::vector<std::unique_ptr<TypeExpr>> tyarr;
		tyarr.push_back(std::move(ty));
		std::string name(".array");
		auto &templ = TemplateMgr.getTemplate(name, tyarr);
		// Check that the object doesn't already exist; if it does, return it
		std::string typenames;
		for (auto &x : tyarr)
			typenames += x->getName() + ',';
		typenames = name + '<' + typenames.substr(0, typenames.size() - 1) + '>';
		if (AliasMgr(typenames)){
			spdlog::debug("Retrieving Array[{}]", typenames);
			return AliasMgr(typenames);
		}
		spdlog::debug("Creating Array[{}]", typenames);
		// Object doesn't already exist, create it.
		std::vector<llvm::Type *> generatedTypes;
		// Manditory to generate types this way to avoid bugs with recursive template types
		for (auto &x : tyarr)
			generatedTypes.push_back(x->codegen());
		for (int i = 0; i < templ.templates.size(); i++)
		{
			auto &x = templ.templates[i];
			AliasMgr.objects.replaceObject(x->getName(), generatedTypes[i]);
		}

		std::vector<llvm::Type *> objectTypes;
		std::vector<std::string> objectNames;
		for (auto &x : templ.members)
		{
			llvm::Type *currentMember = x.ty->codegen();
			objectTypes.push_back(currentMember);
			objectNames.push_back(x.name);
		}
		llvm::Type *ret = llvm::StructType::create(*ctxt, objectTypes, typenames, false);
		AliasMgr.objects.addObject(typenames, ret);
		AliasMgr.objects.addObjectMembers(typenames, objectTypes, objectNames);

		generateArrayFunctions((llvm::StructType *)AliasMgr(typenames), tyarr[0]);
		for (int i = 0; i < templ.templates.size(); i++)
		{
			auto &x = templ.templates[i];
			AliasMgr.objects.removeObject(x->getName());
		}
		return ret;
	}
	std::unique_ptr<TypeExpr> PointerToTypeExpr::clone()
	{
		std::unique_ptr<TypeExpr> encasedType = std::move(ty->clone());
		return std::make_unique<PointerToTypeExpr>(encasedType);
	}

	std::unique_ptr<TypeExpr> ArrayOfTypeExpr::clone()
	{
		std::unique_ptr<TypeExpr> encasedType = std::move(ty->clone());
		return std::make_unique<ArrayOfTypeExpr>(encasedType);
	}

	llvm::Type *ReferenceToTypeExpr::codegen(bool testforval)
	{
		llvm::Type *t = ty->codegen();
		spdlog::debug("Retrieving Reference to below type");
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