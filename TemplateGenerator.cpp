#include "ExprAST.h"
#include "TypeExpr.h"
#include "globals.cpp"
namespace jimpilier{
	class TemplateGenerator{
		class TemplateObject{
			public: 
			std::vector<Variable> members; 
			std::vector<std::unique_ptr<FunctionAST>> functions;
			std::vector<std::string> templateNames; 
			// TemplateObject() : members(NULL), functions(NULL){}; 
			TemplateObject( 
			std::vector<Variable>& members, 
			std::vector<std::unique_ptr<FunctionAST>>& functions, 
			std::vector<std::string> &templateNames): members(std::move(members)), functions(std::move(functions)), templateNames(templateNames){}
			void replaceTemplate(std::string& name, std::unique_ptr<TypeExpr> &ty){
				for(auto &x : members){
					x.ty->replaceTemplate(name, ty); 
				}
				for(auto& x : functions){
					x->replaceTemplate(name, ty); 
				}
			}
			bool isValid(){
				return functions.size() > 0 || members.size() > 0 || templateNames.size() > 0;  
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
		TemplateGenerator(){}

		void insertTemplate(std::string name, std::vector<std::string> &templateNames, std::vector<Variable> &objMembers, std::vector<std::unique_ptr<FunctionAST>> &functions){
			int numTemplates = templateNames.size(); 
			assert(templates[name][numTemplates].isValid() && "Template Redeclaration Error: A template Object with an identical name & number of templates already exists!"); 
			templates[name][numTemplates] = TemplateObject(objMembers, functions, templateNames); 
		}
		TemplateObject &generateFromTemplate(std::string & name, std::vector<std::unique_ptr<TypeExpr>> &types){
			int numTemplates = types.size(); 
			TemplateObject &obj = templates[name][numTemplates]; 
			for(int i = 0; i < obj.templateNames.size(); i++){
				obj.replaceTemplate(obj.templateNames[i], types[i]);
			} 
			return obj; 
		}
	};
}