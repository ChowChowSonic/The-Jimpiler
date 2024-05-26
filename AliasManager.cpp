
#include <map>
#include "llvm/IR/Function.h"

#ifndef aliasmgr
#define aliasmgr
#include "globals.cpp"
namespace jimpilier {
class FunctionHeader{
	public:
		std::vector<llvm::Type *> args;
		llvm::Function *func;
		FunctionHeader(std::vector<llvm::Type *> arglist, llvm::Function *func)
			: args(arglist), func(func) {}

		bool operator==(FunctionHeader &other)
		{
			return this->args == other.args;
		}
		bool operator==(std::vector<llvm::Type *> &other)
		{
			
			return this->args == other; 
		}
	};

class FunctionAliasManager{
	
	std::map<std::string, std::vector<FunctionHeader>> functionAliases;
	public:
	FunctionAliasManager(){}
	llvm::Function *getFunction(std::string &name, std::vector<llvm::Type *> &args)
	{
		for (auto& f : functionAliases[name])
		{
			if (f == args)
				return f.func;
			// std::cout << f.args.size() << " " << args.size() << " " << (f.args[0] == args[0]) <<endl;
		}
		return NULL;
	}
	void addFunction(std::string name, llvm::Function *func, std::vector<llvm::Type *> args)
	{
		functionAliases[name].push_back(FunctionHeader(args, func));
	}
	bool hasAlias(std::string& alias){
		return !functionAliases[alias].empty(); 
	}
}; 

class ObjectAliasManager {
	class Object {	
		class ObjectMember{
			public:
			std::string name;
			llvm::Type* type; 
			int index; //Do away with this variable eventually
			public:
			ObjectMember(const std::string& name, llvm::Type* type, int index) : name(name), type(type), index(index) {}
		};
		public: 
		FunctionAliasManager functions; 
		std::vector<ObjectMember> members;
		llvm::Type* ptr; 
		llvm::Function* destructor;
		Object(){
			ptr = NULL;
			destructor = NULL; 
		}
		Object(llvm::Type* ty) : ptr(ty), destructor(NULL){}
		Object(llvm::Type* ty, std::vector<llvm::Type*> members, std::vector<std::string> names, llvm::Function* destruct) : ptr(ty), destructor(destruct){
			for(int i =0; i < members.size(); i++){
				this->members.push_back(ObjectMember(names[i], members[i], i));
			}
		}
		ObjectMember getMember(int i){ 
			return members[i]; 
		}
		ObjectMember getMember(std::string name){
			for(auto& i : members){
				if(i.name == name){
					return i; 
				}
			}
			return {"", NULL, -1};
		}
		bool operator==(llvm::Type* other){
			return other == ptr; 
		}
	}; 
	std::map<std::string, Object> structTypes;
	std::map<llvm::Type*, std::vector<FunctionHeader>> constructors; //Rework this maybe? 
	public: 
		llvm::Function* getConstructor(llvm::Type* ty, std::vector<llvm::Type*>& args){
		for (auto& f : constructors[ty])
		{
			if (f == args)
				return f.func;
			// std::cout << f.args.size() << " " << args.size() << " " << (f.args[0] == args[0]) <<endl;
		}
		return NULL;
	}

	void addConstructor(llvm::Type* type, llvm::Function *func, std::vector<llvm::Type *> args)
	{
		constructors[type].push_back(FunctionHeader(args, func));
	}
	Object getObject(llvm::Type* ty){
		for(auto& x : structTypes){
			if(x.second == ty) return x.second; 
		}
		return Object(); 
	}	
	std::string getObjectName(llvm::Type* ty){
		for(auto& x : structTypes){
			if(x.second == ty) return x.first; 
		}
		return "<Unknown Object>"; 
	}	
	Object& getObject(std::string alias){
		return structTypes[alias]; 
	}

	bool addObject(std::string alias, llvm::Type* objType, std::vector<llvm::Type*> memberTypes, std::vector<std::string> memberNames, llvm::Function* destructor = NULL){
		if(structTypes[alias].ptr != NULL){ 

			return false; 
		}
		structTypes[alias] = Object(objType, memberTypes, memberNames, destructor); 
		return true; 
	}

	void addObjectMembers(std::string alias, std::vector<llvm::Type*> memberTypes, std::vector<std::string> memberNames){
		for(int i = 0; i < memberTypes.size();i++)
			structTypes[alias].members.push_back({memberNames[i], memberTypes[i], i}); 
	}

	bool addObject(std::string alias, llvm::Type* objType){
		if(structTypes[alias].ptr != NULL){ 
			return false; 
		}
		structTypes[alias] = Object(objType); 
		return true;
	}

	void addObjectFunction(std::string& objName, std::string& funcAlias, std::vector<llvm::Type*> types, llvm::Function* func){
		structTypes[objName].functions.addFunction(funcAlias, func, types); 
	}
	void setObjectDestructor(std::string alias, llvm::Function* des){
		structTypes[alias].destructor = des; 
	}
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
	class CompileTimeVariable{
		public:
		llvm::Value* val = NULL; 
		bool isRef = false; 
	}; 

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
	CompileTimeVariable& operator[](const std::string& alias){
		return variables[alias]; 
	}
	/**
	 * @brief Returns a llvm::Type* that is associated with a particular name. Intended for Read-Only operations
	 * @param alias 
	 * @return a llvm::Value*& that refers to a named variable.  
	 */
	llvm::Type* operator()(const std::string& alias){
		return objects.getObject(alias).ptr; 
	}
	/**
	 * @brief Returns a struct that represents an object's member with a particular name. Intended for Read-Only operations
	 * @param alias 
	 * @return a llvm::Value*& that refers to a named variable.  
	 */
	auto operator()(const std::string& alias, const std::string& member){
		return objects.getObject(alias).getMember(member); 
	}
	/**
	 * @brief Returns a struct that represents an object's member with a particular name. Intended for Read-Only operations
	 * @param alias 
	 * @return a llvm::Value*& that refers to a named variable.  
	 */
	auto operator()(llvm::Type* alias, const std::string& member){
		return objects.getObject(alias).getMember(member); 
	}
	/**
	 * @brief Returns a llvm::Function* that represents an object constructor with a particular set of args. Intended for Read-Only operations
	 * @param alias 
	 * @return a llvm::Value*& that refers to a named variable.  
	 */
	auto operator()(llvm::Type* constructor, std::vector<llvm::Type*>& args){
		return objects.getConstructor(constructor, args); 
	}
	/**
	 * @brief Returns a struct that represents an object's Nth member. Intended for Read-Only operations
	 * @param alias 
	 * @return a llvm::Value*& that refers to a named variable.  
	 */
	auto operator()(const std::string& alias, int member){
		return objects.getObject(alias).getMember(member); 
	}

	llvm::Value *getTypeSize(llvm::Type *ty, std::unique_ptr<llvm::LLVMContext> &ctxt, std::unique_ptr<llvm::DataLayout> &DataLayout){
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

	std::string getTypeName(llvm::Type *ty){
			if(ty == NULL) return "null"; 
			std::string ret = ""; 
			switch (ty->getTypeID())
			{
			case llvm::Type::IntegerTyID:
				return "int"+std::to_string(ty->getIntegerBitWidth()); 
			case llvm::Type::FloatTyID:
				return "float";
			case llvm::Type::ArrayTyID:
			case llvm::Type::PointerTyID:
				{
					llvm::Type* type2 = ty; 
					do{
						ret += type2->isPointerTy() ? "pointer to a " : "array of "; 
						type2 = type2->isPointerTy() ? type2->getNonOpaquePointerElementType() : type2->getContainedType(0);
					}while(type2->isPointerTy() || type2->isArrayTy()); 
					return ret+getTypeName(type2);
				}
			case llvm::Type::DoubleTyID:
				return "double"; 
			case llvm::Type::StructTyID:
				ret+= "structure named "; 
				ret+= this->objects.getObjectName(ty); 
				return ret; 
			case llvm::Type::FunctionTyID:{
			llvm::FunctionType* ft = (llvm::FunctionType*) ty; 
				ret+= "function that returns a(n) ";
				ret+= this->getTypeName(ft->getReturnType());
				ret+= " with exactly ";
				ret+= std::to_string((int)ft->getNumParams()); 
				ret+= " fixed arguments";
				ret+= ft->isVarArg() ? ", and has at least 1 set of varadic arguments" : "."; 
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
}
#endif