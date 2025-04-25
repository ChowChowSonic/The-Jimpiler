#include <spdlog/spdlog.h>
#include "ExprAST.h"
#include "TypeExpr.h"
#include "AliasManager.h"
#include "globals.cpp"
namespace jimpilier{
	class TemplateGenerator{
		class TemplateObject{
			public: 
			std::vector<Variable> members; 
			std::vector<std::unique_ptr<ExprAST>> functions;
			std::vector<std::unique_ptr<TypeExpr>> templates; 
			
			TemplateObject() : members(std::vector<Variable>()), functions(std::vector<std::unique_ptr<ExprAST>>()){}; 
			TemplateObject( 
			std::vector<Variable>& members, 
			std::vector<std::unique_ptr<ExprAST>>& functions, 
			std::vector<std::unique_ptr<TypeExpr>> &templateNames): members(std::move(members)), functions(std::move(functions)), templates(std::move(templateNames)){}
			
			bool isValid(){
				return functions.size() > 0 || members.size() > 0 || templates.size() > 0;  
			}
			private:
			bool contains(std::vector<llvm::Type*>& vec, llvm::Type* t){
				return std::find(vec.begin(), vec.end(), t) != vec.end(); 
			}
			int indexOf(llvm::Type* t, std::vector<llvm::Type*> vec){
				for(int i = 0; i < vec.size(); i++){
					if(vec[i] == t) return i; 
				}
				return -1; 
			}
		};
		std::map<std::string, std::map<std::vector<llvm::Type*>, llvm::Type*>> generatedObjects; 
		/**
		 * @brief a map correlating each templateObject to its name & number of template typenames (number of identifiers enclosed in gt/lt symbols, "<" & ">"). 
		 * Two templateObjects cannot have identical names & template counts. At least one identifier of the two must differ 
		 * 
		 * @example "vector<T>" 	-> templates["vector"][1]
		 * @example "map<X, Y>" 	-> templates["map"][2]
		 * @example "List<T>" 		-> templates["List"][1]
		 * @example "List<A,B>"		-> templates["List"][2]
		 * @example "List<T>" and "List<A,B>" are considered two completely separate objects, and therefore are allowed to coexist. 
		 * @example "map<x, Y>" and "map<Y, Z>" are, as far as we are concerned, the exact same object; therefore declaring the 2nd map would throw a redefinition error. 
		 */
		std::map<std::string, std::map<int, TemplateObject>> templates; 
		public: 
		TemplateGenerator(){
			std::vector<std::unique_ptr<TypeExpr>> templates;
			std::vector<Variable> members;
			std::vector<std::unique_ptr<ExprAST>> functions;
			templates.push_back(std::make_unique<StructTypeExpr>(".T")); 
			std::unique_ptr<TypeExpr> ty = std::make_unique<StructTypeExpr>(".T"); 
			ty = std::make_unique<PointerToTypeExpr>(ty); 
			members.push_back(Variable(std::string("data"), ty)); 
			ty = std::make_unique<LongTypeExpr>(); 
			members.push_back(Variable(std::string("size"), ty)); 
			ty = std::make_unique<LongTypeExpr>(); 
			members.push_back(Variable(std::string("capacity"), ty)); 
			this->insertTemplate(".array", templates, members, functions);
		}

		void insertTemplate(std::string name, std::vector<std::unique_ptr<TypeExpr>> &templateNames, std::vector<Variable> &objMembers, std::vector<std::unique_ptr<ExprAST>> &functions){
			spdlog::debug("TemplateGenerator Inserting template named: {}", name);
			int numTemplates = templateNames.size(); 
			assert(!templates[name][numTemplates].isValid() && "Template Redeclaration Error: A template Object with an identical name & number of templates already exists!"); 
			templates[name][numTemplates] = TemplateObject(objMembers, functions, templateNames); 
		}
		/**
		 * @brief Replace all typenames in a given template with the types provided in the `types` array. 
		 * Returns a `TemplateObject&` containing the modified template; It is up to the end user to codegen all the functions & load the object members into a `llvm::StructType` 
		 * @see TemplateObject::generateAST(std::string name)
		 * @param name - the name of the base template to retrieve
		 * @param types - the templates to replace
		 * @return TemplateObject& 
		 */
		TemplateObject &getTemplate(std::string & name, std::vector<std::unique_ptr<TypeExpr>> &types){
			spdlog::debug("TemplateGenerator retrieving template named: {}", name);
			int numTemplates = types.size(); 
			TemplateObject &obj = templates[name][numTemplates];
			return obj; 
		}
		bool hasTemplate(std::string x){
			return !templates[x].empty(); 
		}
	};

}