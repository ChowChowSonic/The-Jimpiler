//#include "tokenizer.cpp"
//#include "Stack.cpp"
using namespace std; 
//Syntax analyzer for the Compilier

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

bool assign(Stack<Token>& tokens, KeyToken type){
    Token t = tokens.next();
    if(t.token != IDENT || t.token != CONST || t.token != SINGULAR){
        tokens.go_back(2); 
        return false;
        } 
    //variables[t] = type; 

    //put a way to interpret statements here
    while(t.token != SEMICOL || t.token != COMMA){
        t = tokens.next(); 
        if(t.token == COMMA) return assign(tokens, type); 
        else if(t.token == SEMICOL) return true;
    }
    tokens.go_back(2); 
    return false; 
}

bool assign(Stack<Token>& tokens){
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
    }
    //go into the method that defines an object:
    //TBI
    //Need to handle closing curly bracket
    return true; 
}

/**
 * @brief Called whenever the words "public", "private", "protected", or a class of object/primitive ("int", "string", "object") are seen.
 * Essentially we don't know from here if what's being defined is a function or a variable, so we just try to account for anything that comes up.
 * 
 * @param tokens 
 * @return true if a valid function
 */
bool AssignOrFunction(Stack<Token>& tokens){
    Token t = tokens.next();
    if(t.token >= 38 && t.token <= 42){
        //Operators.push_back(t) //Add this to the variable modifier memory
        return AssignOrFunction(tokens); 
    } //*/
    if(t != IDENT && t.token <= 44){
        tokens.go_back(1); 
        return false;
    }
    if(t!=IDENT) t = tokens.next(); 

    if(t.token == IDENT){
        t = tokens.next(); 
        if(t.token == LPAREN){//It's a function
            return true; 
            //return function(tokens); //TBI
        }else if(t== EQUALS){//It's a variable
            tokens.go_back(2); 
            return assign(tokens); 
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
    return true; 
    //Need to handle closing curly bracket
}

bool getValidStmt(Stack<Token>& tokens){
    bool status = false; 
    
    Token t = tokens.next(); 
    //cout << t.lex << " "; 
    switch(t.token){
        case IMPORT:
        return import(tokens); 
        break; 
        case OBJECT:
        return obj(tokens); 
        case RPAREN: case LPAREN:
        return AssignOrFunction(tokens); 
        break; 
        case CONSTRUCTOR: 
        return construct(tokens); 
        case DESTRUCTOR:
        return destruct(tokens); 
        case IDENT: 
        case OPENCURL: case CLOSECURL: //move these around later
        return getValidStmt(tokens); //ignore a random unexpected statement
        break; 
        case PUBLIC: case PRIVATE: case PROTECTED: 
        case SINGULAR: case CONST:
        case INT: case SHORT: 
        case POINTER: case LONG: 
        case FLOAT: case DOUBLE: 
        case STRING: 
        case BOOL: case CHAR: 
        case BYTE:
        return AssignOrFunction(tokens); 
        default:{
        tokens.go_back(1);
        return false;  
        }
    }
    return false; 
}