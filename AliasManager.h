#pragma once
#include <map>
#include "llvm/IR/Function.h"
#ifndef aliasmgr
#define aliasmgr
#include "globals.cpp"
#include "TypeExpr.h"
namespace jimpilier
{
	class CompileTimeType
		{
		public:
			llvm::Type *ty = NULL;
			bool isRef = false;
			CompileTimeType(llvm::Type* t, bool ref) : ty(t), isRef(ref) {}; 
			bool operator==(CompileTimeType &t);
		};
	class FunctionHeader
	{
		

	public:
		std::vector<CompileTimeType> args;
		llvm::Function *func;
		FunctionHeader(std::vector<Variable> &arglist, llvm::Function *func);
		bool operator==(FunctionHeader &other);
		bool operator==(std::vector<llvm::Type *> &other);
	};

	class FunctionAliasManager
	{

		std::map<std::string, std::vector<FunctionHeader>> functionAliases;

	public:
		FunctionAliasManager() {}
		llvm::Function *getFunction(std::string &name, std::vector<llvm::Type *> &args);
		void addFunction(std::string name, llvm::Function *func, std::vector<jimpilier::Variable> &args);
		bool hasAlias(std::string &alias);
	};

		class ObjectMember
	{
	public:
		std::string name;
		llvm::Type *type;
		int index; // Do away with this variable eventually
		ObjectMember(const std::string &name, llvm::Type *type, int index);
	};
	class Object
	{
	public:
		FunctionAliasManager functions;
		std::vector<ObjectMember> members;
		llvm::Type *ptr;
		Object();
		Object(llvm::Type *ty);
		Object(llvm::Type *ty, std::vector<llvm::Type *> members, std::vector<std::string> names);
		ObjectMember getMember(int i);
		ObjectMember getMember(std::string name);
		bool operator==(llvm::Type *other);
	};

	class ObjectAliasManager
	{
		std::map<std::string, Object> structTypes;
		std::map<llvm::Type *, std::vector<FunctionHeader>> constructors; // Rework this maybe?
	public:
		llvm::Function *getConstructor(llvm::Type *ty, std::vector<llvm::Type *> &args);
		void addConstructor(llvm::Type *type, llvm::Function *func, std::vector<jimpilier::Variable> &args);
		Object getObject(llvm::Type *ty);
		std::string getObjectName(llvm::Type *ty);
		Object &getObject(std::string alias);
		bool addObject(std::string alias, llvm::Type *objType, std::vector<llvm::Type *> memberTypes, std::vector<std::string> memberNames);
		void addObjectMembers(std::string alias, std::vector<llvm::Type *> memberTypes, std::vector<std::string> memberNames);
		bool addObject(std::string alias, llvm::Type *objType);
		void addObjectFunction(std::string &objName, std::string &funcAlias, std::vector<Variable> &types, llvm::Function *func);
	};
		class CompileTimeVariable
		{
		public:
			llvm::Value *val;
			bool isRef;
		};
	/**
	 * @brief An all-in-one wrapper class that manages the frontend names/aliases of functions, variables and objects.
	 * Variables can be read & modified via the indexing operator (```AliasManager["name"]```).
	 * Meanwhile objects and functions can be read through the function call operator (```AliasManager("name")```)
	 *
	 *
	 */
	class AliasManager
	{
	public:
		FunctionAliasManager functions;
		ObjectAliasManager objects;
		std::map<std::string, CompileTimeVariable> variables;
		AliasManager() {}
		/**
		 * @brief Returns a refrence to a llvm::Value* that represents a named variable. The llvm::Value itself will
		 * be a pointer to that variable in memory.
		 * To access functions, refer to AliasManager.functions or AliasManager.operator()(std::string&, std::vector<llvm::Type>&).
		 * To access Types by name, refer to AliasManager.objects or AliasManager.operator()(std::string&).
		 * @example int i = 9;
		 * AliasManager["i"] will return an int* that points to i.
		 * @param alias
		 * @return a llvm::Value*& that refers to a named variable.
		 */
		CompileTimeVariable &operator[](const std::string &alias)
		{
			return variables[alias];
		}
		/**
		 * @brief Returns a llvm::Type* that is associated with a particular name. Intended for Read-Only operations
		 * @param alias
		 * @return a llvm::Value*& that refers to a named variable.
		 */
		llvm::Type *operator()(const std::string &alias)
		{
			return objects.getObject(alias).ptr;
		}
		/**
		 * @brief Returns a struct that represents an object's member with a particular name. Intended for Read-Only operations
		 * @param alias
		 * @return a llvm::Value*& that refers to a named variable.
		 */
		ObjectMember operator()(const std::string &alias, const std::string &member)
		{
			return objects.getObject(alias).getMember(member);
		}
		/**
		 * @brief Returns a struct that represents an object's member with a particular name. Intended for Read-Only operations
		 * @param alias
		 * @return a llvm::Value*& that refers to a named variable.
		 */
		ObjectMember operator()(llvm::Type *alias, const std::string &member)
		{
			return objects.getObject(alias).getMember(member);
		}
		/**
		 * @brief Returns a llvm::Function* that represents an object constructor with a particular set of args. Intended for Read-Only operations
		 * @param alias
		 * @return a llvm::Value*& that refers to a named variable.
		 */
		llvm::Function* operator()(llvm::Type *constructor, std::vector<llvm::Type *> &args)
		{
			return objects.getConstructor(constructor, args);
		}
		/**
		 * @brief Returns a struct that represents an object's Nth member. Intended for Read-Only operations
		 * @param alias
		 * @return a llvm::Value*& that refers to a named variable.
		 */
		ObjectMember operator()(const std::string &alias, int member)
		{
			return objects.getObject(alias).getMember(member);
		}
		llvm::Value *getTypeSize(llvm::Type *ty, std::unique_ptr<llvm::LLVMContext> &ctxt, std::unique_ptr<llvm::DataLayout> &DataLayout);
		std::string getTypeName(llvm::Type *ty);
	};
}
#endif