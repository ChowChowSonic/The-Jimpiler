#include <iomanip>
#include <stack>
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#ifndef globals
#define globals
#include "tokenizer.cpp"
#include "AliasManager.h"
#include "TemplateGenerator.cpp"
namespace jimpilier
{
	class AliasManager;
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
	TemplateGenerator TemplateMgr;
	std::vector<std::string> importedFiles;
	llvm::Function *currentFunction;
	llvm::BasicBlock *currentUnwindBlock = NULL;
	/**
	 * @brief function where any code put into a static block is placed. Eventually all code in this will be drained into the beginning of main() if it exists
	 *
	 */
	llvm::Function *STATIC = nullptr;
	/**
	 * @brief In the event that a Break statement is called (I.E. in a for loop or switch statements),
	 * the compilier will automatically jump to the top llvm::BasicBlock in this stack.
	 * The first block in the pair is for `continue` keywords, the second is for `break` keywords.
	 *
	 */
	std::stack<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>> escapeBlock;
	std::map<llvm::Type *, std::map<std::string, std::map<llvm::Type *, FunctionHeader>>> operators;
	std::map<llvm::Type *, llvm::Value *> classInfoVals;
	std::string currentFile;
	/* Strictly for testing purposes, not meant for releases*/
	const bool DEBUGGING = false;

	// <-- BEGINNING OF UTILITY FUNCTIONS -->

	void logError(const std::string &s, Token t)
	{
		std::cout << s << ' ' << t.toString() << std::endl;
		assert(false);
	}
	void logError(const std::string &s)
	{
		std::cout << s << ' ' << std::endl;
		assert(false);
	}
	/**
	 * @brief Makes a call to a function (defined in `CalleeF`) using a std::vector<> containing pointers to each of the args.
	 * Dereferences each arg as necessary to ensure that non-references are passed correctly, while references may be passed as 
	 * pointers to the object or as the object itself (in other words, if we have `int* x`, we don't have to say `@x` when passing x to a `int@ arg`)
	 * 
	 * @param ptrsToArgs - The arguments passed to this object, ideally via their `codegen(false)` method
	 * @param CalleeF - The function header to call
	 * @return llvm::Value* - The result of the call
	 */
	llvm::Value *makeCallWithReferences(std::vector<llvm::Value *> &ptrsToArgs, FunctionHeader &CalleeF, bool hasParent = false)
	{
		for (unsigned i = hasParent; i < ptrsToArgs.size(); ++i)
		{
			if (!CalleeF.args[i-hasParent].isRef && ptrsToArgs[i]->getType() == CalleeF.args[i-hasParent].ty->getPointerTo())
				ptrsToArgs[i] = builder->CreateLoad(ptrsToArgs[i]->getType()->getNonOpaquePointerElementType(), ptrsToArgs[i], "dereftmp");
			if (!ptrsToArgs[i])
			{
				assert(false && "Error saving function args");
			}
		}
		if (!CalleeF.canThrow())
			return CalleeF.func->getReturnType() == llvm::Type::getVoidTy(*ctxt) ? builder->CreateCall(CalleeF.func, ptrsToArgs) : builder->CreateCall(CalleeF.func, ptrsToArgs, "calltmp");
		assert(currentUnwindBlock != NULL && "Attempted to call a function that throws errors with no way to catch the error!");
		llvm::BasicBlock *normalUnwindBlock = llvm::BasicBlock::Create(*ctxt, "NormalExecBlock", currentFunction);
		llvm::Value *retval = CalleeF.func->getReturnType() == llvm::Type::getVoidTy(*ctxt) ? builder->CreateInvoke(CalleeF.func, normalUnwindBlock, currentUnwindBlock, ptrsToArgs) : builder->CreateInvoke(CalleeF.func, normalUnwindBlock, currentUnwindBlock, ptrsToArgs, "calltmp");
		builder->SetInsertPoint(normalUnwindBlock);
		return retval;
	}

	/**
	 * @brief Get the Operator called between two types. Attempts to retrieve operators with references if one with raw object types are not found
	 * 
	 * @param arg1 - the left-hand ("first") argument type passed to the operator 
	 * @param opStr - The string representing the operator being called
	 * @param arg2 - the right-hand ("second") argument type passed to the operator
	 * @return FunctionHeader& - The operator to be called
	 */
	FunctionHeader getOperatorFromTypes(llvm::Type *arg1, const std::string& opStr, llvm::Type* arg2){
		FunctionHeader ret = operators[arg1][opStr][arg2];
		if(ret.func == NULL) ret = operators[arg1 == NULL ? NULL : arg1->getPointerTo()][opStr][arg2]; 
		if(ret.func == NULL) ret = operators[arg1][opStr][arg2 == NULL ? NULL : arg2->getPointerTo()]; 
		if(ret.func == NULL) ret = operators[arg1 == NULL? NULL : arg1->getPointerTo()][opStr][arg2 == NULL ? NULL : arg2->getPointerTo()]; 
		return ret; 
	}
	/**
	 * @brief Get the Operator called between two values. Attempts to retrieve operators with references if one with raw object types are not found
	 * 
	 * @param arg1 - the left-hand ("first") argument passed to the operator 
	 * @param opStr - The string representing the operator being called
	 * @param arg2 - the right-hand ("second") argument passed to the operator
	 * @return FunctionHeader& - the operator to be called
	 */
	FunctionHeader getOperatorFromVals(llvm::Value *arg1, const std::string& opStr, llvm::Value* arg2){
		llvm::Type *t1 = arg1 == NULL ? (llvm::Type*)NULL : (llvm::Type*)arg1->getType(), *t2 = arg2 == NULL ? (llvm::Type*) NULL : (llvm::Type*)arg2->getType(); 
		return getOperatorFromTypes(t1, opStr, t2); 
	}
}

#endif
