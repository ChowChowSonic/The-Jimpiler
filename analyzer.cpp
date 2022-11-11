//#include "tokenizer.cpp"
//#include "Stack.cpp"
using namespace std; 
//Syntax analyzer for the Compilier
scope * currentScope = 0; 
vector<scope> scopes(1);//Can't use a default constructor because APPARENTLY if I try that it overlaps with currentScope and corrupts the end result???????? 
bool import(Stack<Token>& tokens){
	Token t = tokens.next(); 
	while((t.token == IDENT || t.token == COMMA) && t.token != SEMICOL){
		t = tokens.next(); 
	}
	if(t.token != SEMICOL){ 
			tokens.go_back(2); 
			return false; 
	}
	return true; 
}

bool addScope(){
		scopes.emplace_back(currentScope); 
		currentScope = &scopes[scopes.size()-1];
		return true; 
}

/* //I don't think I need this?
bool declare(Stack<Token>& tokens, KeyToken type){
	Token t = tokens.next();
	if(t.token != IDENT || t.token != CONST || t.token != SINGULAR){
		tokens.go_back(2); 
		return false;
		} 
	//variables[t] = type; 

	//put a way to interpret statements here
	while(t.token != SEMICOL || t.token != COMMA){
		t = tokens.next(); 
		if(t.token == COMMA) return declare(tokens, type); 
		else if(t.token == SEMICOL) return true;
	}
	tokens.go_back(2); 
	return false; 
}*/
bool logicStmt(Stack<Token>& tokens); 

bool logicHelper(Stack<Token>& tokens){
	Token t1; 
	//cout << "In helper\n";
	if((t1 = tokens.next()) == LPAREN){
		return logicStmt(tokens); 
	} 
	if(t1 == TRU || t1 == FALS){ return true;} 
	//cout << t1; 
	if(t1 != IDENT && t1 !=SCONST && t1 !=NUMCONST)return false; 
	t1 = tokens.next(); //cout <<t1; 
	if(t1 != GREATER && t1 != LESS && t1 != EQUALCMP) return false; 
	t1 = tokens.next(); //cout <<t1; 
	if(t1 != IDENT && t1 !=SCONST && t1 !=NUMCONST)return false; 
	//cout << "Out of helper\n";
	return true; 
}


/**
 * @brief Parses a logic statement (bool value; I.E. "x == 5") consisting of only idents and string/number constants.
 * logicStmt 	-> (?<helper>)? <Join> (?<helper>)?
 * helper		-> <base> | <logicStmt>
 * base			-> <terminal> <op> <terminal> | "true" | "false";
 * op			-> "==" | ">=" | "<=" | ">" | "<"
 * join			-> "and" | "or"
 * terminal		-> SCONST | NUMCONST | VARIABLE //Literal string number or variable values
 * 
 * @param tokens 
 * @return true 
 * @return false 
 */
bool logicStmt(Stack<Token>& tokens){
	bool x = false; 

	// (? <helper> )? 
	x = logicHelper(tokens);
	if(!x) return false; 
	if(tokens.next() != RPAREN) tokens.go_back(); 
	
	//<join>
	if(tokens.peek() != AND && tokens.peek() != OR){ return true; }
	tokens.next(); 
	

	// (? <helper> )?
	x = logicHelper(tokens);
	if(!x)return false ;
	if(tokens.next() != RPAREN) tokens.go_back(); 
	
	return true; 
	
}


/**
 * @brief using EBNF notation, a term is defined as: 
 * <number> = <IDENT> (*of integer type*) | <NUMCONST>; 
 *   <term> = (["+"|"-"]["++"|"--"] | "->" | "@")<number>["++"|"--"]; 
 * However the ending [++|--] are only allowed if there is no (++|--) at the beginning of the term
 * This method will probably be compiled into the getBoolStmt function, or something similar at a later point
 * 
 * @param tokens 
 * @return true if the syntax is valid
 * @return false if syntax is not valid
 */
bool term(Stack<Token>& tokens){
	//int x = 0++;  //Testing C++ syntax and what's valid
	Token t = tokens.next(); 
	if(t == PLUS || t == MINUS){
		t = tokens.next(); 
		//return t == IDENT || t == NUMCONST; 
	}
	if(t == INCREMENT || t == DECREMENT){
		t = tokens.next();
		bool b = t == IDENT && currentScope->hasVariable(t.lex); 
		if(!b) {cout << "Unidentified token: " << t.lex <<endl; tokens.go_back();}
		return b; 
	}else if(t == POINTERTO || t == REFRENCETO){
		t = tokens.next(); 
		bool b = t == IDENT && currentScope->hasVariable(t.lex); 
		if(!b) {cout << "Unidentified token: " << t.lex <<endl; tokens.go_back();}
		return b; 
	}else if(t == IDENT && (tokens.peek() == INCREMENT || tokens.peek() == DECREMENT)){
		tokens.next(); 
		bool b = currentScope->hasVariable(t.lex); 
		if(!b) {cout << "Unidentified token: " << t.lex <<endl; tokens.go_back();}
		return b; 
	}else if(t == IDENT){
		bool b = currentScope->hasVariable(t.lex); 
		if(!b) {cout << "Unidentified token: " << t.lex <<endl;}
		return b; 
	}
	return t == NUMCONST; 
}

bool expr(Stack<Token>& tokens){
	bool isValidTerm = term(tokens); 
	if( !isValidTerm ) {
		tokens.go_back(); 
		return false;
	}
	Token t = tokens.next(); 
	while ( t == PLUS || t == MINUS ){
		isValidTerm = term(tokens); 
		if(!isValidTerm){
			tokens.go_back();
			return false; 
		}
		t = tokens.next(); 
	}
	tokens.go_back(); 
	return true; 
}

/**
 * @brief Called whenever a primitive type or object type keyword is seen. 
 * 
 * @param tokens 
 * @return true 
 * @return false 
 */
bool declare(Stack<Token>& tokens){ 
	tokens.go_back(1); 
	Token t = tokens.next();
	KeyToken type = t.token; //int, char, bool, etc. 
	t = tokens.next();
	string id = t.lex; //the variable name
	//cout <<currentScope->hasVariable(t.lex) <<endl; 
	if(t.token != IDENT){
		cout << "Error: Not an identifier: " << t.lex <<endl; 
		tokens.go_back(2); 
		return false; 
	}else if(currentScope->hasVariable(t.lex)){
		cout << "Variable redclaration" << endl; 
		tokens.go_back(2);
		return false; 
	}
	//cout << "Adding variable: " << t; 
	//cout << currentScope->getCascadingVars()<<endl<<endl; 
	if(tokens.peek() ==SEMICOL){
		 tokens.next(); 
		 currentScope->addVariable(type, t.lex); 
		 return true; 
	}else if(tokens.next() == EQUALS){ 
		bool b = false; 
		if(type == BOOL) b = logicStmt(tokens);
		else b = expr(tokens);
		//tokens.go_back(); 
		//cout <<tokens.peek(); 
		if(!b){
			//tokens.go_back(); 
			//cout << tokens.peek(); 
			if(tokens.next().lex == id) cout << "Cannot use variable in its own declaration statement" << endl; 
			tokens.go_back();
			return false; 
		}
	}
	//cout << t.lex <<endl ; 
	currentScope->addVariable(type, id); 
	t = tokens.next(); 
	//variables[t] = type; 
	//put a way to interpret statements here
	if(t == SEMICOL) return true;
	tokens.go_back(2); 
	return false; 
}

bool obj(Stack<Token>& tokens){
	if(tokens.next().token != IDENT){
		return false; 
	}
	return true; 
}

bool functionParamList(Stack<Token>& tokens){
	Token t = tokens.next();
	if( t == RPAREN) return true; 
		if(t.token <= INT){
			tokens.go_back(1); 
			return false; 
		}
		t = tokens.next(); 
		if( t != IDENT ){
			tokens.go_back(1); 
			return false; 
		}
		t = tokens.next(); 
		if( t == COMMA){
			return functionParamList(tokens); 
		}
	return t == RPAREN; 
}

bool func(Stack<Token>& tokens){
	bool b = functionParamList(tokens); 
	return b; 
}

/**
 * @brief Called whenever the words "public", "private", "protected", or a class of object/primitive ("int", "string", "object") are seen.
 * Essentially we don't know from here if what's being defined is a function or a variable, so we just try to account for anything that comes up.
 * 
 * @param tokens 
 * @return true if a valid function
 */
bool declareOrFunction(Stack<Token>& tokens){
	Token t = tokens.next();
	if(t.token >= CONST && t.token <= PROTECTED){
		//Operators.push_back(t) //Add this to the variable modifier memory
		return declareOrFunction(tokens); 
	} //*/
	if(t != IDENT && t.token < INT){
		tokens.go_back(1); 
		return false;
	}
	if(t!=IDENT){t = tokens.next(); }
	if(t.token == IDENT){
		t = tokens.next(); 
		if(t.token == LPAREN){//It's a function
			return func(tokens); 
			//return function(tokens); //TBI
		}else if(t== EQUALS || t == SEMICOL){//It's a variable
			tokens.go_back(2); 
			return declare(tokens); 
		}
	} 
	tokens.go_back(2);
	return false; 
}

bool construct(Stack<Token>& tokens){
	Token t = tokens.next(); 
	if(t != LPAREN){
		tokens.go_back(1); 
		return false; 
	}
	t=tokens.next(); 
	if(t != RPAREN){
		tokens.go_back(1); 
		return false; 
	}
	return true; 
	//Need to handle closing curly bracket
}

bool destruct(Stack<Token>& tokens){
	Token t = tokens.next(); 
	if(t != LPAREN){
		tokens.go_back(1); 
		return false; 
	}
	t=tokens.next(); 
	if(t != RPAREN){
		tokens.go_back(1); 
		return false; 
	}
	return true; 
	//Need to handle closing curly bracket
}
/**
 * @brief Described in short using EBNF, generic statements consist of: \n 
 * ( <variable>["++"|"--"|"=", <stmts>] ) | [++|--]<variable>; 
 * 
 * @param tokens 
 * @return true 
 * @return false 
 */
bool genericStmt(Stack<Token>& tokens){
	Token t = tokens.next(); 
	if(t == INCREMENT || t == DECREMENT){
		Token t2 = tokens.next(); 
		//cout << t2;
		bool b = t2 == IDENT && currentScope->hasVariable(t2.lex);
		if(!b){
			tokens.go_back(1); 
			cout << "Unidentified token: " << t2.lex <<endl; 
		}
		return b; 
	}
	if(t != IDENT){
		tokens.go_back();
		return false; 
	}else if(!currentScope->hasVariable(t.lex)){
		tokens.go_back(); 
			cout << "Unidentified token: " << t.lex <<endl; 
			return false; 
	}
	t = tokens.next(); 
	if(t == INCREMENT || t == DECREMENT) return true;
	if(t == EQUALS){
		return expr(tokens); 
	}
	return false; 
}

bool forStmt(Stack<Token>& tokens){
	Token t = tokens.next(); 
	if(t != LPAREN){
		tokens.go_back(1);
		return false; 
	}
	t = tokens.next(); 
	bool b = declare(tokens); 
	b = logicStmt(tokens); 
	t = tokens.next();
	b = expr(tokens); //TBI alongside variable checking 
	t = tokens.next();
	if(t != RPAREN){
		tokens.go_back(1);
		return false;
	}
	return b; 
}

bool ifStmt(Stack<Token>& tokens){
	//Token t = tokens.next(); 
	/*if(t!=LPAREN){
		tokens.go_back(1);
		return false; 
	}//*/
	bool b = logicStmt(tokens); 
	//t = tokens.next(); 
	/*if(t!=RPAREN){
		tokens.go_back(1);
		return false; 
	}//*/
	return true; 
}
bool caseStmt(Stack<Token>& tokens){
	//int availablebrackets = 0; // equals zero if the next closing curly is for the case stmt
	Token variable = tokens.next(); 
	if(variable.token != SCONST && variable.token != NUMCONST && variable.token != TRU && variable.token != FALS) return false; 
	//if(tokens.next() == OPENCURL) openbrackets++; 
	//while((t = tokens.next()) != CLOSECURL) {
	//}
	return true; //Leaving this for now because I have to figure the specifics out later... 
	//I Probably just need a simple call to getNextStmt(); 
}

bool caseSwitchStmt(Stack<Token>& tokens){
	//int availablebrackets = 0; // equals zero if the next closing curly is for the switch stmt
	if(tokens.next() == LPAREN) Token variable = tokens.next(); 
	else return false; 
	if(tokens.next() != RPAREN) return false; 
	if(tokens.peek() != OPENCURL) return false; 
	Token next;
	//while( (next = tokens.next()) == CASE && b){
	//	b = caseStmt(tokens); 
	//}
	return true; 
}
// /*
void printAllScopes(){
	for(int i = 0; i < scopes.size(); i++){
		cout<< "Number " <<i<<": "<<scopes[i].getCascadingVars() <<endl;
	}
	cout << "End of scopes" <<endl<<endl;
}//*/

/**
 * @brief Get the next Valid Stmt in the code file provided. 
 * 
 * @param tokens 
 * @return true 
 * @return false 
 */
bool getValidStmt(Stack<Token>& tokens){
	//Reminder: Vector of scopes has to keep moving whenever the list gets extended
	//so the pointers are no longer usable; occasionally everything gets corrupted
	if(currentScope == 0) {
		addScope(); 
	}
	bool status = false; 
	Token t = tokens.next(); 
	//cout << t; 
	switch(t.token){
		case IMPORT:
		return import(tokens); 
		case FOR:
		return forStmt(tokens); 
		case IF:
		return ifStmt(tokens); 
		case SWITCH://heh
		return caseSwitchStmt(tokens);
		case CASE:
		return caseStmt(tokens);
		case OBJECT:
		return obj(tokens); 
		case CONSTRUCTOR: 
		return construct(tokens); 
		case DESTRUCTOR:
		return destruct(tokens); 
		case IDENT: 
		case INCREMENT:
		case DECREMENT:
		tokens.go_back(1); 
		return genericStmt(tokens) && tokens.next() == SEMICOL; 
		case OPENCURL: {
		return addScope(); 
		}
		case CLOSECURL:
		if(currentScope->getParentPointer() == 0){
			cout << "unbalanced brackets" <<endl; 
			tokens.go_back(1);
			return false; 
		}
		currentScope = currentScope->getParentPointer();
		return true; 
		case INT:       case SHORT: //int i = 0; << We start at the int token, then have to move to the ident, so we go back 1 to undo that
		case POINTER:   case LONG: 
		case FLOAT:     case DOUBLE: 
		case STRING:    case CHAR:
		case BOOL:      case BYTE:
		tokens.go_back(1); 
		case PUBLIC: case PRIVATE: case PROTECTED: 
		case SINGULAR: case CONST:
		return declareOrFunction(tokens); 
		default:{
			cout << "ERROR: Unknown token: \n" << t <<endl ;
			tokens.go_back(1);
			return false;  
		}
	}
	cout << "Returning false by error?"<<endl;
	return false; 
}

void deleteScopes(){
	scopes.clear(); 
	scopes.shrink_to_fit(); 
}