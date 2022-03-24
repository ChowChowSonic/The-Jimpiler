using namespace std; 
//Syntax analyzer for the Compilier

//trying to initialize this isn't working and I have no clue why
map<Token, KeyToken> variables; 
class Node{
    private:
    Token obj; 
    Node *Left;
    Node *Right;  
    public:
    Node(Node Lchild, Token ob, Node Rchild){
        obj = ob; 
        Left = &Lchild; 
        Right = &Rchild; 
    }

    Node(Node Lchild, Token ob){
        obj = ob; 
        Left = &Lchild; 
    }

    Node(Node *Lchild, Node *Rchild){
        Left = Lchild; 
        Right = Rchild; 
    }
    
    Node(){
        return; 
    }
    void setRight(Node n){
        Right = &n; 
    }
    void setRight(Node *n){
        Right = n; 
    }
    void setLeft(Node n){
        Left = &n; 
    }
    void setLeft(Node *n){
        Left = n; 
    }
    Node getLeft() {
    return *Left;
    }
    Node getRight() {
    return *Right;
    }
};

bool import(Stack<Token> tokens){
    
    Token t = tokens.next(); 
    while(true){
        bool isvalid = (t.token == IDENT || t.token == COMMA || t.token == IMPORT); 
        if(t.token == SEMICOL) {
            return true; 
        }else if(!isvalid){ 
            return false; 
        }
        t = tokens.next(); 
    }
    return true; 
}

bool assign(Stack<Token> tokens, KeyToken type){
    Token t = tokens.next();
    if(t.token != IDENT)return false; 
    //variables[t] = type; 

    //put a way to interpret statements here
    while(t.token != SEMICOL || t.token != COMMA){
        t = tokens.next(); 
        if(t.token == COMMA) return assign(tokens, type); 
        else if(t.token == SEMICOL) return true;
    }

    return false; 
}

bool assign(Stack<Token> tokens){
    tokens.go_back(1); 
    Token t = tokens.next();
    KeyToken type = t.token; 
    t = tokens.next();
    if(t.token != IDENT)return false; 
    //variables[t] = type; 

    //put a way to interpret statements here
    while(t.token != SEMICOL || t.token != COMMA){
        //bool isvalid = (t.token >=20 && t.token <=24) || t.token == IDENT || t.token == NUMCONST; 
        t = tokens.next(); 
        if(t.token == COMMA) return assign(tokens, type); 
        else if(t.token == SEMICOL) return true;
        //if(!isvalid) return false; 
    }
    return false; 
}

bool getValidStmt(Stack<Token> tokens){
    bool status = false; 
    
    Token t = tokens.next(); 
    
    switch(t.token){
        case IMPORT:
        return import(tokens); 
        break; 
        case IDENT:
        return getValidStmt(tokens); //ignore a random unexpected statement
        break; 
        case INT: case SHORT: 
        case POINTER: case LONG: 
        case FLOAT: case DOUBLE: 
        case STRING: 
        case BOOL: case CHAR: 
        case BYTE:
        assign(tokens); 
        break; 
        default:{
        tokens.go_back(1);
        return false;  
        }
    }
}