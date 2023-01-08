using namespace std; 

class variable{
	private:
	//bool isPointer = false; 
	KeyToken type = ERR;
	string name; 
	//char * value = NULL; //I need bytes and there is no default byte type in C++. Fuck. 
	public:
	variable(){
	}

	variable(KeyToken datatype, string ident, int ptrsize = 1){
		type = datatype; 
		name = ident; 
	}
	
	virtual ~variable(){
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
	scope * parent = 0; 
	std::vector<variable> vars; 
	//std::vector<unique_ptr<variable>> vars; //Lord help me unique_ptrs are hell to deal with

	public:
	/**
	 * @brief Construct a new base scope object - That is, a scope with no parent
	 * 
	 */
	scope(){
		this->parent = 0;  
	}
	scope(const scope & cpy){
		this->parent = cpy.parent; 
		this->vars = cpy.vars; 
	}
	/**
	 * @brief Construct a new scope object, with a parent
	 * 
	 * @param parnt 
	 */
	scope(scope * parnt){
		this->parent = parnt; 
	}

	bool addVariable(KeyToken type, string s){
		variable b = variable(type, s); 
		//unique_ptr<variable>* v = new unique_ptr<variable>(&b); 
		//vars.push_back(*new unique_ptr<variable>(&b));
		vars.push_back(b);
		return true;
	}

	bool hasVariable(string s){
		for(variable & v : vars){
			if(v.getName() == s) return true; 

		}
		if(this->hasParent()){
			return this->parent->hasVariable(s); 
		} 
		return false; ///find(vars.begin(), vars.end(), s) != vars.end(); 
	}
	
	bool hasVariable(variable &v){
		
		if(this->hasParent()) {
			return parent->hasVariable(v); 
		}
		return find(vars.begin(), vars.end(), v) != vars.end(); 
	}

	bool hasParent(){
		return this->parent != NULL; 
	}

	scope* getParentPointer(){
		return parent; 
	}
	/**
	 * @brief Get the Variable Names of THIS SCOPE ONLY - not the parent scopes. See getCascadingVars() for parent scopes as well.
	 * 
	 * @return string 
	 */
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
	/**
	 * @brief Get the Cascading Vars of this scope - AKA, get the variables of this scope and all parent scopes
	 * 
	 * @return string 
	 */
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
		if(this->hasParent())s+= parent->getCascadingVars(); 
		else{
			s+= " No further scopes found"; 
		}
		return s;
	}

	string toString(){
		string s = this->getVarsNames()+" "; 
		if(this->hasParent()) s+= "Has Parent";
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