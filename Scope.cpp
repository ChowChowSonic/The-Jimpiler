#include <vector>
#include <string> 

using namespace std; 

class variable{
	private:
	bool isPointer = false; 
	KeyToken type = ERR;
	string name; 
	protected:
	variable(){
	}
	variable(KeyToken datatype, string ident, bool isPtr = false){
		type = datatype; 
		name = ident; 
		isPointer = isPtr; 
	}
	public:
	virtual ~variable(){
		type = ERR; 
		delete &name; 
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

class intVariable : public variable{
	private:
	int value = 0; 
	public:
	intVariable(KeyToken datatype, string ident, int val)		: variable(datatype, ident){
		value = val; 
	}

};

class floatVariable : public variable{
	private:
	float value = 0; 
	public:
	floatVariable(KeyToken datatype, string ident, float val)  	: variable(datatype, ident){
		value = val; 
	}
};

class boolVariable : public variable{
	private:
	bool value = false; 
	public:
	boolVariable(KeyToken datatype, string ident, bool val)  	: variable(datatype, ident){
		value = val; 
	}

};

class stringVariable : public variable{
	private:
	string value; 
	public:
	stringVariable(KeyToken datatype, string ident, string val)	: variable(datatype, ident){
		value = val; 
	}

	~stringVariable(){
		delete &value; 
	}

};
/**
 * @brief Not yet going to be used, will be implemented later
 * 
 */
class objectVariable : public variable{
	private:
	//byte * data; //vector<variable> vars; 
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
	std::vector<string> vars; 
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
		intVariable b = intVariable(type, s, 0); 
		//unique_ptr<variable>* v = new unique_ptr<variable>(&b); 
		//vars.push_back(*new unique_ptr<variable>(&b));
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
		//for(int i = 0; i <vars.size(); i++){
			//string s2 = vars[i].get()->getName(); 
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
		//for(int i = 0; i <vars.size(); i++){
		//	string s2 = vars[i].get()->getName(); 
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