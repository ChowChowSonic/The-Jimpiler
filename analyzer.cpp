//#include "tokenizer.cpp"
//#include "Stack.cpp"
using namespace std; 
//Syntax analyzer for the Compilier

/**
 * @brief The number of open curly brackets that the current function is nested within. Whenever an opening bracket is seen ("{"),
 * this number is incremented by 1. Whenever a closing curly bracket is seen ("}"), it is reduced by 1. 
 * 
 */
int openbrackets = 0; 
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
/**
 * @brief Parses a logic statement (bool value; I.E. "x == 5") consisting of only idents and string/number constants.
 * I need to add in the ability to parse valid complex statements later on
 * 
 * @param tokens 
 * @return true 
 * @return false 
 */
bool logicStmt(Stack<Token>& tokens){
	Token t1 = tokens.next(); 
	if(t1 == TRU || t1 == FALS) return true; 
	//bool isbool = {} //True if t1 is a variable/function of the type bool
	if(t1 != IDENT && t1!=SCONST && t1!=NUMCONST){ //Check if it's a boolean when implemented
	
		tokens.go_back(1); 
		return false; 
	}
	//if(isbool){
	t1 = tokens.next(); 
	if(t1 != EQUALCMP && t1!=GREATER && t1!=LESS){
		tokens.go_back(1); 
		return false; 
	}
	t1=tokens.next(); 
	if(t1 != IDENT && t1!=SCONST && t1!=NUMCONST){
		tokens.go_back(1); 
		return false; 
	}
	if(tokens.peek() == AND || tokens.peek() == OR) {
		tokens.next(); 
		return logicStmt(tokens); 
	}
	//}
	return true; 
}


/**
 * @brief using EBNF notation, a term is defined as: 
 * <number> = <IDENT> (*of integer type*) | <NUMCONST>; 
 *   <term> = (["+"|"-"]["++"|"--"] | "->" | "@")<number>["++"|"--"]; 
 * However the ending [++|--] are only allowed if there is no ++|-- at the beginning of the term
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
		return t == IDENT; 
	}else if(t == POINTERTO || t == REFRENCETO){
		return tokens.next() == IDENT; 
	}else if(t == IDENT && (tokens.peek() == INCREMENT || tokens.peek() == DECREMENT)){
		tokens.next(); 
		return true; 
	}
	return t == IDENT || t == NUMCONST; 
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
	KeyToken type = t.token; 
	t = tokens.next();
	if(t.token != IDENT){
		
		tokens.go_back(2); 
		return false; 
	}
	
	 if(tokens.peek() ==SEMICOL) return true; 
	 else if(tokens.next() == EQUALS){ 
		 bool b = false; 
		 if(type == BOOL)b = logicStmt(tokens);
		 else b = expr(tokens);
		return b && tokens.next() == SEMICOL; 
	}
	 
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
	if(tokens.next().token != OPENCURL){
		return false; 
	}openbrackets++; 
	//go into the method that defines an object:
	//TBI
	//Need to handle closing curly bracket
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

bool function(Stack<Token>& tokens){
bool b = functionParamList(tokens); 
if(tokens.next() != OPENCURL){
	tokens.go_back(1); 
	return false; 
}openbrackets++; 
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
	if(t!=IDENT) t = tokens.next(); 

	if(t.token == IDENT){
		t = tokens.next(); 
		if(t.token == LPAREN){//It's a function
			return function(tokens); 
			//return function(tokens); //TBI
		}else if(t== EQUALS){//It's a variable
			tokens.go_back(2); 
			return declare(tokens); 
		}else if(t == SEMICOL){//It's a variable
			return true; 
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
	t = tokens.next(); 
	if(t != OPENCURL){
		tokens.go_back(1); 
		return false; 
	}
	openbrackets++; 
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
	t = tokens.next(); 
	if(t != OPENCURL){
		tokens.go_back(1); 
		return false; 
	}
	openbrackets++; 
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
	if(t == INCREMENT || t == DECREMENT){return tokens.next() == IDENT;}
	if(t != IDENT){
		tokens.go_back();
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
	t = tokens.next();
	if(t != OPENCURL){
		tokens.go_back(1);
		return false; 
	}openbrackets++; 
	return b; 
}

bool ifStmt(Stack<Token>& tokens){
	Token t = tokens.next(); 
	if(t!=LPAREN){
		tokens.go_back(1);
		return false; 
	}
	bool b = logicStmt(tokens); 
	t = tokens.next(); 
	if(t!=RPAREN){
		tokens.go_back(1);
		return false; 
	}
	t = tokens.next(); 
	if(t!=OPENCURL){
		tokens.go_back(1);
		return false; 
	}openbrackets++; 
}
bool caseStmt(Stack<Token>& tokens){
	//int availablebrackets = 0; // equals zero if the next closing curly is for the case stmt
	Token variable = tokens.next(), t; 
	//if(tokens.next() == OPENCURL) openbrackets++; 
	while((t = tokens.next()) != CLOSECURL) {
		if(t == OPENCURL) openbrackets++; 
		if(t == CLOSECURL) openbrackets--;
	}
	if(t == CLOSECURL) openbrackets--; 
	return true; //Leaving this for now because I have to figure the specifics out later... 
	//I Probably just need a simple call to getNextStmt(); 
}

bool caseSwitchStmt(Stack<Token>& tokens){
	//int availablebrackets = 0; // equals zero if the next closing curly is for the switch stmt
	if(tokens.next() == LPAREN) Token variable = tokens.next(); 
	else return false; 
	if(tokens.next() != RPAREN) return false; 
	if(tokens.next() != OPENCURL) return false; 
	else openbrackets++; 
	Token next; bool b = true; 
	while( (next = tokens.next()) == CASE && b){
		b = caseStmt(tokens); 
	}
	if(next == CLOSECURL) openbrackets--; 
	return b; 
}

/**
 * @brief Get the next Valid Stmt in the code file provided. 
 * 
 * @param tokens 
 * @return true 
 * @return false 
 */
bool getValidStmt(Stack<Token>& tokens){
	bool status = false; 
	Token t = tokens.next(); 
	switch(t.token){
		case IMPORT:
		return import(tokens); 
		case FOR:
		return forStmt(tokens); 
		case IF:
		return ifStmt(tokens); 
		case SWITCH://heh
		return caseSwitchStmt(tokens);
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
		case OPENCURL: 
		openbrackets++;
		break; 
		case CLOSECURL: //move these around later
		openbrackets--; 
		if(openbrackets < 0){
			cout << "unbalanced brackets" <<endl; 
			tokens.go_back(1);
			return false; 
		}
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
			cout << "ERROR: Unknown token"<<endl ;
			tokens.go_back(1);
			return false;  
		}
	}
	return false; 
}
