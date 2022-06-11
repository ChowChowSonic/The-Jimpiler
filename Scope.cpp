#include <vector>
#include <string> 

using namespace std; 

class variable{
	private:
	bool isPointer = false; 
	KeyToken type = ERR;
	string name; 
	char * value; //I need bytes and there is no default byte type in C++. Fuck. 
	public:
	variable(){
	}

	variable(KeyToken datatype, string ident, int ptrsize = 1){
		type = datatype; 
		name = ident; 
		isPointer = ptrsize != 1; 
		switch(type){
			case FLOAT:
			case INT: 
			value = new char[ptrsize*sizeof(int)]; 
			break;
			case BYTE:
			case BOOL: 
			case CHAR:
			value = new char[ptrsize*sizeof(char)]; 
			break;
			default:
			value = new char[ptrsize*sizeof(int)]; 
			break;  
		}
	}
	
	virtual ~variable(){
		type = ERR; 
		isPointer = false; 
		delete &name; 
		delete[] value; 
	}

	virtual bool operator ==(variable v){
		return (this->name == v.name) && (this->type == v.type); 
	}
	
	string getName(){
		return name; 
	}

	KeyToken getType(){
		return type; 
	}
};

/**
 * @brief A block of code bounded by an opening and closing curly bracket, with variables inside of it.
 * A scope can access any variables owned by itself or it's parent (or it's parent's parent, or it's parent's parent's parent, etc...)
 * A scope with no parent is called a "base" scope. 
 * 
 * Currently unsafe (I think), and badly coded As all hell... still trying to debug stuff before I correct alot of these problems. 
 * 
 */
class scope{
	private: 
	bool hasPar = false; 
	scope * parent = 0; 
	//std::vector<string> vars; 
	std::vector<variable> vars; 
	//std::vector<unique_ptr<variable>> vars; //Lord help me unique_ptrs are hell to deal with

	public:
	/**
	 * @brief Construct a new base scope object - That is, a scope with no parent
	 * 
	 */
	scope(){
		//cout << "Base scope" <<endl; 
		this->hasPar = false; 
		this->parent = 0; 
	}
	/**
	 * @brief Construct a new scope object, with a parent
	 * 
	 * @param parnt 
	 */
	scope(scope* parnt){
		//cout << "Scope made" <<endl; 
		this->hasPar = true; 
		this->parent = parnt; 
	}

	~scope(){
		//cout << "Destroying scope: " <<this->getCascadingVars() <<endl; 
		hasPar = false; 
		parent = 0; 
		vars.clear();
		vars.shrink_to_fit(); 
	}//*/

	bool addVariable(KeyToken type, string s){
		variable b(type, s); 
		//unique_ptr<variable>* v = new unique_ptr<variable>(&b); 
		//vars.push_back(*new unique_ptr<variable>(&b));
		vars.emplace_back(b);
		return std::find(vars.begin(), vars.end(), b) == vars.end(); 
	}

	bool hasVariable(string s){
		//cout <<"Finding: "<< s << ", Vars: " << this->getVarsNames()<<endl; 
		for(variable v : vars){
			if(v.getName() == s) {
				//cout << "Returning true" <<endl; 
				return true; 
			}
		} 
		if(this->hasPar) {
			return parent->hasVariable(s); 
		}
		//cout << "Returning false" <<endl; 
		return false; 
	}
	
	bool hasVariable(variable v){
		return hasVariable(v.getName()); 
	}

	bool hasParent(){
		return this->hasPar; 
	}

	scope* getParentPointer(){
		return parent; 
	}

	string getVarsNames(){
		string s;
		//for(int i = 0; i <vars.size(); i++){
			//string s2 = vars[i].get()->getName(); 
		for(variable s2 : vars){
			s+=s2.getName();
			s+= ", "; 
		}
		//if(parent != 0) s+= parent->getVarsNames(); 
		if(s == "") s = "No variables found";
		return s;
	}

	string getCascadingVars(){
		string s;
		//for(int i = 0; i <vars.size(); i++){
		//	string s2 = vars[i].get()->getName(); 
		for(variable s2 : vars){
			s+=s2.getName();
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
	//I completely forget why I did this. Pretty sure it's unsafe to at least some extent...
	//I was probably just way more overtired than I thought I was. 
	/*static void operator delete(void* s){
		scope* s2 = static_cast<scope*>(s);
		s2->hasPar = false; 
		s2->parent = 0;
		for(int i = 0; i < s2->vars.size(); i++){
			//s2->vars[i].get_deleter(); 
			delete &s2->vars[i]; 
		}
		s2->vars.clear(); 
		s2->vars.shrink_to_fit();  
	}//*/

};