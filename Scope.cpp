#include <vector>
#include <string> 
using namespace std; 
/**
 * @brief A block of code bounded by an opening and closing curly bracket, with variables inside of it.
 * A scope can access any variables owned by itself or it's parent (or it's parent's parent, or it's parent's parent's parent, etc...)
 * A scope with no parent is called a "base" scope. 
 * 
 * Currently unsafe, needs a delete method, and badly coded As all hell... still trying to debug stuff before I correct alot of these problems. 
 * 
 */
class scope{
	private: 
	bool hasPar = false; 
	scope * parent = 0; 
	std::vector<string> vars; 

	public:
	/**
	 * @brief Construct a new base scope object - That is, a scope with no parent
	 * 
	 */
	scope(){
		this->hasPar = false; 
		this->parent = 0; 
	}

	scope(scope* parnt){
		this->hasPar = true; 
		this->parent = parnt; 
	}

	bool addVariable(string s){
		vars.push_back(s);
		return std::find(vars.begin(), vars.end(), s) == vars.end(); 
	}

	bool hasVariable(string s){
		//cout <<"Finding: "<< s << ", Vars: " << this->getVarsNames()<<endl; 
		if(find(vars.begin(), vars.end(), s) != vars.end()) {
			//cout << "Returning true" <<endl; 
			return true; 
		}else if(this->hasPar) {
			return parent->hasVariable(s); 
		}
		//cout << "Returning false" <<endl; 
		return false; 
	}

	bool hasParent(){
		return this->hasPar; 
	}

	scope* getParentPointer(){
		return parent; 
	}

	string getVarsNames(){
		string s;
		for(string s2 : vars){
			s+=s2;
			s+= ", "; 
		}
		//if(parent != 0) s+= parent->getVarsNames(); 
		if(s == "") s = "No variables found";
		return s;
	}

	string getCascadingVars(){
		string s;
		for(string s2 : vars){
			s+=s2;
			s+= ", "; 
		}
		if(s == "") s = "No variables found locally ";
		s+= " / ";
		//if(this->hasPar || parent != 0) cout << endl << parent << " " << ((this->hasPar)? "true" : "false"); 
		if(hasPar)s+= parent->getCascadingVars(); 
		else{
			s+= " No further scopes found"; 
		}
		//cout << parent<<endl ; 
		return s;
	}

	string toString(){
		string s = this->getVarsNames()+" "; 
		if(this->hasPar) s+= "Has Parent";
		s+="\n";
		return s; 
	}

}; 