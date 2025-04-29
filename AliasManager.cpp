#pragma once
#include <spdlog/spdlog.h>
#include <map>
#include "llvm/IR/Function.h"
#include "globals.cpp"
#include "TypeExpr.h"
#include "AliasManager.h"

namespace jimpilier
{
	bool operator==(CompileTimeType og, CompileTimeType t)
	{
		return og.ty == t.ty || (og.ty->isPointerTy() && og.ty->getNonOpaquePointerElementType() == t.ty);
	}
	bool operator==(CompileTimeType og, llvm::Type *t)
	{
		return og.ty == t || (og.ty->isPointerTy() && og.ty->getNonOpaquePointerElementType() == t);
	}
	bool operator!=(CompileTimeType og, llvm::Type *t)
	{
		return ((og.ty->isPointerTy() &&
				 og.ty->getNonOpaquePointerElementType() != t) &&
				og.ty != t);
	}
	// class FunctionHeader
	FunctionHeader::FunctionHeader(std::vector<Variable> &arglist, llvm::Function *func, bool returnsRefrence)
		: func(func), returnsRefrence(returnsRefrence)
	{
		for (auto &x : arglist)
			args.push_back(CompileTimeType(x.ty->codegen(), x.ty->isReference()));
	}

	FunctionHeader::FunctionHeader(std::vector<Variable> &arglist, std::vector<llvm::Type *> &throwables, llvm::Function *func, bool returnsRefrence)
		: func(func), returnsRefrence(returnsRefrence), throwableTypes(throwables)
	{
		for (auto &x : arglist)
			args.push_back(CompileTimeType(x.ty->codegen(), x.ty->isReference()));
	}
	bool operator==(FunctionHeader &h, std::vector<llvm::Type *> &other)
	{
		if (h.args.size() != other.size())
			return false;
		for (int i = 0; i < h.args.size(); i++)
		{
			bool b = h.args[i] != other[i];
			if (b)
			{
				return false;
			}
		}
		return true;
	}

	bool operator==(FunctionHeader &og, FunctionHeader &other)
	{
		return og.args == other.args;
	}
	// ends FunctioHeader functions
	// class FunctionAliasManager {
	FunctionHeader &FunctionAliasManager::getFunction(llvm::Function *f)
	{
		for (auto &f2 : functionAliases)
		{
			for (auto &fheader : f2.second)
			{
				if (fheader.func == f)
					return fheader;
			}
		}
		assert(false && "Function not found");
	};

	llvm::Function *FunctionAliasManager::getFunction(std::string &name, std::vector<llvm::Type *> &args)
	{
		for (auto &f : functionAliases[name])
		{
			// std::vector<llvm::Type*> tys;
			// for(auto& x : f.args) tys.push_back(x.ty);
			if (f == args)
				return f.func;
			spdlog::debug("Searching for function {0} {1}(argc={2})", f.toString(), name, args.size());
		}
		return NULL;
	}
	FunctionHeader &FunctionAliasManager::getFunctionObject(std::string &name, std::vector<llvm::Type *> &args)
	{
		for (auto &f : functionAliases[name])
		{
			// std::vector<llvm::Type*> tys;
			// for(auto& x : f.args) tys.push_back(x.ty);
			if (f == args)
				return f;
			spdlog::debug("Searching for function {0} {1}(argc={2})", f.toString(), name, args.size());
		}
		std::string msg = "Unknown object function referenced, or incorrect arg types were passed: " + name + "(";
		int ctr = 0;
		for (auto &x : args)
		{
			msg += AliasMgr.getTypeName(x);
			if (ctr < args.size() - 1)
				msg += ", ";
		}
		msg += ')';
		spdlog::error(msg);
		assert(false);
	}
	void FunctionAliasManager::addFunction(std::string name, llvm::Function *func, std::vector<jimpilier::Variable> &args, bool returnsRef)
	{
		functionAliases[name].push_back(FunctionHeader(args, func, returnsRef));
	}
	void FunctionAliasManager::addFunction(std::string name, llvm::Function *func, std::vector<jimpilier::Variable> &args, std::vector<llvm::Type *> &throwables, bool returnsRef)
	{
		functionAliases[name].push_back(FunctionHeader(args, throwables, func, returnsRef));
	}
	bool FunctionAliasManager::hasAlias(std::string &alias)
	{
		return !functionAliases[alias].empty();
	}
	// ends FunctionAliasManager functions
	//  class ObjectMember {
	ObjectMember::ObjectMember(const std::string &name, llvm::Type *type, int index) : name(name), type(type), index(index) {}
	// ends ObjectMember functions
	// class Object {
	Object::Object()
	{
		ptr = NULL;
	}
	Object::Object(llvm::Type *ty) : ptr(ty) {}
	Object::Object(llvm::Type *ty, std::vector<llvm::Type *> members, std::vector<std::string> names) : ptr(ty)
	{
		for (int i = 0; i < members.size(); i++)
		{
			this->members.push_back(ObjectMember(names[i], members[i], i));
		}
	}
	ObjectMember Object::getMember(int i)
	{
		return members[i];
	}
	ObjectMember Object::getMember(std::string name)
	{
		for (auto &i : members)
		{
			if (i.name == name)
			{
				return i;
			}
		}
		return {"", NULL, -1};
	}
	bool operator==(Object obj, llvm::Type *other)
	{
		return other == obj.ptr;
	}
	// ends Object Functions
	// class ObjectAliasManager {
	llvm::Function *ObjectAliasManager::getConstructor(llvm::Type *ty, std::vector<llvm::Type *> &args)
	{
		for (auto &f : constructors[ty])
		{
			if (f == args)
				return f.func;
			spdlog::debug("Searching for function {0} {1}(argc={2})", f.toString(), name, args.size());
		}
		return NULL;
	}

	void ObjectAliasManager::addConstructor(llvm::Type *type, llvm::Function *func, std::vector<jimpilier::Variable> &args)
	{
		constructors[type].push_back(FunctionHeader(args, func));
	}
	Object ObjectAliasManager::getObject(llvm::Type *ty)
	{
		for (auto &x : structTypes)
		{
			if (x.second.ptr == ty)
				return x.second;
		}
		return Object();
	}
	std::string ObjectAliasManager::getObjectName(llvm::Type *ty)
	{
		for (auto &x : structTypes)
		{
			if (x.second.ptr == ty)
				return x.first;
		}
		return "<Unknown Object>";
	}
	Object &ObjectAliasManager::getObject(std::string alias)
	{
		return structTypes[alias];
	}

	bool ObjectAliasManager::addObject(std::string alias, llvm::Type *objType, std::vector<llvm::Type *> memberTypes, std::vector<std::string> memberNames)
	{
		if (structTypes[alias].ptr != NULL)
		{

			return false;
		}
		structTypes[alias] = Object(objType, memberTypes, memberNames);
		return true;
	}

	void ObjectAliasManager::addObjectMembers(std::string alias, std::vector<llvm::Type *> memberTypes, std::vector<std::string> memberNames)
	{
		for (int i = 0; i < memberTypes.size(); i++)
			structTypes[alias].members.push_back({memberNames[i], memberTypes[i], i});
	}

	bool ObjectAliasManager::addObject(std::string alias, llvm::Type *objType)
	{
		if (structTypes[alias].ptr != NULL)
		{
			return false;
		}
		structTypes[alias] = Object(objType);
		return true;
	}

	void ObjectAliasManager::replaceObject(std::string alias, llvm::Type *objType)
	{
		structTypes[alias] = Object(objType);
	}

	void ObjectAliasManager::addObjectFunction(std::string &objName, std::string &funcAlias, std::vector<Variable> &types, llvm::Function *func, bool returnsRef)
	{
		structTypes[objName].functions.addFunction(funcAlias, func, types);
	}

	void ObjectAliasManager::removeObject(std::string name)
	{
		structTypes[name].ptr = NULL;
		structTypes[name].members.clear();
		structTypes[name].functions.clear();
	}
	// ends ObjectAliasManager functions

	/**
	 * @brief An all-in-one wrapper class that manages the frontend names/aliases of functions, variables and objects.
	 * Variables can be read & modified via the indexing operator (```AliasManager["name"]```).
	 * Meanwhile objects and functions can be read through the function call operator (```AliasManager("name")```)
	 *
	 *
	 */
	// class AliasManager{
	llvm::Value *AliasManager::getTypeSize(llvm::Type *ty, std::unique_ptr<llvm::LLVMContext> &ctxt, std::unique_ptr<llvm::DataLayout> &DataLayout)
	{
		llvm::Value *size = NULL;
		switch (ty->getTypeID())
		{
		case llvm::Type::IntegerTyID:
			size = llvm::ConstantInt::getIntegerValue(llvm::Type::getInt64Ty(*ctxt), llvm::APInt(64, ty->getIntegerBitWidth() / 8));
			break;
		case llvm::Type::FloatTyID:
			size = llvm::ConstantInt::getIntegerValue(llvm::Type::getInt64Ty(*ctxt), llvm::APInt(64, 4));
			break;
		case llvm::Type::PointerTyID:
		case llvm::Type::DoubleTyID:
			size = llvm::ConstantInt::getIntegerValue(llvm::Type::getInt64Ty(*ctxt), llvm::APInt(64, 8));
			break;
		case llvm::Type::ArrayTyID:
			size = llvm::ConstantInt::getIntegerValue(llvm::Type::getInt64Ty(*ctxt), llvm::APInt(64, DataLayout->getTypeAllocSize(ty) * ty->getArrayNumElements()));
			break;
		case llvm::Type::StructTyID:
			llvm::StructType *castedval = (llvm::StructType *)ty;
			size = llvm::ConstantInt::getIntegerValue(llvm::Type::getInt64Ty(*ctxt), llvm::APInt(64, DataLayout->getStructLayout(castedval)->getSizeInBytes()));
			break;
		}
		return size;
	}

	std::string AliasManager::getTypeName(llvm::Type *ty, bool prettyname)
	{
		if (ty == NULL)
			return "null";
		std::string ret = "";
		switch (ty->getTypeID())
		{
		case llvm::Type::IntegerTyID:
			return "int" + std::to_string(ty->getIntegerBitWidth());
		case llvm::Type::FloatTyID:
			return "float";
		case llvm::Type::ArrayTyID:
		case llvm::Type::PointerTyID:
		{
			llvm::Type *type2 = ty;
			do
			{
				bool isptr = type2->isPointerTy();
				ret += isptr ? (prettyname ? "pointer to a " : "->") : (prettyname ? "array of " : "[");
				type2 = isptr ? type2->getNonOpaquePointerElementType() : type2->getContainedType(0);
				if (!prettyname && !isptr)
					ret += ']';
			} while (type2->isPointerTy() || type2->isArrayTy());
			return ret + getTypeName(type2, prettyname);
		}
		case llvm::Type::DoubleTyID:
			return "double";
		case llvm::Type::StructTyID:
			ret += prettyname ? "structure named " : "";
			ret += this->objects.getObjectName(ty);
			return ret;
		case llvm::Type::FunctionTyID:
		{
			llvm::FunctionType *ft = (llvm::FunctionType *)ty;
			ret += prettyname ? "function that returns a(n) " : "";
			ret += this->getTypeName(ft->getReturnType());
			ret += prettyname ? " with exactly " : " ";
			if (prettyname)
				return ret;
			ret += std::to_string((int)ft->getNumParams());
			ret += " fixed arguments";
			ret += ft->isVarArg() ? ", and has at least 1 set of varadic arguments" : ".";
			return ret;
		}
		case llvm::Type::VoidTyID:
			return "void";
		default:
			return "<unknown type>";
		}
		return ret;
	}
};
// ends AliasManager functions