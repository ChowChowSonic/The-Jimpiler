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
 * A scope can access any variables owned by itself or it's parent 
 * (or it's parent's parent, or it's parent's parent's parent, etc...)
 * A scope with no parent is called a "base" scope. 
 * 
 * Currently unsafe (I think), and badly coded As all hell... 
 * still trying to debug stuff before I correct alot of these problems. 
 * 
 */
static int IDs =0; 
class scope{
	private: 
	std::vector<variable> vars; 
	std::string asmcode, name; 
	std::stack<std::string> queuedinstructions; 
	scope * parent = 0; 
	int dumpcount = 0;
	//std::vector<unique_ptr<variable>> vars; //Lord help me unique_ptrs are hell to deal with

	public:
	/**
	 * @brief Construct a new base scope object - That is, a scope with no parent
	 * 
	 */
	scope(){
		this->parent = 0;  
		name = "Main"; 
		IDs++; 
		this->asmcode = name+":\n"; 
	}

	scope(const scope & cpy){
		this->parent = cpy.parent; 
		this->vars = cpy.vars; 
		name = cpy.name; 
		this->asmcode = cpy.asmcode; 
	}
	scope(scope* parnt, std::string ident){
		this->parent = parnt;
		name = ident; 
		this->asmcode = name+":\n";
	}
	/**
	 * @brief Construct a new scope object, with a parent
	 * 
	 * @param parnt 
	 */
	scope(scope * parnt) {
		this->parent = parnt;
		name = "Scope_"+to_string(IDs); 
		this->asmcode = name+":\n";
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
	 * @brief Get the Variable Names of THIS SCOPE ONLY - not the parent scopes. See getCascadingVars() 
	 * for parent scopes as well.
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
		s+=name; s+=": "; 
		for(variable s2 : vars){
			s+=s2.getName();
			s+= ", "; 
		}
		if(s == "") s = "No variables found locally ";
		
		if(this->hasParent()){
			s+= "/ ";
			s+= parent->getCascadingVars(); 
		}
		return s;
	}

	string toString(){
		string s = this->getVarsNames()+" "; 
		if(this->hasParent()) s+= "Has Parent";
		s+="\n";
		return s; 
	}

/**
 * Prepares an instruction to be inserted into the assembly code. Adds it to an internal stack, 
 * where it can be later removed from that stack and inserted onto the end of the asm code via pushInstruction()
 * @return void
 * @see pushInstruction()
*/
	void prepInstruction(std::string s){
		queuedinstructions.push(s); 
	}
/**
 * Takes the most recently pushed instruction on the internal instruction stack and inserts it into the assembly code.
 * returns true if it was successful, false if not
 * @return whether or not the operation was successful
*/
	bool pushInstruction(){
		if(queuedinstructions.empty()) return false; 
		writeASM(queuedinstructions.top()); 
		queuedinstructions.pop(); 
		return true; 
	}

	void writeASM(string s){
		asmcode.append(s); 
	}

/**
 * Writes assembly for this scope only into a new file named after the scope, then clears any prepared asm
*/
	void dumpASM(){
		fstream f; 
		string s = name+".s"; 
		f.open(s, std::fstream::out); 
		if(!f)
			f << asmcode << endl; 
		if(f.bad()){
			cout << "error writing to file"<<endl; 
		}else
			f.close(); 
		asmcode.clear(); 
		asmcode = name+"_Dump_"+to_string(dumpcount++)+':'; 
	}
/**
 * Writes assembly to an existing file, then clears any prepared asm
*/
	void dumpASM(fstream & out){
		out << asmcode << endl; 
		asmcode.clear(); 
		asmcode = name+"_Dump_"+to_string(dumpcount++)+':'; 
	}
/**
 * Writes assembly into a new or existing file with a given name. Then clears any prepared asm.
*/
	void dumpASM(string out){
		fstream f; 
		f.open(out, std::fstream::out | std::fstream::app);

		f << asmcode << endl; 
		f.close(); 
		asmcode.clear(); 
		asmcode = name+"_Dump_"+to_string(dumpcount++)+':'; 
	}

	std::string getName(){
		return name; 
	}

};