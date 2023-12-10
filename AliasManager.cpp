#include <map>
#include "llvm/IR/Function.h"

class AliasManager
{
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

public:
	std::map<std::string, std::vector<FunctionHeader>> functionAliases;
	AliasManager() {}
	llvm::Function *getFunction(std::string &name, std::vector<llvm::Type *> &args)
	{
		for (auto f : functionAliases[name])
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
	void debug()
	{
		for (map<std::string, std::vector<FunctionHeader>>::iterator itr = functionAliases.begin();
			 itr != functionAliases.end(); ++itr)
		{
			std::cout << itr->first << " " << itr->second.size() << " " << this->hasAlias(itr->first) << endl;
		}
	}
};
