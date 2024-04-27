#include <iomanip>
#include <stack>
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
#ifndef globals
#define globals
#include "AliasManager.cpp"
namespace jimpilier{
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
	/**
	 * @brief function where any code put into a static block is placed. Eventually all code in this will be drained into the beginning of main() if it exists
	 * 
	 */
	llvm::Function *STATIC = nullptr;
	/**
	 * @brief In the event that a Break statement is called (I.E. in a for loop or switch statements),
	 * the compilier will automatically jump to the top llvm::BasicBlock in this stack
	 *
	 */
	std::stack<std::pair<llvm::BasicBlock *, llvm::BasicBlock *>> escapeBlock;
	std::map<llvm::Type *, std::map<std::string, std::map<llvm::Type *, llvm::Function *>>> operators;
	/* Strictly for testing purposes, not meant for releases*/
	const bool DEBUGGING = false;
	bool errored = false;
}
#endif