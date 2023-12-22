#include <map>
#include "llvm/IR/Function.h"

class AliasManager
{
	std::pair<int, llvm::Type*> errorval = std::pair<int, llvm::Type*>(-1, NULL); 
	class FunctionHeader
	{
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

	class Object {
		public: 
		std::map<std::string, std::vector<FunctionHeader>> functions;
		std::map<std::string, std::pair<int, llvm::Type *>> members;
		llvm::Type* ptr; 
		Object(){
			ptr = NULL; 
		}
		Object(llvm::Type* ty, std::vector<llvm::Type*> members, std::vector<std::string> names) : ptr(ty) {
			for(int i =0; i < members.size(); i++){
				this->members[names[i]] = std::pair<int, llvm::Type*>(i, members[i]);
			}
		}

		std::pair<int, llvm::Type*>& getMember(std::string name){
			return members[name];
		}
	}; 

public:
	std::map<std::string, std::vector<FunctionHeader>> functionAliases;
		std::map<std::string, Object> structTypes;
	AliasManager() {}
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
	bool hasAlias(std::string alias)
	{
		return !functionAliases[alias].empty();
	}
	bool addObject(std::string& name, llvm::Type* ty, std::vector<llvm::Type*> types, std::vector<std::string> names){
		//if(structTypes[name].ptr != NULL) return false; 
		structTypes.emplace(name, Object(ty, types, names));
		return true; 
	}
	void addObjectFunction(std::string& objName, std::string& funcAlias, std::vector<llvm::Type*> types, llvm::Function* func){
		structTypes[objName].functions[funcAlias].push_back(FunctionHeader(types, func)); 
	}
	/**
	 * Retrieves an object's member as a pair containing the index of the member and its type. 
	 * @example
	 * obj str{
	 * <TYPE> <NAME>; //this is at index 0 
	 * int* obj; //this is at index 1 
	 * } 
	 * getObjMember("str", "obj") //returns <1, int*>
	*/
	std::pair<int, llvm::Type*>& getObjMember(llvm::Type* ty, std::string name){
		for(auto& x : structTypes){
			if(x.second.ptr == ty) return x.second.getMember(name); 
		}
		
		return errorval; 
	}
	/**
	 * Retrieves an object's member as a pair containing the index of the member and its type. 
	 * @example
	 * obj str{
	 * <TYPE> <NAME>; //this is at index 0 
	 * int* obj; //this is at index 1 
	 * } 
	 * getObjMember("str", "obj") //returns <1, int*>
	*/
	std::pair<int, llvm::Type*>& getObjMember(std::string objName, std::string name){
		return structTypes[objName].getMember(name); 
	}
};
