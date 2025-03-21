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
#include "llvm/IR/InlineAsm.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/FileCheck/FileCheck.h"

#include "globals.cpp"
#include "TypeExpr.h"
#include "AliasManager.h"
#include "ExprAST.h"
using namespace std;
namespace jimpilier
{

	llvm::Value *NumberExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		if (DEBUGGING)
			std::cout << Val;
		if (isInt)
			return llvm::ConstantInt::get(*ctxt, llvm::APInt(32, static_cast<long>(Val), true));
		if (isBool)
			return llvm::ConstantInt::get(*ctxt, llvm::APInt(1, Val == 1, true));
		return llvm::ConstantFP::get(*ctxt, llvm::APFloat((float)Val));
	}

	llvm::Value *StringExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		if (DEBUGGING)
			std::cout << Val;
		// if(Val.length() > 1)
		return builder->CreateGlobalStringPtr(Val, "Sconst");
		// return llvm::ConstantInt::get(llvm::Type::getInt8Ty(*ctxt), llvm::APInt(8, Val[0]));
	}

	llvm::Value *VariableExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		if (DEBUGGING)
			std::cout << Name;
		llvm::Value *V = AliasMgr[Name].val;
		if (V && AliasMgr[Name].isRef && currentFunction != NULL)
		{
			V = builder->CreateLoad(V->getType()->getNonOpaquePointerElementType(), V, "loadtmp");
		}
		if (!V)
		{
			std::cout << "Unknown variable name: " << Name << std::endl;
			return nullptr;
		}
		if (autoDeref && currentFunction != NULL)
			return builder->CreateLoad(V->getType()->getNonOpaquePointerElementType(), V, "loadtmp");
		return V;
	}

	llvm::Value *DeclareExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::Type *ty = this->type->codegen();
		llvm::Value *sizeval = llvm::ConstantInt::getIntegerValue(llvm::Type::getInt64Ty(*ctxt), llvm::APInt(64, (long)size, false));
		if (AliasMgr[name].val != NULL)
		{
			std::cout << "Warning: The variable '" << name << "' was already defined. Overwriting the previous value..." << std::endl;
			std::cout << "If this was not intentional, please make use of semicolons to better denote the end of each statement" << std::endl;
		}
		if (currentFunction == NULL || currentFunction == STATIC)
		{
			GlobalVarsAndFunctions->getOrInsertGlobal(name, ty);
			AliasMgr[name] = {(llvm::Value *)GlobalVarsAndFunctions->getNamedGlobal(name), this->type->isReference()};
			GlobalVarsAndFunctions->getNamedGlobal(name)->setInitializer(llvm::ConstantAggregateZero::get(ty));
		}
		else
		{
			AliasMgr[name] = {(llvm::Value *)builder->CreateAlloca(ty, sizeval, name), this->type->isReference()};
		}
		if (!lateinit && currentFunction != NULL)
			builder->CreateStore(llvm::ConstantAggregateZero::get(ty), AliasMgr[name].val);
		return AliasMgr[name].val;
	}
	// TODO: Decide whether or not deletion operators should be manditory for throwable objects
	// TODO: Improve object function solution. Current Implementation feels wrong
	// TODO: Double check ObjectConstructorCallExprAST.codegen(), make sure it works properly
	// TODO: Get labels working if possible
	// TODO: Implement forEach statements
	// TODO: Finish implementing list objects (Implement as a fat pointer struct)
	// TODO: Break BinaryStmtAST up into several ExprAST objects for each operator
	// TODO: Finalize boolean support
	// TODO: Add non-null dot operator using a question mark; "objptr?func()" == "if objptr ! (@objptr).func()"
	// TODO: Add implicit type casting (maybe)
	// TODO: Completely revamp data type system to be almost exclusively front-end
	// TODO: Find a better way to differentiate between char* and string (use structs maybe?)
	// TODO: Add Template objects (God help me)
	// TODO: Add arrays (God help me)
	// TODO: Add other modifier keywords (volatile, extern, etc...)
	// TODO: Make "auto" keyword work like C/C++
	// TODO: Add pointer arithmatic
	// TODO: Make strings safer

	llvm::Value *IncDecExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::Value *v = val->codegen(false);
		FunctionHeader op = getOperatorFromVals(prefix ? NULL : v, decrement ? "--" : "++", prefix ? v : NULL);
		if (op.func != NULL)
		{
			std::vector<llvm::Value *> args;
			args.push_back(v);
			return makeCallWithReferences(args, op);
		}
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

	llvm::Value *NotExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::Value *v = val->codegen();
		FunctionHeader op = getOperatorFromVals(NULL, "!", v);
		if (op.func != NULL)
		{
			std::vector<llvm::Value *> args;
			args.push_back(v);
			return makeCallWithReferences(args, op);
		}

		if (v->getType()->getTypeID() == llvm::Type::IntegerTyID && v->getType()->getIntegerBitWidth() == 1)
			return builder->CreateNot(v, "negationtmp");
		if (v->getType()->isStructTy())
		{
			// TODO: Implement operator overloading
		}
		std::cout << "Error: you're trying to take a negation of a non-boolean value!" << std::endl;
		return NULL;
	}

	llvm::Value *RefrenceExprAST::codegen(bool autoderef, llvm::Value *other)
	{
		return val->codegen(false);
	}

	llvm::Value *DeRefrenceExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::Value *v = val->codegen(autoDeref);
		FunctionHeader op = getOperatorFromVals(NULL, "@", v);
		if (op.func != NULL)
		{
			std::vector<llvm::Value *> args;
			args.push_back(v);
			return makeCallWithReferences(args, op);
		}
		if (v != NULL && v->getType()->isPointerTy())
			return builder->CreateLoad(v->getType()->getContainedType(0), v, "derefrencetmp");
		else
		{
			logError("Attempt to derefrence non-pointer type found: Aborting...");
			return NULL;
		}
	}

	llvm::Value *IndexExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::Value *bsval = bas->codegen(), *offv = offs->codegen();
		FunctionHeader op = getOperatorFromVals(bsval, "[", offv);
		if (op.func != NULL)
		{
			std::vector<llvm::Value *> args;
			args.push_back(bsval);
			args.push_back(offv);
			return makeCallWithReferences(args, op);
		}
		else if (!bsval->getType()->isPointerTy())
		{
			logError("Error: You tried to take an offset of a non-pointer, non-array type!\nMake sure that if you say 'variable[0]' (or similar), the type of 'variable' is a pointer or array!");
			return NULL;
		}
		else if (!offv->getType()->isIntegerTy())
		{
			logError("Error when trying to take an offset: The value provided must be an integer. Cast it to an int if possible");
			return NULL;
		}
		llvm::Value *indextmp = builder->CreateGEP(bsval->getType()->getContainedType(0), bsval, offv, "offsetval");
		if (autoDeref)
			return builder->CreateLoad(bsval->getType()->getNonOpaquePointerElementType(), indextmp, "loadtmp");
		return indextmp;
	}

	llvm::Value *TypeCastExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::Type *to = this->totype->codegen();
		llvm::Value *init = from->codegen();
		llvm::Instruction::CastOps op;

		FunctionHeader fh = operators[init->getType()]["AS"][to];
		if (fh.func == NULL)
			fh = operators[init->getType()->getPointerTo()]["AS"][to];
		if (fh.func != NULL)
		{
			std::vector<llvm::Value *> args;
			args.push_back(init);
			return makeCallWithReferences(args, fh);
		}

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
				logError("Attempted conversion failed: \nProvided data type cannot be cast to other primitive/struct, and no overloaded operator exists that would do that for us");
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
				logError("Attempted conversion failed: \nProvided data type cannot be cast to other primitive/struct, and no overloaded operator exists that would do that for us");
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
				logError("Attempted conversion failed: \nProvided data type cannot be cast to other primitive/struct, and no overloaded operator exists that would do that for us");
			}
			break;
		case (llvm::Type::PointerTyID):
			if (to->getTypeID() == llvm::Type::IntegerTyID)
				op = llvm::Instruction::CastOps::PtrToInt;
			else if (to->getTypeID() == llvm::Type::PointerTyID)
				op = llvm::Instruction::CastOps::BitCast;
			else
			{
				logError("Attempted conversion failed:\
                    Provided data type cannot be cast to other primitive/struct, and no overloaded operator exists that would do that for us");
			}
			break;
		default:
			logError("No known conversion (or defined operator overload) when converting " + AliasMgr.getTypeName(init->getType()) + " to a(n) " + AliasMgr.getTypeName(to));
			return NULL;
		}
		return builder->CreateCast(op, init, to, "typecasttmp");
	}
	llvm::Value *AndOrStmtAST::codegen(bool autoDeref, llvm::Value *other)
	{
		bool isLabel = other != NULL && other->getType()->isLabelTy();
		llvm::BasicBlock *falseBlock = NULL, *glblend = llvm::BasicBlock::Create(*ctxt, "andEvalBlock", currentFunction, isLabel ? (llvm::BasicBlock *)other : NULL);
		llvm::BasicBlock *trueBlock = isLabel ? (llvm::BasicBlock *)other : llvm::BasicBlock::Create(*ctxt, "andTrueShortCircuitBlock", currentFunction, glblend);
		llvm::BasicBlock *entryBlock = builder->GetInsertBlock();
		llvm::PHINode *phi = NULL;
		if (!isLabel)
		{
			builder->SetInsertPoint(glblend);
			phi = builder->CreatePHI(llvm::Type::getInt1Ty(*ctxt), 0, "conditionalAssignVal");
			builder->SetInsertPoint(entryBlock);
		}
		for (int i = 0; i < items.size(); i++)
		{
			auto &x = items[i];
			llvm::Value *v = x->codegen(true, trueBlock);
			if (!v->getType()->isLabelTy())
			{
				llvm::Value *boolval = v;
				v = llvm::BasicBlock::Create(*ctxt, "andend", currentFunction, falseBlock != NULL && falseBlock->getType()->isLabelTy() ? falseBlock : NULL);
				llvm::Value *zeroval = llvm::ConstantAggregateZero::get(boolval->getType());
				boolval = boolval->getType()->isIntegerTy() ? builder->CreateICmpNE(boolval, zeroval, "cmptmp") : builder->CreateFCmpOEQ(boolval, zeroval, "cmptmp");
				builder->CreateCondBr(boolval, operations[i] == AND ? trueBlock : falseBlock, operations[i] == AND ? falseBlock : trueBlock);
			}
			falseBlock = (llvm::BasicBlock *)v;
			builder->SetInsertPoint(falseBlock);
			builder->CreateBr(glblend);
			builder->SetInsertPoint(trueBlock);
			if (phi != NULL)
				phi->addIncoming(llvm::ConstantAggregateZero::get(llvm::Type::getInt1Ty(*ctxt)), falseBlock);
			if (i < items.size() - 1)
			{
				trueBlock = llvm::BasicBlock::Create(*ctxt, "andTrueBlock", currentFunction, glblend);
			}
		}
		if (isLabel)
		{
			builder->CreateBr((llvm::BasicBlock *)other);
		}
		else
		{
			phi->addIncoming(llvm::ConstantInt::getAllOnesValue(llvm::Type::getInt1Ty(*ctxt)), builder->GetInsertBlock());
			builder->CreateBr(glblend);
			builder->SetInsertPoint(glblend);
		}
		builder->SetInsertPoint(glblend);
		return !isLabel ? phi : (llvm::Value *)glblend;
	}

	llvm::Value *ComparisonStmtAST::codegen(bool autoDeref, llvm::Value *other)
	{
		bool isLabel = other != NULL && other->getType()->isLabelTy();
		// std::vector<llvm::Value*> oldcomparisons;
		// convert RHS, LHS to vector<llvm::Value*>
		llvm::Value *RHS = NULL, *LHS = NULL, *comparison = NULL;
		llvm::BasicBlock *shortCircuitEvalEnd = llvm::BasicBlock::Create(*ctxt, "ShortcircuitEvaluationEnd", currentFunction, isLabel ? (llvm::BasicBlock *)other : NULL),
						 *entryBlock = builder->GetInsertBlock(),
						 *ANDConditional = NULL, *ORConditional = NULL;
		llvm::PHINode *phi = NULL;
		if (!isLabel)
		{
			builder->SetInsertPoint(shortCircuitEvalEnd);
			phi = builder->CreatePHI(llvm::Type::getInt1Ty(*ctxt), 0, "conditionalAssignVal");
			builder->SetInsertPoint(entryBlock);
		}
		assert(operations.size() == items.size() - 1 && "Operations and operators do not line up!");
		for (int i = 0; i < operations.size(); i++)
		{
			if (isLabel)
			{
				if (i < operations.size() - 1)
				{
					ANDConditional = llvm::BasicBlock::Create(*ctxt, "ANDShortCircuitEvalBlock", currentFunction, shortCircuitEvalEnd);
				}
				else
					ANDConditional = (llvm::BasicBlock *)other;
			}
			else
			{
				// if(i == operations.size()-1) ANDConditional = shortCircuitEvalEnd;
				ANDConditional = llvm::BasicBlock::Create(*ctxt, "ANDShortCircuitEvalBlock", currentFunction, shortCircuitEvalEnd);
			}
			for (int LHSIndex = 0; LHSIndex < items[i].size(); LHSIndex++)
			{
				LHS = getCachedResult(i, LHSIndex);
				for (int RHSIndex = 0; RHSIndex < items[i + 1].size(); RHSIndex++)
				{
					RHS = getCachedResult(i + 1, RHSIndex);
					FunctionHeader fh;
					switch (operations[i])
					{
					case EQUALCMP:
					{
						fh = getOperatorFromVals(LHS, "==", RHS);
						if (fh.func != NULL)
						{
							std::vector<llvm::Value *> args;
							args.push_back(LHS);
							args.push_back(RHS);
							comparison = makeCallWithReferences(args, fh);
						}
						else
							comparison = LHS->getType()->isIntegerTy() ? builder->CreateICmpEQ(LHS, RHS, "cmptmp") : builder->CreateFCmpOEQ(LHS, RHS, "cmptmp");
						break;
					}
					case NOTEQUAL:
					{
						fh = getOperatorFromVals(LHS, "!=", RHS);
						if (fh.func != NULL)
						{
							std::vector<llvm::Value *> args;
							args.push_back(LHS);
							args.push_back(RHS);
							comparison = makeCallWithReferences(args, fh);
						}
						else
							comparison = LHS->getType()->isIntegerTy() ? builder->CreateICmpNE(LHS, RHS, "cmptmp") : builder->CreateFCmpONE(LHS, RHS, "cmptmp");
						break;
					}
					case GREATER:
					{
						fh = getOperatorFromVals(LHS, ">", RHS);
						if (fh.func != NULL)
						{
							std::vector<llvm::Value *> args;
							args.push_back(LHS);
							args.push_back(RHS);
							comparison = makeCallWithReferences(args, fh);
						}
						else
							comparison = LHS->getType()->isIntegerTy() ? builder->CreateICmpSGT(LHS, RHS, "cmptmp") : builder->CreateFCmpOGT(LHS, RHS, "cmptmp");
						break;
					}
					case GREATEREQUALS:
					{
						fh = getOperatorFromVals(LHS, ">=", RHS);
						if (fh.func != NULL)
						{
							std::vector<llvm::Value *> args;
							args.push_back(LHS);
							args.push_back(RHS);
							comparison = makeCallWithReferences(args, fh);
						}
						else
							comparison = LHS->getType()->isIntegerTy() ? builder->CreateICmpSGE(LHS, RHS, "cmptmp") : builder->CreateFCmpOGE(LHS, RHS, "cmptmp");
						break;
					}
					case LESS:
					{
						fh = getOperatorFromVals(LHS, "<", RHS);
						if (fh.func != NULL)
						{
							std::vector<llvm::Value *> args;
							args.push_back(LHS);
							args.push_back(RHS);
							comparison = makeCallWithReferences(args, fh);
						}
						else
							comparison = LHS->getType()->isIntegerTy() ? builder->CreateICmpSLT(LHS, RHS, "cmptmp") : builder->CreateFCmpOLT(LHS, RHS, "cmptmp");
						break;
					}
					case LESSEQUALS:
						fh = getOperatorFromVals(LHS, "<=", RHS);
						if (fh.func != NULL)
						{
							std::vector<llvm::Value *> args;
							args.push_back(LHS);
							args.push_back(RHS);
							comparison = makeCallWithReferences(args, fh);
						}
						else
							comparison = LHS->getType()->isIntegerTy() ? builder->CreateICmpSLE(LHS, RHS, "cmptmp") : builder->CreateFCmpOLE(LHS, RHS, "cmptmp");
						break;
					default:
						logError("Unknown comparision operator: " + keytokens[operations[i]]);
					}
					if ((RHSIndex < items[i + 1].size() - 1) || LHSIndex < items[i].size() - 1)
					{
						if (phi != NULL && (ANDConditional == shortCircuitEvalEnd || ORConditional == shortCircuitEvalEnd))
							phi->addIncoming(llvm::ConstantAggregateZero::get(llvm::Type::getInt1Ty(*ctxt)), builder->GetInsertBlock());
						ORConditional = llvm::BasicBlock::Create(*ctxt, "ORShortCircuitEvalBlock", currentFunction, ANDConditional);
						// if ((RHSIndex < items[i + 1].size() - 1) && LHSIndex < items[i].size() -1)
						builder->CreateCondBr(comparison, ANDConditional, ORConditional);
						builder->SetInsertPoint(ORConditional);
					}
				}
			}
			if (i < operations.size() - 1 || !isLabel)
			{
				if (phi != NULL)
					phi->addIncoming(llvm::ConstantAggregateZero::get(llvm::Type::getInt1Ty(*ctxt)), builder->GetInsertBlock());
				if (ANDConditional == shortCircuitEvalEnd)
				{
					ANDConditional = llvm::BasicBlock::Create(*ctxt, "ANDShortCircuitEvalBlock", currentFunction, shortCircuitEvalEnd);
					entryBlock = builder->GetInsertBlock();
					builder->SetInsertPoint(ANDConditional);
					builder->CreateBr(shortCircuitEvalEnd);
					builder->SetInsertPoint(entryBlock);
				}
				builder->CreateCondBr(comparison, ANDConditional, shortCircuitEvalEnd);
				// else builder->CreateBr(ANDConditional);
				builder->SetInsertPoint(ANDConditional);
			}
		}
		if (isLabel)
		{
			builder->CreateCondBr(comparison, (llvm::BasicBlock *)other, shortCircuitEvalEnd);
		}
		else
		{
			phi->addIncoming(llvm::ConstantInt::getAllOnesValue(llvm::Type::getInt1Ty(*ctxt)), builder->GetInsertBlock());
			builder->CreateBr(shortCircuitEvalEnd);
			builder->SetInsertPoint(shortCircuitEvalEnd);
			if (other != NULL)
				builder->CreateStore(phi, other);
		}
		builder->SetInsertPoint(shortCircuitEvalEnd);
		cache.clear(); // Clears cache in case this object gets codegenned again; in which case we want to re-codegen all args
		return !isLabel ? phi : (llvm::Value *)shortCircuitEvalEnd;
	}

	llvm::Value *IfExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::BasicBlock *start, *end;
		llvm::BasicBlock *glblend = llvm::BasicBlock::Create(*ctxt, "glblifend", currentFunction);
		int i = 0;
		for (i = 0; i < std::min(condition.size(), body.size()); i++)
		{
			start = llvm::BasicBlock::Create(*ctxt, "ifstart", currentFunction, glblend);
			// end = llvm::BasicBlock::Create(*ctxt, "ifend", currentFunction, glblend);
			llvm::Value *end = condition[i]->codegen(true, start);
			if (!end->getType()->isLabelTy())
			{
				llvm::Value *boolval = end;
				end = llvm::BasicBlock::Create(*ctxt, "ifend", currentFunction, glblend);
				llvm::Value *zeroval = llvm::ConstantAggregateZero::get(boolval->getType());
				boolval = boolval->getType()->isIntegerTy() ? builder->CreateICmpNE(boolval, zeroval, "cmptmp") : builder->CreateFCmpOEQ(boolval, zeroval, "cmptmp");
				builder->CreateCondBr(boolval, start, (llvm::BasicBlock *)end);
			}
			builder->SetInsertPoint(start);
			body[i]->codegen();
			builder->CreateBr(glblend);
			builder->SetInsertPoint((llvm::BasicBlock *)end);
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

	llvm::Value *SwitchExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		std::vector<llvm::BasicBlock *> bodBlocks;
		llvm::BasicBlock *glblend = llvm::BasicBlock::Create(*ctxt, "glblswitchend", currentFunction), *lastbody = glblend;
		llvm::SwitchInst *val = builder->CreateSwitch(comp->codegen(), glblend, cases.size());
		escapeBlock.push(std::pair<llvm::BasicBlock *, llvm::BasicBlock *>(glblend, lastbody));

		for (auto caseExpr = cases.rbegin(); caseExpr != cases.rend(); caseExpr++)
		{
			llvm::BasicBlock *currentbody = llvm::BasicBlock::Create(*ctxt, "body", currentFunction, lastbody);
			bodBlocks.push_back(currentbody);
			builder->SetInsertPoint(currentbody);
			caseExpr->second->codegen();
			builder->CreateBr(autoBr ? glblend : lastbody);
			for (auto &x : caseExpr->first)
				val->addCase((llvm::ConstantInt *)x->codegen(), currentbody);
			if (caseExpr->first.empty())
				val->setDefaultDest(currentbody);
			lastbody = currentbody;
			escapeBlock.pop();
			escapeBlock.push(std::pair<llvm::BasicBlock *, llvm::BasicBlock *>(glblend, lastbody));
		}
		builder->SetInsertPoint(glblend);
		escapeBlock.pop();
		return val;
	}

	llvm::Value *BreakExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		if (escapeBlock.empty())
			return llvm::ConstantInt::get(*ctxt, llvm::APInt(32, 0, true));
		// if (labelVal != "")
		return builder->CreateBr(escapeBlock.top().first);
	}
	llvm::Value *ContinueExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		if (escapeBlock.empty())
			return llvm::ConstantInt::get(*ctxt, llvm::APInt(32, 0, true));
		// if (labelVal != "")
		return builder->CreateBr(escapeBlock.top().second);
	}
	llvm::Value *SizeOfExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		if (!type && !target)
		{
			logError("Invalid target for sizeof: The expression you're taking the size of doesn't do what you think it does");
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
	llvm::Value *HeapExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		// initalize calloc(i64, i64) as the primary ways to allocate heap memory
		GlobalVarsAndFunctions->getOrInsertFunction("calloc", llvm::FunctionType::get(
																  llvm::Type::getInt8Ty(*ctxt)->getPointerTo(),
																  {llvm::Type::getInt64Ty(*ctxt), llvm::Type::getInt64Ty(*ctxt)},
																  false));

		llvm::Value *alloc = builder->CreateCall(GlobalVarsAndFunctions->getFunction("calloc"), {other, llvm::ConstantInt::getIntegerValue(llvm::Type::getInt64Ty(*ctxt), llvm::APInt(64, 1))}, "clearalloctmp");
		return alloc;
	}
	llvm::Value *DeleteExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::Value *freefunc = GlobalVarsAndFunctions->getOrInsertFunction("free", {llvm::Type::getInt8PtrTy(*ctxt)}, llvm::Type::getInt8PtrTy(*ctxt)).getCallee();
		llvm::Value *deletedthing = val->codegen(true);
		FunctionHeader op = getOperatorFromVals(NULL, "DELETE", deletedthing);
		if (op.func != NULL)
		{
			std::vector<llvm::Value *> args;
			args.push_back(deletedthing);
			return makeCallWithReferences(args, op);
		}
		if (!deletedthing->getType()->isPointerTy())
		{
			logError("Remember: Objects on the stack are accessed directly, you only need to delete pointers that point to the heap\nYou might've tried to delete an object (directly) by mistake rather than a pointer to that object");
			return NULL;
		}
		std::vector<llvm::Type *> argstmp;
		argstmp.push_back(deletedthing->getType());
		std::string nametmp = "destructor@" + AliasMgr.objects.getObjectName(deletedthing->getType()->getContainedType(0));
		// std::cout << AliasMgr.getTypeName(AliasMgr.objects.getObject(deletedthing->getType()->getContainedType(0)).ptr) << std::endl;
		deletedthing = builder->CreateBitCast(deletedthing, llvm::Type::getInt8PtrTy(*ctxt), "bitcasttmp");
		builder->CreateCall((llvm::Function *)freefunc, deletedthing, "freedValue");
		return NULL;
	}
	// I have a feeling this function needs to be revamped.
	llvm::Value *ForExprAST::codegen(bool autoDeref, llvm::Value *other)
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
	llvm::Value *RangeExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::Value *begin = start->codegen(), *fin = end->codegen(), *delta = step == NULL ? llvm::ConstantInt::getIntegerValue(llvm::Type::getInt32Ty(*ctxt), llvm::APInt(32, 1u)) : step->codegen();
		llvm::Value *ret;
		for (auto &x : start->throwables)
			this->throwables.insert(x);
		for (auto &x : end->throwables)
			this->throwables.insert(x);
		if (step != NULL)
		{
			for (auto &x : end->throwables)
				this->throwables.insert(x);
		}
		FunctionHeader op = getOperatorFromVals(begin, "RANGE", fin);
		if (op.func != NULL)
		{
			std::vector<llvm::Value *> args;
			args.push_back(begin);
			args.push_back(fin);
			return makeCallWithReferences(args, op);
		}
		if (llvm::isa<llvm::ConstantInt>(begin) && llvm::isa<llvm::ConstantInt>(fin) && llvm::isa<llvm::ConstantInt>(delta))
		{
			llvm::ConstantInt *cbegin = (llvm::ConstantInt *)begin, *cend = (llvm::ConstantInt *)fin, *cdelta = (llvm::ConstantInt *)delta;
			long bval = cbegin->getSExtValue(), eval = cend->getSExtValue(), dval = cdelta->getSExtValue();
			assert(dval != 0 && "Ranges must have a non-zero delta value. How are we supposed to move through a range when we can't move?");
			if (bval > eval && dval > 0)
			{
				dval *= -1;
			}
			else if (bval < eval && dval < 0)
			{
				dval *= -1;
			}
			std::vector<llvm::Constant *> constarray;
			for (long l = bval; (bval > eval && dval < 0 ? l > eval : l < eval); l += dval)
				constarray.push_back(llvm::ConstantInt::get(cbegin->getType(), llvm::APInt(cbegin->getBitWidth(), l)));
			llvm::ArrayType *arrtype = llvm::ArrayType::get(cbegin->getType(), constarray.size());
			llvm::GlobalVariable *glbl = new llvm::GlobalVariable(*GlobalVarsAndFunctions, (llvm::Type *)arrtype, true, llvm::GlobalValue::PrivateLinkage, llvm::ConstantArray::get(arrtype, constarray));
			ret = glbl;
		}
		else if (llvm::isa<llvm::ConstantFP>(begin) && llvm::isa<llvm::Constant>(fin) && llvm::isa<llvm::Constant>(delta))
		{
			llvm::ConstantFP *cbegin = (llvm::ConstantFP *)begin, *cend = fin->getType()->isFloatingPointTy() ? (llvm::ConstantFP *)fin : (llvm::ConstantFP *)builder->CreateCast(llvm::Instruction::CastOps::SIToFP, fin, cbegin->getType()), *cdelta = delta->getType()->isIntegerTy() ? (llvm::ConstantFP *)builder->CreateCast(llvm::Instruction::CastOps::SIToFP, delta, cbegin->getType()) : (llvm::ConstantFP *)delta;
			double bval = cbegin->getValue().convertToDouble(), eval = cend->getValue().convertToDouble(), dval = cdelta->getValue().convertToDouble();
			assert(dval != 0.0 && "Ranges must have a non-zero delta value. How are we supposed to move through a range when we can't move?");
			if (bval > eval && dval > 0.0)
			{
				dval *= -1.0;
			}
			else if (bval < eval && dval < 0.0)
			{
				dval *= -1.0;
			}
			std::vector<llvm::Constant *> constarray;
			for (double l = bval; (bval > eval && dval < 0.0 ? l > eval : l < eval); l += dval)
			{
				if (cbegin->getType()->isFloatTy())
				{
					constarray.push_back(llvm::ConstantFP::get(cbegin->getType(), llvm::APFloat((float)l)));
				}
				else
				{
					constarray.push_back(llvm::ConstantFP::get(cbegin->getType(), llvm::APFloat(l)));
				}
			}
			llvm::ArrayType *arrtype = llvm::ArrayType::get(cbegin->getType(), constarray.size());
			llvm::GlobalVariable *glbl = new llvm::GlobalVariable(*GlobalVarsAndFunctions, (llvm::Type *)arrtype, true, llvm::GlobalValue::PrivateLinkage, llvm::ConstantArray::get(arrtype, constarray));
			ret = glbl;
		}
		else
		{
			// TODO: Swap implementation with something similar to Python's RangeObject implementation
			assert(delta->getType() == begin->getType() && begin->getType() == fin->getType());

			llvm::Value *arrsize = builder->CreateFSub(builder->CreateCast(llvm::Instruction::CastOps::SIToFP, fin, llvm::Type::getDoubleTy(*ctxt)), builder->CreateCast(llvm::Instruction::CastOps::SIToFP, begin, llvm::Type::getDoubleTy(*ctxt)), "subtmp");
			arrsize = builder->CreateFDiv(arrsize, builder->CreateCast(llvm::Instruction::CastOps::SIToFP, delta, llvm::Type::getDoubleTy(*ctxt), "divtmp"), "divtmp");
			arrsize = builder->CreateCall(GlobalVarsAndFunctions->getOrInsertFunction("round", llvm::Type::getDoubleTy(*ctxt), llvm::Type::getDoubleTy(*ctxt)), {arrsize}, "calltmp");
			llvm::Value *arrsizeval = builder->CreateCast(llvm::Instruction::CastOps::FPToUI, arrsize, llvm::Type::getInt32Ty(*ctxt), "typecasttmp");
			llvm::Value *arrlocation = builder->CreateAlloca(begin->getType(), arrsizeval, "rangeallocation");
			llvm::BasicBlock *loopstart = llvm::BasicBlock::Create(*ctxt, "loopstart", currentFunction), *loopend = llvm::BasicBlock::Create(*ctxt, "loopend", currentFunction);
			llvm::Value *accum = builder->CreateAlloca(llvm::Type::getInt32Ty(*ctxt), NULL, "accumtmp");
			builder->CreateStore(llvm::ConstantAggregateZero::get(llvm::Type::getInt32Ty(*ctxt)), accum);
			llvm::Value *brcond = builder->CreateFCmp(llvm::CmpInst::Predicate::FCMP_OEQ, arrsize, llvm::ConstantAggregateZero::get(arrsize->getType()), "cmptmp");
			builder->CreateCondBr(brcond, loopend, loopstart);
			builder->SetInsertPoint(loopstart);
			llvm::Value *v = builder->CreateLoad(llvm::Type::getInt32Ty(*ctxt), accum, "accumtmp");
			llvm::Value *v2 = delta->getType()->isFloatingPointTy() ? builder->CreateFMul(delta, builder->CreateCast(llvm::Instruction::CastOps::SIToFP, v, delta->getType(), "casttmp"), "multmp") : builder->CreateMul(delta, v, "multmp");
			v2 = begin->getType()->isFloatingPointTy() ? builder->CreateFAdd(begin, v2, "addtmp") : builder->CreateAdd(begin, v2, "addtmp");
			llvm::Value *locationval = builder->CreateGEP(begin->getType(), arrlocation, v, "indextmp");
			builder->CreateStore(v2, locationval);
			v = builder->CreateAdd(v, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctxt), llvm::APInt(32, 1)), "addtmp");
			builder->CreateStore(v, accum);
			ret = arrlocation;
			llvm::Value *cond = builder->CreateCmp(llvm::CmpInst::Predicate::ICMP_SLT, v, arrsizeval, "cmptmp");
			builder->CreateCondBr(cond, loopstart, loopend);
			builder->SetInsertPoint(loopend);
		}
		return ret;
	}
	llvm::Value *ListExprAST::codegen(bool autoDeref, llvm::Value *other)
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

	llvm::Value *DebugPrintExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::Value *data = val->codegen();
		std::string placeholder = "Debug value (Line " + std::to_string(ln) + "): ";

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
	llvm::Value *RetStmtAST::codegen(bool autoDeref, llvm::Value *other)
	{
		if (DEBUGGING)
			std::cout << "return ";
		if (ret == NULL)
			return builder->CreateRetVoid();
		llvm::Value *retval = ret->codegen();
		// if (retval->getType() != currentFunction->getReturnType())
		//{
		//  builder->CreateCast() //Add type casting here
		// }

		return builder->CreateRet(retval);
	}

	llvm::Value *AssignStmtAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::Value *lval, *rval;
		// std::cout << std::hex << LHS << RHS <<endl;
		lval = lhs->codegen(false);
		for (auto &x : lhs->throwables)
			this->throwables.insert(x);
		rval = rhs->codegen(true, lval);
		for (auto &x : rhs->throwables)
			this->throwables.insert(x);
		if (lval != NULL && rval != NULL)
		{
			if (currentFunction == NULL)
			{
				if (!llvm::isa<llvm::GlobalVariable>(lval))
				{
					logError("Error: Global variable not found " + lval->getName().str());
					return NULL;
				}
				else if (!llvm::isa<llvm::Constant>(rval) || lval->getType() != rval->getType()->getPointerTo())
				{
					logError("Error: Global variable " + lval->getName().str() + " is not being set to a constant value");
					return NULL;
				}
				llvm::dyn_cast<llvm::GlobalVariable>(lval)->setInitializer(llvm::dyn_cast<llvm::Constant>(rval));
			}
			else
			{
				// if (lval->getType() != rval->getType()->getPointerTo())
				// {
				// 	logError("Error when attempting to assign a value: The type of the right side (" + AliasMgr.getTypeName(lval->getType()) + ") does not match the left side (" + AliasMgr.getTypeName(rval->getType()) + ").");
				// 	return NULL;
				// }
				builder->CreateStore(rval, lval);
			}
		}
		return rval;
	}
	llvm::Value *MultDivStmtAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::Value *lhs = LHS->codegen();
		llvm::Value *rhs = RHS->codegen();

		FunctionHeader op = getOperatorFromVals(lhs, div ? "/" : "*", rhs);
		if (op.func != NULL)
		{
			std::vector<llvm::Value *> args;
			args.push_back(lhs);
			args.push_back(rhs);
			return makeCallWithReferences(args, op);
		}
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
				std::string c = div ? "/" : "*";
				logError("Operator " + c + " never overloaded to support " + AliasMgr.getTypeName(lhs->getType()) + " and " + AliasMgr.getTypeName(rhs->getType()));
				return NULL;
			}
		}
		else
		{
			std::string s = (div ? "divide" : "multiply");
			logError("Error when attempting to " + s + " two types (these should match): " + AliasMgr.getTypeName(lhs->getType()) + " and " + AliasMgr.getTypeName(rhs->getType()));
			return NULL;
		}
	}
	llvm::Value *AddSubStmtAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::Value *lhs = LHS->codegen();
		llvm::Value *rhs = RHS->codegen();

		FunctionHeader op = getOperatorFromVals(lhs, sub? "-" : "+", rhs);
		if (op.func != NULL)
		{
			std::vector<llvm::Value *> args;
			args.push_back(lhs);
			args.push_back(rhs);
			return makeCallWithReferences(args, op);
		}
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
			std::string s = sub ? "-" : "+";
			logError("Operator " + s + " never overloaded to support " + AliasMgr.getTypeName(lhs->getType()) + " and " + AliasMgr.getTypeName(rhs->getType()));
			return NULL;
		}
	}
	llvm::Value *PowModStmtAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::Type *longty = llvm::IntegerType::getInt32Ty(*ctxt);
		llvm::Type *doublety = llvm::Type::getDoubleTy(*ctxt);
		llvm::Value *lhs = LHS->codegen();
		llvm::Value *rhs = RHS->codegen();

		FunctionHeader op = getOperatorFromVals(lhs, mod? "%" : "^", rhs);
		if (op.func != NULL)
		{
			std::vector<llvm::Value *> args;
			args.push_back(lhs);
			args.push_back(rhs);
			return makeCallWithReferences(args, op);
		}
		if (!mod && (rhs->getType()->isFloatingPointTy() || rhs->getType()->isIntegerTy()))
		{
			GlobalVarsAndFunctions->getOrInsertFunction("pow",
														llvm::FunctionType::get(doublety, {doublety, doublety}, false));
		}
		llvm::Function *powfunc = GlobalVarsAndFunctions->getFunction("pow");

		switch (lhs->getType()->getTypeID())
		{
		case llvm::Type::IntegerTyID:
			// TODO: Create type alignment system
			if (mod)
				return builder->CreateSRem(lhs, rhs, "modtmp");
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
				logError("Error when attempting to raise a number to a power: the right-hand-side is not an integer or float");
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
			return mod ? builder->CreateFRem(lhs, rhs) : builder->CreateCall(powfunc, {lhs, rhs}, "powtmp");
		}
		default:
			std::string s = mod ? "%" : "^";
			logError("Operator " + s + " never overloaded to support " + AliasMgr.getTypeName(lhs->getType()) + " and " + AliasMgr.getTypeName(rhs->getType()));
			return NULL;
		}
	}
	llvm::Value *BinaryStmtAST::codegen(bool autoDeref, llvm::Value *other)
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
	llvm::Value *TryStmtAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::FunctionCallee typeidfor = GlobalVarsAndFunctions->getOrInsertFunction("llvm.eh.typeid.for", llvm::FunctionType::get(llvm::Type::getInt32Ty(*ctxt), {llvm::Type::getInt8PtrTy(*ctxt)}, false));
		llvm::FunctionCallee personalityfunc = GlobalVarsAndFunctions->getOrInsertFunction("__gxx_personality_v0", llvm::FunctionType::get(llvm::Type::getInt32Ty(*ctxt), {llvm::Type::getInt8PtrTy(*ctxt)}, false));
		llvm::FunctionCallee begin_catch = GlobalVarsAndFunctions->getOrInsertFunction("__cxa_begin_catch", llvm::FunctionType::get(llvm::Type::getInt8PtrTy(*ctxt), {llvm::Type::getInt8PtrTy(*ctxt)}, false));
		llvm::FunctionCallee end_catch = GlobalVarsAndFunctions->getOrInsertFunction("__cxa_end_catch", llvm::FunctionType::get(llvm::Type::getVoidTy(*ctxt), false));
		currentFunction->setPersonalityFn((llvm::Constant *)personalityfunc.getCallee());
		std::vector<llvm::BasicBlock *> catchBlocks;
		llvm::BasicBlock *tryBlock = llvm::BasicBlock::Create(*ctxt, "tryentry", currentFunction), *tryEnd = llvm::BasicBlock::Create(*ctxt, "tryend", currentFunction);
		llvm::BasicBlock *landingpad = llvm::BasicBlock::Create(*ctxt, "landingpad", currentFunction, tryEnd), *oldLP = currentUnwindBlock;
		builder->CreateBr(tryBlock);
		builder->SetInsertPoint(tryBlock);
		currentUnwindBlock = landingpad;
		llvm::Value *ballval = body->codegen();
		builder->CreateBr(tryEnd);
		builder->SetInsertPoint(landingpad);

		llvm::Type *errorMetadata = llvm::StructType::get(*ctxt, {llvm::Type::getInt8PtrTy(*ctxt), llvm::Type::getInt32Ty(*ctxt)});
		llvm::LandingPadInst *lpval = builder->CreateLandingPad(errorMetadata, 1, "catchBlockLP");
		llvm::Value *extractedval = builder->CreateExtractValue(lpval, {0u}, "extractedval"),
					*extractedint = builder->CreateExtractValue(lpval, {1u}, "typeIDcode");
		llvm::BasicBlock *catchCheckBlock = llvm::BasicBlock::Create(*ctxt, "catchcheck", currentFunction, tryEnd);
		builder->CreateBr(catchCheckBlock);
		llvm::BasicBlock *nextblock = tryEnd;
		llvm::StructType *errorMetadataType = llvm::StructType::get(llvm::Type::getInt8PtrTy(*ctxt), llvm::Type::getInt8PtrTy(*ctxt));

		if (!catchStmts.empty())
		{
			for (auto x = catchStmts.rbegin(); x != catchStmts.rend(); ++x)
			{
				assert(x->first != NULL && x->first->codegen() != NULL && "Unable to generate class for object");

				if (classInfoVals[x->first->codegen()] == NULL)
				{
					std::string thrownerrorname = "_error@" + AliasMgr.getTypeName(x->first->codegen(), false);
					llvm::GlobalVariable *thrownerror = (llvm::GlobalVariable *)GlobalVarsAndFunctions->getOrInsertGlobal(thrownerrorname, errorMetadataType);
					classInfoVals[x->first->codegen()] = thrownerror;
					llvm::Value *classinfo = GlobalVarsAndFunctions->getOrInsertGlobal("_ZTVN10__cxxabiv117__class_type_infoE", llvm::Type::getInt8PtrTy(*ctxt));
					classinfo = builder->CreateGEP(llvm::Type::getInt8PtrTy(*ctxt), classinfo, llvm::ConstantInt::get(llvm::Type::getInt64Ty(*ctxt), llvm::APInt(64, 2)));
					classinfo = builder->CreateBitCast(classinfo, llvm::Type::getInt8PtrTy(*ctxt));
					thrownerror->setInitializer(llvm::ConstantStruct::get(errorMetadataType, (llvm::Constant *)classinfo, builder->CreateGlobalStringPtr(AliasMgr.getTypeName(x->first->codegen(), false), AliasMgr.getTypeName(x->first->codegen(), false))));
					// logError(classinfo == NULL ? "true" : "false");
				}

				assert(classInfoVals[x->first->codegen()] != NULL && "Unable to find class info for object; make sure your code throws an instance of each type it tries to catch. This error happens whenever you try to catch an error that cannot be thrown at this point in the code");
				lpval->addClause((llvm::Constant *)builder->CreateBitCast(classInfoVals[x->first->codegen()], llvm::Type::getInt8PtrTy(*ctxt)));
				if (nextblock != tryEnd)
					catchCheckBlock = llvm::BasicBlock::Create(*ctxt, "catchcheck", currentFunction, tryEnd);
				llvm::BasicBlock *catchBlock = llvm::BasicBlock::Create(*ctxt, "catchblock", currentFunction, tryEnd);
				builder->SetInsertPoint(catchCheckBlock);
				llvm::Value *objTypeID = builder->CreateCall(typeidfor, {builder->CreateBitCast(classInfoVals[x->first->codegen()], llvm::Type::getInt8PtrTy(*ctxt))}, "getTypeID");
				llvm::Value *condition = builder->CreateICmpEQ(extractedint, objTypeID, "cmptmp");
				builder->CreateCondBr(condition, catchBlock, nextblock);
				nextblock = catchCheckBlock;

				builder->SetInsertPoint(catchBlock);
				AliasMgr[x->second.second] = {(llvm::Value *)builder->CreateBitCast(builder->CreateCall(begin_catch, {extractedval}), x->first->codegen()->getPointerTo(), "errorptr"), false};
				llvm::Value* caughtError = x->second.first->codegen(); 
				FunctionHeader fh = getOperatorFromVals(NULL, "CATCH", caughtError); 
				if (fh.func != NULL){
					std::vector<llvm::Value*> args; 
					args.push_back(caughtError); 
					makeCallWithReferences(args, fh); 
				}
				builder->CreateCall(end_catch, {});
				builder->CreateBr(tryEnd);
				AliasMgr[x->second.second] = {NULL, false};
				catchBlocks.push_back(catchBlock);
			}
		}
		else
		{
			for (auto &x : body->throwables)
			{
				assert(x != NULL && "Unable to generate class for object");
				// TODO: Remove this assertion, generate the error metadata here instead

				if (classInfoVals[x] == NULL)
				{
					std::string thrownerrorname = "_error@" + AliasMgr.getTypeName(x, false);
					llvm::GlobalVariable *thrownerror = (llvm::GlobalVariable *)GlobalVarsAndFunctions->getOrInsertGlobal(thrownerrorname, errorMetadataType);
					llvm::Value *classinfo = GlobalVarsAndFunctions->getOrInsertGlobal("_ZTVN10__cxxabiv117__class_type_infoE", llvm::Type::getInt8PtrTy(*ctxt));
					classinfo = builder->CreateGEP(llvm::Type::getInt8PtrTy(*ctxt), classinfo, llvm::ConstantInt::get(llvm::Type::getInt64Ty(*ctxt), llvm::APInt(64, 2)));
					classinfo = builder->CreateBitCast(classinfo, llvm::Type::getInt8PtrTy(*ctxt));
					thrownerror->setInitializer(llvm::ConstantStruct::get(errorMetadataType, (llvm::Constant *)classinfo, builder->CreateGlobalStringPtr(AliasMgr.getTypeName(x, false))));
					classInfoVals[x] = thrownerror;
				}

				lpval->addClause((llvm::Constant *)builder->CreateBitCast(classInfoVals[x], llvm::Type::getInt8PtrTy(*ctxt)));
				if (nextblock != tryEnd)
					catchCheckBlock = llvm::BasicBlock::Create(*ctxt, "catchcheck", currentFunction, tryEnd);
				llvm::BasicBlock *catchBlock = llvm::BasicBlock::Create(*ctxt, "catchblock", currentFunction, tryEnd);
				builder->SetInsertPoint(catchCheckBlock);
				llvm::Value *objTypeID = builder->CreateCall(typeidfor, {builder->CreateBitCast(classInfoVals[x], llvm::Type::getInt8PtrTy(*ctxt))}, "getTypeID");
				llvm::Value *condition = builder->CreateICmpEQ(extractedint, objTypeID, "cmptmp");
				builder->CreateCondBr(condition, catchBlock, nextblock);
				nextblock = catchCheckBlock;

				builder->SetInsertPoint(catchBlock);
				llvm::Value *errorval = builder->CreateCall(begin_catch, {extractedval}, "error");
				FunctionHeader  fh = getOperatorFromTypes(NULL, "CATCH", x); 
				if (fh.func != NULL){
					std::vector<llvm::Value*> args; 
					args.push_back(builder->CreateBitCast(errorval, x->getPointerTo())); 
					makeCallWithReferences(args, fh); 
				}
				builder->CreateCall(end_catch);
				builder->CreateBr(tryEnd);
				catchBlocks.push_back(catchBlock);
			}
		}
		builder->SetInsertPoint(tryEnd);
		currentUnwindBlock = oldLP;
		return tryBlock;
	}

	llvm::Value *ThrowStmtAST::codegen(bool autoDeref, llvm::Value *other)
	{
		GlobalVarsAndFunctions->getOrInsertFunction("__cxa_allocate_exception", llvm::FunctionType::get(llvm::Type::getInt8PtrTy(*ctxt), {llvm::Type::getInt64Ty(*ctxt)}, false));
		GlobalVarsAndFunctions->getOrInsertFunction("__cxa_throw", llvm::FunctionType::get(llvm::Type::getVoidTy(*ctxt), {llvm::Type::getInt8PtrTy(*ctxt), llvm::Type::getInt8PtrTy(*ctxt), llvm::Type::getInt8PtrTy(*ctxt)}, false));
		llvm::StructType *errorMetadataType = llvm::StructType::get(llvm::Type::getInt8PtrTy(*ctxt), llvm::Type::getInt8PtrTy(*ctxt));
		llvm::Value *ballval = ball->codegen();
		this->throwables.insert(ballval->getType());
		assert(ballval != NULL && "Fatal error when trying to throw an error: Object provided failed to return a useful value");
		llvm::Value *classinfo = GlobalVarsAndFunctions->getOrInsertGlobal("_ZTVN10__cxxabiv117__class_type_infoE", llvm::Type::getInt8PtrTy(*ctxt));
		assert(classinfo != NULL && "Fatal error trying to generate throw statement");
		llvm::Value *error = builder->CreateCall(GlobalVarsAndFunctions->getFunction("__cxa_allocate_exception"), {AliasMgr.getTypeSize(ballval->getType(), ctxt, DataLayout)}, "errorAllocatmp");
		llvm::Value *error2 = builder->CreateBitCast(error, ballval->getType()->getPointerTo(), "bitcasttmp");
		builder->CreateStore(ballval, error2);
		assert(error != NULL && "Fatal error creating space for the error you're trying to throw");
		llvm::Constant *typeStringVal = NULL;
		if (classInfoVals[ballval->getType()] == NULL)
		{
			typeStringVal = builder->CreateGlobalStringPtr(AliasMgr.getTypeName(ballval->getType(), false), AliasMgr.getTypeName(ballval->getType(), false));
		}
		else
			typeStringVal = GlobalVarsAndFunctions->getGlobalVariable(AliasMgr.getTypeName(ballval->getType(), false), true);
		assert(typeStringVal != NULL && "Fatal error trying to generate throw statement");
		std::string thrownerrorname = "_error@" + AliasMgr.getTypeName(ballval->getType(), false);
		if (classInfoVals[ballval->getType()] == NULL)
		{
			llvm::GlobalVariable *thrownerror = (llvm::GlobalVariable *)GlobalVarsAndFunctions->getOrInsertGlobal(thrownerrorname, errorMetadataType);
			// std::cout << AliasMgr.getTypeName(ballval->getType(), true) <<endl;
			classInfoVals[ballval->getType()] = thrownerror;
			assert(thrownerror != NULL && "Fatal error trying to generate throw statement");
			// initialize the globals:
			classinfo = builder->CreateGEP(llvm::Type::getInt8PtrTy(*ctxt), classinfo, llvm::ConstantInt::get(llvm::Type::getInt64Ty(*ctxt), llvm::APInt(64, 2)));
			classinfo = builder->CreateBitCast(classinfo, llvm::Type::getInt8PtrTy(*ctxt));
			thrownerror->setInitializer(llvm::ConstantStruct::get(errorMetadataType, (llvm::Constant *)classinfo, typeStringVal));
			// builder->CreateStore(ballval, error, "storetmp");
		}
		llvm::Value *deleter = getOperatorFromVals( NULL, "DELETE", ballval).func; 
		// assert(deleter ! && "Fatal error: You tried to throw an object that has no deletion operator");

		return builder->CreateCall(GlobalVarsAndFunctions->getFunction("__cxa_throw"),
								   {error,
									builder->CreateBitCast(classInfoVals[ballval->getType()], llvm::Type::getInt8PtrTy(*ctxt)),
									builder->CreateBitCast(deleter, llvm::Type::getInt8PtrTy(*ctxt))});
	}
	llvm::Value *PrintStmtAST::codegen(bool autoDeref, llvm::Value *other)
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

	llvm::Value *CodeBlockAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::Value *ret;
		for (int i = 0; i < Contents.size(); i++)
		{
			ret = Contents[i]->codegen();
			for (auto &x : Contents[i]->throwables)
				this->throwables.insert(x);
			if (DEBUGGING)
				std::cout << std::endl;
		};
		return ret;
	}
	llvm::Value *MemberAccessExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		// return NULL;
		llvm::Value *lhs = base->codegen(dereferenceParent);
		auto returnTy = AliasMgr(lhs->getType()->getContainedType(0), member);
		if (returnTy.index == -1)
		{
			logError("No object member or function with name '" + member + "' found in object of type: " + AliasMgr.getTypeName(lhs->getType()->getContainedType(0)));
			return NULL;
		}
		llvm::Constant *offset = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctxt), llvm::APInt(32, returnTy.index));
		llvm::Constant *objIndex = llvm::ConstantInt::get(*ctxt, llvm::APInt(32, 0, false));
		llvm::Value *gep = builder->CreateInBoundsGEP(lhs->getType()->getContainedType(0), lhs, llvm::ArrayRef<llvm::Value *>({objIndex, offset}), "memberaccess");
		return autoDeref ? builder->CreateLoad(returnTy.type, gep, "LoadTmp") : gep;
	}
	llvm::Value *CallExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		if (!AliasMgr.functions.hasAlias(Callee))
		{
			logError("A function with name was never declared: " + Callee);
		}
		std::vector<llvm::Value *> ArgsV;
		std::vector<llvm::Type *> ArgsT;
		for (unsigned i = 0, e = Args.size(); i != e; ++i)
		{
			ArgsV.push_back(Args[i]->codegen(false));
			for (auto &x : Args[i]->throwables)
				this->throwables.insert(x);
			if (!ArgsV.back())
			{
				assert(false && "Error saving function args");
			}
			ArgsT.push_back(ArgsV.back()->getType());
		}

		// Look up the name in the global module table.
		FunctionHeader CalleeF = AliasMgr.functions.getFunctionObject(Callee, ArgsT);
		for (auto &x : CalleeF.throwableTypes)
			this->throwables.insert(x);
		return makeCallWithReferences(ArgsV, CalleeF);
	}

	llvm::Value *ObjectFunctionCallExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::Value *parval = parent->codegen(dereferenceParent);
		std::vector<llvm::Value *> ArgsV;
		std::vector<llvm::Type *> ArgsT;
		for (unsigned i = 0, e = Args.size(); i != e; ++i)
		{
			ArgsV.push_back(Args[i]->codegen(false));
			for (auto &x : Args[i]->throwables)
				this->throwables.insert(x);
			if (!ArgsV.back())
			{
				assert(false && "Error saving function args");
			}
			ArgsT.push_back(ArgsV.back()->getType());
		}
		ArgsT.insert(ArgsT.begin(), parval->getType()); 
		ArgsV.insert(ArgsV.begin(), parval);
		FunctionHeader CalleeF = AliasMgr.functions.getFunctionObject(Callee, ArgsT);
		for (auto &x : CalleeF.throwableTypes)
			this->throwables.insert(x);
		for (auto &x : parent->throwables)
			this->throwables.insert(x);
		return makeCallWithReferences(ArgsV, CalleeF, true);
	}

	llvm::Value *ObjectConstructorCallExprAST::codegen(bool autoDeref, llvm::Value *other)
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
		if (other == NULL)
			other = builder->CreateAlloca(TargetType, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctxt), llvm::APInt(1, 32)), "objConstructorTmp");
		if (other->getType() != TargetType->getPointerTo())
		{
			logError("Error when attempting to assign a constructor value: The type of the left side (" + AliasMgr.getTypeName(other->getType()) + ") does not match the right side (" + AliasMgr.getTypeName(target == NULL ? TargetType : TargetType->getPointerTo()) + ").");
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
			ArgsV.push_back(Args[i]->codegen(false));
			if (!ArgsV.back())
			{
				std::cout << "Error saving function args";
				return nullptr;
			}
			ArgsT.push_back(ArgsV.back()->getType());
		}

		FunctionHeader CalleeF = (Callee == "") ? AliasMgr(TargetType, ArgsT) : AliasMgr.functions.getFunctionObject(Callee, ArgsT);

		for (int i = 0; i < CalleeF.args.size(); i++)
		{
			if (!CalleeF.args[i].isRef && ArgsT[i]->isPointerTy() && ArgsT[i] != CalleeF.args[i].ty)
				ArgsV[i] = builder->CreateLoad(ArgsT[i]->getNonOpaquePointerElementType(), ArgsV[i], "dereftmp");
		}

		builder->CreateCall(CalleeF.func, ArgsV, "calltmp");
		return NULL;
	}

	llvm::Value *ConstructorExprAST::codegen(bool autoderef, llvm::Value *other)
	{
		std::vector<std::string> argnames;
		std::vector<llvm::Type *> argtypes;
		std::unique_ptr<TypeExpr> retType = std::make_unique<StructTypeExpr>(objName);
		retType = std::make_unique<ReferenceToTypeExpr>(retType);
		argslist.emplace(argslist.begin(), Variable("this", retType));
		for (auto &x : argslist)
		{
			argnames.push_back(x.name);
			argtypes.push_back(x.ty->codegen());
		}
		llvm::FunctionType *FT =
			llvm::FunctionType::get(argtypes[0], argtypes, false);
		llvm::Function *lastfunc = currentFunction;
		currentFunction =
			llvm::Function::Create(FT, llvm::Function::ExternalLinkage, objName + ".constructor", GlobalVarsAndFunctions.get());
		llvm::BasicBlock *entry = llvm::BasicBlock::Create(*ctxt, "entry", currentFunction);
		builder->SetInsertPoint(entry);

		unsigned Idx = 0;
		for (auto &Arg : currentFunction->args())
		{
			std::string name = argnames[Idx++];
			Arg.setName(name);
			llvm::Value *storedvar = builder->CreateAlloca(Arg.getType(), NULL, Arg.getName());
			builder->CreateStore(&Arg, storedvar);
			AliasMgr[name] = {storedvar, argslist[Idx - 1].ty->isReference()};
		}

		llvm::Value *RetVal = bod->codegen();
		builder->CreateRet(builder->CreateLoad(AliasMgr["this"].val->getType()->getNonOpaquePointerElementType(), AliasMgr["this"].val));
		// Validate the generated code, checking for consistency.
		verifyFunction(*currentFunction);
		if (DEBUGGING)
			std::cout << "//end of " << objName << "'s constructor" << std::endl;
		// remove the arguments now that they're out of scope
		for (auto &Arg : currentFunction->args())
		{
			AliasMgr[std::string(Arg.getName())] = {NULL, false};
			// dtypes[std::string(Arg.getName())] = NULL;
		}
		// for (auto x : AliasMgr.structTypes[objName].members)
		// {
		// 	AliasMgr[x.first] = NULL; //builder->CreateGEP(x.second.second, (llvm::Value *)thisfunc->getArg(0), offset, "ObjMemberAccessTmp");
		// }
		AliasMgr.functions.addFunction(objName, currentFunction, argslist);
		AliasMgr.objects.addConstructor(AliasMgr(objName), currentFunction, argslist);
		llvm::Function *thisfunc = currentFunction;
		currentFunction = lastfunc;
		if (currentFunction != NULL)
			builder->SetInsertPoint(&currentFunction->getBasicBlockList().back());
		argslist.erase(argslist.begin());
		return thisfunc;
	}

	llvm::StructType *ObjectHeaderExpr::codegen(bool autoderef, llvm::Value *other)
	{
		// TODO: This could cause bugs later. Find a better (non-bandaid) solution
		llvm::StructType *ty = llvm::StructType::getTypeByName(*ctxt, name);
		ty = ty == NULL ? llvm::StructType::create(*ctxt, name) : ty;
		AliasMgr.objects.addObject(name, ty);
		return ty;
	}
	llvm::Value *ObjectExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::StructType *ty = base.codegen();
		std::vector<llvm::Type *> types;
		std::vector<std::string> names;

		if (!base.templates.empty())
		{
			TemplateMgr.insertTemplate(base.name, base.templates, vars, functions);
			for (auto &x : base.templates)
			{
				AliasMgr.objects.removeObject(x->getName());
			}
			return NULL;
		}

		for (auto &var : vars)
		{
			names.push_back(var.name);
			types.push_back(var.ty->codegen());
		}

		if (!types.empty())
			ty->setBody(types);

		if (base.templates.empty())
			AliasMgr.objects.addObjectMembers(base.name, types, names);
		else
		{
		}
		if (!ops.empty())
			for (auto &func : ops)
			{
				func->codegen();
			}
		if (!functions.empty())
			for (auto &func : functions)
			{
				func->codegen();
			}
		for (auto &x : base.templates)
		{
			AliasMgr.objects.removeObject(x->getName());
		}
		return NULL;
	}
	std::vector<llvm::Type *> PrototypeAST::getArgTypes()
	{
		std::vector<llvm::Type *> ret;
		if (parent != "")
		{
			ret.push_back(AliasMgr(parent));
		}
		for (auto &x : Args)
			ret.push_back(x.ty->codegen());
		// if(retType != NULL)ret.emplace(ret.begin(), retType->codegen());
		return ret;
	}

	llvm::Function *PrototypeAST::codegen(bool autoDeref, llvm::Value *other)
	{
		std::vector<std::string> Argnames;
		std::vector<llvm::Type *> Argt;
		std::vector<llvm::Type *> Errt;
		if (parent != "")
		{
			std::string name = "this";
			std::unique_ptr<TypeExpr> ty = std::make_unique<StructTypeExpr>(parent);
			ty = std::make_unique<ReferenceToTypeExpr>(ty);
			Args.insert(Args.begin(), Variable(name, ty)); 
		}

		for (auto &x : Args)
		{
			Argnames.push_back(x.name);
			Argt.push_back(x.ty->codegen());
		}
		for (auto &x : throwableTypes)
		{
			Errt.push_back(x->codegen());
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
		AliasMgr.functions.addFunction(Name, F, Args, Errt, retType->isReference());
		return F;
	}

	llvm::Value *FunctionAST::codegen(bool autoDeref, llvm::Value *other)
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
			logError("Function Cannot be redefined: " + currentFunction->getName().str());
			currentFunction = prevFunction;
			return NULL;
		}

		llvm::BasicBlock *BB = llvm::BasicBlock::Create(*ctxt, "entry", currentFunction);
		builder->SetInsertPoint(BB);
		// Record the function arguments in the Named Values map. Check if they're references and act accordingly.
		std::vector<bool> areReferences;
		if (Proto->parent != "")
			areReferences.push_back(true);
		for (auto &arg : Proto->Args)
			areReferences.push_back(arg.ty->isReference());
		for (auto &Arg : currentFunction->args())
		{
			int argno = Arg.getArgNo();
			llvm::Value *storedvar = builder->CreateAlloca(Arg.getType(), NULL, Arg.getName());
			builder->CreateStore(&Arg, storedvar);
			std::string name = std::string(Arg.getName());
			// std::cout << Proto->Name +" " << Proto->Args.size() << " " <<Arg.getArgNo() << " " << currentFunction->arg_size() <<endl;
			AliasMgr[name] = {storedvar, areReferences[argno]};
		}

		llvm::Instruction *currentEntry = &BB->getIterator()->back();
		llvm::Value *RetVal = Body == NULL ? NULL : Body->codegen();

		if (RetVal != NULL && (!RetVal->getType()->isPointerTy() || !RetVal->getType()->getNonOpaquePointerElementType()->isFunctionTy()))
		{
			if (currentFunction->getReturnType()->isVoidTy())
				builder->CreateRetVoid();
			else
				builder->CreateRet(llvm::ConstantAggregateZero::get(currentFunction->getReturnType()));
			// Validate the generated code, checking for consistency.
			verifyFunction(*currentFunction);
			if (DEBUGGING)
				std::cout << "//end of " << Proto->getName() << std::endl;
			// remove the arguments now that they're out of scope
		}
		else
		{
			currentFunction->deleteBody();
		}
		for (auto &Arg : currentFunction->args())
		{
			AliasMgr[std::string(Arg.getName())] = {NULL, false};
			// dtypes[std::string(Arg.getName())] ;
		}
		currentFunction = prevFunction;
		if (currentFunction != NULL)
			builder->SetInsertPoint(&currentFunction->getBasicBlockList().back());

		return AliasMgr.functions.getFunction(Proto->Name, argtypes);
	}

	llvm::Value *OperatorOverloadAST::codegen(bool autoDeref, llvm::Value *other)
	{

		name += oper + "_";
		llvm::raw_string_ostream stringstream(name);
		std::vector<llvm::Type *> nullableArgtypes;
		for (auto t = args.begin(); t < args.end(); t++)
		{
			if (t->ty == NULL)
			{
				nullableArgtypes.push_back(NULL);
				args.erase(t);
				t--;
				continue;
			}

			llvm::Type *typev = t->ty->codegen();
			nullableArgtypes.push_back(typev);
			if (typev->isStructTy())
				name += typev->getStructName();
			else
				typev->print(stringstream);
			name += '_';
		}
		Proto = PrototypeAST(name, args, retType);
		llvm::Function *prevFunction = currentFunction;
		// Segment stolen from Proto.codegen()
		std::vector<std::string> Argnames;
		std::vector<llvm::Type *> Argt;
		for (auto &x : Proto.Args)
		{
			if (x.ty == NULL)
				continue;
			Argnames.push_back(x.name);
			Argt.push_back(x.ty->codegen());
		}
		llvm::FunctionType *FT =
			llvm::FunctionType::get(Proto.retType->codegen(), Argt, false);
		currentFunction =
			llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Proto.Name, GlobalVarsAndFunctions.get());
		unsigned Idx = 0;
		for (auto &Arg : currentFunction->args())
		{
			Arg.setName(Argnames[Idx++]);
		}
		// end of segment stolen from Proto.codegen()

		if (!currentFunction)
		{
			currentFunction = prevFunction;
			return NULL;
		}
		if (!currentFunction->empty())
		{
			logError("Operator Cannot be redefined: " + currentFunction->getName().str());
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
			Variable &v = Proto.Args[Arg.getArgNo()];
			AliasMgr[name] = {storedvar, v.ty->isReference()};
			// dtypes[name] = Arg.getType();
		}
		llvm::Instruction *currentEntry = &BB->getIterator()->back();
		llvm::Value *RetVal = Body == NULL ? NULL : Body->codegen();

		if (RetVal != NULL && (!RetVal->getType()->isPointerTy() || !RetVal->getType()->getNonOpaquePointerElementType()->isFunctionTy()))
		{
			// Validate the generated code, checking for consistency.
			verifyFunction(*currentFunction);
			if (DEBUGGING)
				std::cout << "//end of " << Proto.getName() << std::endl;
			transform(oper.begin(), oper.end(), oper.begin(), ::toupper);
		}
		else
		{
			currentFunction->deleteBody();
		}
		// std::cout << AliasMgr.getTypeName(nullableArgtypes[0]) << " " << oper << " " << AliasMgr.getTypeName(nullableArgtypes[1]) <<endl;
		operators[nullableArgtypes[0]][oper][nullableArgtypes[1]] = FunctionHeader(args, currentFunction, this->retType->isReference());
		// remove the arguments now that they're out of scope
		for (auto &Arg : currentFunction->args())
		{
			AliasMgr[std::string(Arg.getName())] = {NULL, false};
			// dtypes[std::string(Arg.getName())] ;
		}
		currentFunction = prevFunction;
		if (currentFunction != NULL)
			builder->SetInsertPoint(&currentFunction->getBasicBlockList().back());
		return getOperatorFromTypes(nullableArgtypes[0], oper, nullableArgtypes[1]).func;
	}

	llvm::Value *AsOperatorOverloadAST::codegen(bool autoDeref, llvm::Value *other)
	{
		name += "operator_as_";
		llvm::raw_string_ostream stringstream(name);
		arg1[0].ty->codegen()->print(stringstream);
		name += '_';
		ty->codegen()->print(stringstream);
		PrototypeAST Proto(name, arg1, ret);
		llvm::Function *prevFunction = currentFunction;
		std::vector<llvm::Type *> argtypes;
		for (auto &x : Proto.Args)
			argtypes.push_back(x.ty == NULL ? NULL : x.ty->codegen());
		argtypes.push_back(ty->codegen());
		// Segment stolen from Proto.codegen()
		std::vector<std::string> Argnames;
		std::vector<llvm::Type *> Argt;
		for (auto &x : Proto.Args)
		{
			if (x.ty == NULL)
				continue;
			Argnames.push_back(x.name);
			Argt.push_back(x.ty->codegen());
		}
		llvm::FunctionType *FT =
			llvm::FunctionType::get(Proto.retType->codegen(), Argt, false);
		currentFunction =
			llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Proto.Name, GlobalVarsAndFunctions.get());
		unsigned Idx = 0;
		for (auto &Arg : currentFunction->args())
		{
			Arg.setName(Argnames[Idx++]);
		}
		// end of segment stolen from Proto.codegen()

		if (!currentFunction)
		{
			currentFunction = prevFunction;
			return NULL;
		}
		if (!currentFunction->empty())
		{
			logError("Operator Cannot be redefined: " + currentFunction->getName().str());
		}

		llvm::BasicBlock *BB = llvm::BasicBlock::Create(*ctxt, "entry", currentFunction);
		builder->SetInsertPoint(BB);
		// Record the function arguments in the Named Values map.
		for (auto &Arg : currentFunction->args())
		{
			llvm::Value *storedvar = builder->CreateAlloca(Arg.getType(), NULL, Arg.getName());
			builder->CreateStore(&Arg, storedvar);
			std::string name = std::string(Arg.getName());
			AliasMgr[name] = {storedvar, Proto.Args[Arg.getArgNo()].ty->isReference()};
			// dtypes[name] = Arg.getType();
		}
		llvm::Instruction *currentEntry = &BB->getIterator()->back();
		llvm::Value *RetVal = body == NULL ? NULL : body->codegen();

		if (RetVal != NULL && (!RetVal->getType()->isPointerTy() || !RetVal->getType()->getNonOpaquePointerElementType()->isFunctionTy()))
		{
			// Validate the generated code, checking for consistency.
			verifyFunction(*currentFunction);
			if (DEBUGGING)
				std::cout << "//end of " << Proto.getName() << std::endl;
			operators[argtypes[0]]["AS"][argtypes[1]] = FunctionHeader(this->arg1, currentFunction, this->ret->isReference());
			// remove the arguments now that they're out of scope
		}
		else
		{
			currentFunction->deleteBody();
		}
		for (auto &Arg : currentFunction->args())
		{
			AliasMgr[std::string(Arg.getName())] = {NULL, false};
			// dtypes[std::string(Arg.getName())] = NULL;
		}
		currentFunction = prevFunction;
		if (currentFunction != NULL)
			builder->SetInsertPoint(&currentFunction->getBasicBlockList().back());
		return operators[argtypes[0]]["AS"][argtypes[1]].func;
	}
	llvm::Value *AssertionExprAST::codegen(bool autoDeref, llvm::Value *other)
	{
		llvm::FunctionCallee abortfunc = GlobalVarsAndFunctions->getOrInsertFunction("abort",
																					 llvm::FunctionType::get(llvm::Type::getVoidTy(*ctxt), false));
		llvm::FunctionCallee printfunc = GlobalVarsAndFunctions->getOrInsertFunction("printf",
																					 llvm::FunctionType::get(llvm::IntegerType::getInt32Ty(*ctxt), llvm::PointerType::get(llvm::Type::getInt8Ty(*ctxt), false), true));
		llvm::Value *strval = msg == NULL ? builder->CreateGlobalStringPtr("<No Message Provided>") : msg->codegen();
		llvm::Value *boolval = condition->codegen();
		llvm::BasicBlock *assertblock = llvm::BasicBlock::Create(*ctxt, "assertionIfFalseBlock", currentFunction);
		llvm::BasicBlock *passblock = llvm::BasicBlock::Create(*ctxt, "assertionIfTrueBlock", currentFunction);
		builder->CreateCondBr(boolval, passblock, assertblock);
		builder->SetInsertPoint(assertblock);
		std::string assertstr = "Assertion failed in: %s:%d - \"%s\"\n";
		llvm::Value *assertstrptr = builder->CreateGlobalStringPtr(assertstr);
		llvm::Value *filenameptr = builder->CreateGlobalStringPtr(currentFile);
		builder->CreateCall(printfunc, {assertstrptr, filenameptr, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctxt), llvm::APInt(32, line)), strval}, "printlntmp");
		builder->CreateCall(abortfunc);
		if (currentFunction->getReturnType()->getTypeID() != llvm::Type::VoidTyID)
		{
			builder->CreateRet(llvm::ConstantAggregateZero::get(currentFunction->getReturnType()));
		}
		else
		{
			builder->CreateRetVoid();
		}
		builder->SetInsertPoint(passblock);
		return boolval;
	}

	llvm::Value *ASMExprAST::codegen(bool autoderef, llvm::Value *other)
	{
		llvm::InlineAsm *v = llvm::InlineAsm::get(llvm::FunctionType::get(llvm::Type::getVoidTy(*ctxt), false), assembly, "~{dirflag},~{fpsr},~{flags}", true, false, llvm::InlineAsm::AD_ATT);
		builder->CreateCall(v, {});
		return v;
	}
}