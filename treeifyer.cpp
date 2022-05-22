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
}

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
    //variables[t] = type; 
    //put a way to interpret statements here
    while( ((t = tokens.next()) != SEMICOL)){ //TBI
    }
    if(t ==SEMICOL) return true; 
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

bool FunctionParamList(Stack<Token>& tokens){
    Token t = tokens.next();
    if( t == RPAREN) return true; 
        if(t.token < 45){
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
            return FunctionParamList(tokens); 
        }
    return t == RPAREN; 
}

bool function(Stack<Token>& tokens){
bool b = FunctionParamList(tokens); 
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
    if(t.token >= 38 && t.token <= 42){
        //Operators.push_back(t) //Add this to the variable modifier memory
        return declareOrFunction(tokens); 
    } //*/
    if(t != IDENT && t.token < 45){
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
 * @brief Parses a logic statement consisting of only idents and string/number constants.
 * I need to add in the ability to parse valid complex statements later on
 * 
 * @param tokens 
 * @return true 
 * @return false 
 */
bool LogicStmt(Stack<Token>& tokens){
    Token t1 = tokens.next(); 
    if(t1 != IDENT && t1!=SCONST && t1!=NUMCONST){
        tokens.go_back(1); 
        return false; 
    }
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
        return LogicStmt(tokens); 
    }
    return true; 
}

bool ForStmt(Stack<Token>& tokens){
    Token t = tokens.next(); 
    if(t != LPAREN){
        tokens.go_back(1);
        return false; 
    }
    t = tokens.next(); 
    bool b = declare(tokens); 
    b = LogicStmt(tokens); 
    t = tokens.next();
    //b =

    t = tokens.next();
    if(t != OPENCURL){
        tokens.go_back(1);
        return false; 
    }openbrackets++; 
    return b; 
}

bool getValidStmt(Stack<Token>& tokens){
    bool status = false; 
    
    Token t = tokens.next(); 
    //cout << t.lex << " "; 
    switch(t.token){
        case IMPORT:
        return import(tokens); 
        break; 
        case FOR:
        return ForStmt(tokens); 
        case OBJECT:
        return obj(tokens); 
        case CONSTRUCTOR: 
        return construct(tokens); 
        case DESTRUCTOR:
        return destruct(tokens); 
        case IDENT: 
        return getValidStmt(tokens);
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
        tokens.go_back(1);
        return false;  
        }
    }
    return false; 
}
