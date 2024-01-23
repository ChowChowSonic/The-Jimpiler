#include <map>
#include "llvm/IR/Function.h"

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
		Object(){
			ptr = NULL; 
		}
		Object(llvm::Type* ty, std::vector<llvm::Type*> members, std::vector<std::string> names) : ptr(ty) {
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
	Object& getObject(std::string alias){
		return structTypes[alias]; 
	}

	bool addObject(std::string alias, llvm::Type* objType, std::vector<llvm::Type*> memberTypes, std::vector<std::string> memberNames){
		if(structTypes[alias].ptr != NULL){ 
			return false; 
		}
		structTypes[alias] = Object(objType, memberTypes, memberNames); 
		return true; 
	}
	void addObjectFunction(std::string& objName, std::string& funcAlias, std::vector<llvm::Type*> types, llvm::Function* func){
		structTypes[objName].functions.addFunction(funcAlias, func, types); 
	}
}; 

class AliasManager
{
public:
	FunctionAliasManager functions; 
	ObjectAliasManager objects; 
	std::map<std::string, llvm::Value *> variables;
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
	llvm::Value*& operator[](const std::string& alias){
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
	 * @brief Returns a struct that represents an object's Nth member. Intended for Read-Only operations
	 * @param alias 
	 * @return a llvm::Value*& that refers to a named variable.  
	 */
	auto operator()(const std::string& alias, int member){
		return objects.getObject(alias).getMember(member); 
	}
	
};
