using namespace std;
// Syntax analyzer for the Compilier
scope *currentScope = 0;
vector<scope> scopes(1); // Can't use a default constructor because APPARENTLY if I try that it overlaps with currentScope and corrupts the end result????????

bool analyzeFile(string fileDir);

bool import(Stack<Token> &tokens)
{
	Token t;
	bool b = true;
	while ((t = tokens.next()).token != SEMICOL && (t.token == IDENT || t.token == COMMA) && b)
	{
		if (t == IDENT)
		{
			string s = t.lex + ".jmb";
			b = analyzeFile(s);
		}
	}
	if (t.token != SEMICOL || !b)
	{
		tokens.go_back(2);
		return false;
	}
	return b;
}

bool isValidVariable(Token &t)
{
	bool x = (t == IDENT) && currentScope->hasVariable(t.lex);
	if (!x)
		cout << "Error: Unknown variable '" << t.lex << "' at position:" << endl;
	return x;
}

bool addScope(){
	scope s;
	scopes.emplace_back(currentScope);
	currentScope = &scopes[scopes.size() - 1];
	return true;
}
bool mathExpr(Stack<Token> &tokens); 
bool logicStmt(Stack<Token> &tokens);

bool greaterLessStmt(Stack<Token>& tokens){
	bool b = mathExpr(tokens); 
	if(!b) return false; 
	Token t = tokens.next(); 
	while(t == GREATER || t == GREATEREQUALS || t == LESS || t == LESSEQUALS){
		b = mathExpr(tokens); 
		if(!b) return false; 
		t = tokens.next(); 
	}
	tokens.go_back(); 
	return true; 

}

/**
 * @brief checks for a basic <IDENT> (["=="|"!="] <IDENT>)* However, 
 * I am torn between enabling N-way comparison (I.E.: "x == y == z == a == b").
 * No other programming languages that I know of include this, and I have a 
 * feeling it's for good reason... But at the same time, I want to add it as I 
 * feel that is extremely convienent in the situations it comes up in. 
 * 
 * @param tokens 
 * @return true - if it is a valid statement,
 * @return false otherwise
 */
bool compareStmt(Stack<Token>& tokens){
	// cout << t1;
	bool b = greaterLessStmt(tokens); 
	Token t1 = tokens.next(); //cout <<t1;
	//Keeping N-way comparison in for now, but I might remove it later
	while (t1 == EQUALCMP || t1 == NOTEQUAL){
		b = greaterLessStmt(tokens); 
		if(!b) return false; 
		t1 = tokens.next(); 
	}

	tokens.go_back(); 
	// cout << "Out of helper\n";
	return b;
}

/**
 * @brief Parses a logic statement (bool value; I.E. "x == 5") consisting of only idents and string/number constants.
 * EBNF isn't technically 100% accurate, but that's because a few assumptions should be made going into this, like, 
 * for example, all open parenthesis should have a matching closing parenthesis 
 * logicStmt 	-> <Stmt> <join> <logicStmt> | <Stmt>
 * Stmt			-> "("?<helper>")"? <Join> "("?<helper>")"?
 * helper		-> <base> | <logicStmt>
 * base			-> <terminal> <op> <terminal> | "true" | "false";
 * op			-> "==" | ">=" | "<=" | ">" | "<"
 * join			-> "and" | "or"
 * terminal		-> SCONST | NUMCONST | VARIABLE //Literal string number or variable values
 *
 * TODO: Plan out operator precidence so that you can write code without/minimally hitting shift
 * 
 * @param tokens
 * @return true - if it is a valid logic statement,
 * @return false otherwise
 */
bool logicStmt(Stack<Token> &tokens){
	if(tokens.peek() == NOT) tokens.next(); 
	bool isValidTerm = compareStmt(tokens);
	if (!isValidTerm)
	{
		tokens.go_back();
		return false;
	}
	Token t = tokens.next();
	while (t == AND || t == OR)
	{
		if(tokens.peek() == NOT) tokens.next(); 
		isValidTerm = compareStmt(tokens);
		if (!isValidTerm)
		{
			
			return false;
		}
		t = tokens.next();
	}
	tokens.go_back();
	return true;
}

/**
 * @brief using EBNF notation, a term is roughly defined as:
 *   <term> = (["+"|"-"]["++"|"--"] | "->" | "@")<IDENT>["++"|"--"] | "true" | "false" | "(" <expr> ")";
 * However the ending [++|--] are only allowed if there is no (++|--) at the beginning of the term
 *
 * @param tokens
 * @return true if the syntax is valid
 * @return false if syntax is not valid
 */
bool term(Stack<Token> &tokens){
	// int x = 0++;  //Testing C++ syntax and what's valid
	if(tokens.peek() == LPAREN){
		tokens.next();
		bool t = logicStmt(tokens); 
		if(tokens.peek() != RPAREN || !t) {
			if(t)
				cout << "Expected a closing parenthesis" << endl; 
			return false;
		}  
		tokens.next(); 
		return true; 
	}
	Token t = tokens.next();
	if(t == TRU || t == FALS) return true;

	if (t == PLUS || t == MINUS)
	{
		t = tokens.next();
		// return t == IDENT || t == NUMCONST;
	}
	if (t == INCREMENT || t == DECREMENT)
	{
		t = tokens.next();
		bool b = t == IDENT && currentScope->hasVariable(t.lex);
		if (!b)
		{
			cout << "Unidentified token: " << t.lex << endl;
			tokens.go_back();
		}
		return b;
	}
	else if (t == POINTERTO || t == REFRENCETO)
	{
		t = tokens.next();
		bool b = t == IDENT && currentScope->hasVariable(t.lex);
		if (!b)
		{
			cout << "Unidentified token: " << t.lex << endl;
			tokens.go_back();
		}
		return b;
	}
	else if (t == IDENT && (tokens.peek() == INCREMENT || tokens.peek() == DECREMENT))
	{
		tokens.next();
		bool b = currentScope->hasVariable(t.lex);
		if (!b)
		{
			cout << "Unidentified token: " << t.lex << endl;
			tokens.go_back();
		}
		return b;
	}
	else if (t == IDENT)
	{
		bool b = currentScope->hasVariable(t.lex);
		if (!b)
		{
			cout << "Unidentified token: " << t.lex << endl;
		}
		return b;
	}
	
	return t == NUMCONST;
}

bool raisedToExpr(Stack<Token> &tokens){
	bool t = term(tokens);
	if (!t)
	{
		return false;
	}
	Token tok = tokens.next(); 
	while (tok == POWERTO || tok == LEFTOVER){
		bool isValidTerm = term(tokens);
		if (!isValidTerm)
		{
			return false;
		}
		tok = tokens.next();
	}
	tokens.go_back(); 
	return true; 
}
bool multAndDivExpr(Stack<Token> &tokens){
	bool t = raisedToExpr(tokens);
	if (!t)
	{
		return false;
	}
	Token tok = tokens.next(); 
	while (tok == MULT || tok == DIV)
	{
		bool isValidTerm = raisedToExpr(tokens);
		if (!isValidTerm)
		{
			return false;
		}
		tok = tokens.next();
	}
	tokens.go_back(); 
	return true; 
}
/**
 * @brief Parses a mathematical expression. mathExpr itself only handles 
 * addition and subtraction, but calls multAndDivExpr & RaisedToExper for any multiplication or division. 
 * A mathExpr is approximately defined as:
 * 
 * expr		-> <TERM> <join> <expr> | <TERM>
 * join		-> "+" | "-" | "*" | "/" | "^"
 * term is denfined in the function of the same name -> "bool term(Stack<Token> tokens)"
 * @param tokens 
 * @return true - if it is a valid statement,
 * @return false otherwise
 */
bool mathExpr(Stack<Token> &tokens){
	bool isValidTerm; 
	isValidTerm = multAndDivExpr(tokens);
	if (!isValidTerm)
	{
		return false;
	}
	Token t = tokens.next();
	while (t == PLUS || t == MINUS)
	{
		isValidTerm = multAndDivExpr(tokens);
		if (!isValidTerm)
		{
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
bool declare(Stack<Token> &tokens)
{
	tokens.go_back(1);
	Token t = tokens.next();
	KeyToken type = t.token; // int, char, bool, etc.
	if (tokens.peek() == POINTER)
		t = tokens.next();
	t = tokens.next();
	string id = t.lex; // the variable name
	// cout <<currentScope->hasVariable(t.lex) <<endl;
	if (t.token != IDENT)
	{
		cout << "Error: Not an identifier: " << t.lex << endl;
		tokens.go_back(2);
		return false;
	}
	else if (currentScope->hasVariable(t.lex))
	{
		cout << "Variable redclaration" << endl;
		tokens.go_back(2);
		return false;
	}
	// cout << "Adding variable: " << t;
	// cout << currentScope->getCascadingVars()<<endl<<endl;
	if (tokens.peek() == SEMICOL)
	{
		tokens.next();
		currentScope->addVariable(type, t.lex);
		return true;
	}
	else if (tokens.next() == EQUALS)
	{
		bool b = false;
		if (type == BOOL)
			b = logicStmt(tokens);
		else
			b = mathExpr(tokens);
		// tokens.go_back();
		// cout <<tokens.peek();
		if (!b)
		{
			// tokens.go_back();
			// cout << tokens.peek();
			if (tokens.next().lex == id)
				cout << "Cannot use variable in its own declaration statement" << endl;
			return false;
		}
	}
	// cout << t.lex <<endl ;
	currentScope->addVariable(type, id);
	t = tokens.next();
	// variables[t] = type;
	// put a way to interpret statements here
	if (t == SEMICOL)
		return true;
	tokens.go_back(2);
	return false;
}

bool obj(Stack<Token> &tokens)
{
	if (tokens.next().token != IDENT)
	{
		return false;
	}
	return true;
}

bool functionParamList(Stack<Token> &tokens)
{
	Token t = tokens.next();
	;
	if (t == RPAREN)
		return true;
	if (t.token <= INT)
	{
		tokens.go_back(1);
		return false;
	}
	t = tokens.next();
	if (t != IDENT)
	{
		tokens.go_back(1);
		return false;
	}
	t = tokens.next();
	if (t == COMMA)
	{
		return functionParamList(tokens);
	}
	return t == RPAREN;
}

bool func(Stack<Token> &tokens)
{
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
bool declareOrFunction(Stack<Token> &tokens)
{
	Token t = tokens.next();
	if (tokens.peek() == POINTER)
		t = tokens.next();
	if (t.token >= CONST && t.token <= PROTECTED)
	{
		// Operators.push_back(t) //Add this to the variable modifier memory
		return declareOrFunction(tokens);
	} //*/
	if (t != IDENT && t.token < INT)
	{
		tokens.go_back(1);
		return false;
	}
	if (t != IDENT)
	{
		t = tokens.next();
	}
	if (t.token == IDENT)
	{
		t = tokens.next();
		if (t.token == LPAREN)
		{ // It's a function
			return func(tokens);
			// return function(tokens); //TBI
		}
		else if (t == EQUALS || t == SEMICOL)
		{ // It's a variable
			tokens.go_back(2);
			return declare(tokens);
		}
	}
	tokens.go_back(2);
	return false;
}

bool construct(Stack<Token> &tokens)
{
	Token t = tokens.next();
	if (t != LPAREN)
	{
		tokens.go_back(1);
		return false;
	}
	t = tokens.next();
	if (t != RPAREN)
	{
		tokens.go_back(1);
		return false;
	}
	return true;
	// Need to handle closing curly bracket
}

bool destruct(Stack<Token> &tokens)
{
	Token t = tokens.next();
	if (t != LPAREN)
	{
		tokens.go_back(1);
		return false;
	}
	t = tokens.next();
	if (t != RPAREN)
	{
		tokens.go_back(1);
		return false;
	}
	return true;
	// Need to handle closing curly bracket
}
/**
 * @brief Described in short using EBNF, generic statements consist of: \n
 * generic	-> <inc/dec> | <assign>
 * inc/dec	-> <variable>["++"|"--"]";" | [++|--]<variable>";"
 * assign	-> <variable> "=" <someValue>;
 * @param tokens
 * @return true
 * @return false
 */
bool genericStmt(Stack<Token> &tokens)
{
	Token t = tokens.next();
	if (t == INCREMENT || t == DECREMENT)
	{
		Token t2 = tokens.next();
		// cout << t2;
		bool b = isValidVariable(t2);
		if (!b)
		{
			tokens.go_back(1);
			cout << "Unidentified token: " << t2.lex << endl;
		}
		return b;
	}
	if (!isValidVariable(t))
	{
		return false;
	}
	t = tokens.next();
	if (t == INCREMENT || t == DECREMENT)
		return true;
	if (t == EQUALS)
	{
		return mathExpr(tokens);
	}
	return false;
}

bool forStmt(Stack<Token> &tokens)
{
	Token t = tokens.next();

	bool b = declare(tokens);

	b = logicStmt(tokens);
	if (tokens.next() != SEMICOL)
	{
		tokens.go_back(1);
		return false;
	}
	b = mathExpr(tokens); // TBI: variable checking
	if (tokens.peek() == SEMICOL)
		tokens.next();
	return b;
}

bool ifStmt(Stack<Token> &tokens)
{
	bool b = logicStmt(tokens);
	return b;
}
bool caseStmt(Stack<Token> &tokens)
{
	Token variable = tokens.next();
	if (variable.token != SCONST && variable.token != NUMCONST && variable.token != TRU && variable.token != FALS)
		return false;
	return true; // Leaving this for now because I have to figure the specifics out later...
}

bool caseSwitchStmt(Stack<Token> &tokens)
{
	Token variable = tokens.next();
	if (variable != IDENT)
	{
		cout << "Unexpected token: '" << variable.lex << "' - expected a variable." << endl;
		tokens.go_back();
		return false;
	}
	else if (!isValidVariable(variable))
	{
		cout << "Unknown variable name: '" << variable.lex << "'" << endl;
		tokens.go_back();
		return false;
	}
	return true;
}
// /*
void printAllScopes()
{
	for (int i = 0; i < scopes.size(); i++)
	{
		cout << "Number " << i << ": " << scopes[i].getCascadingVars() << endl;
	}
	cout << "End of scopes" << endl
		<< endl;
} //*/

/**
 * @brief Get the next Valid Stmt in the code file provided.
 *
 * @param tokens
 * @return true
 * @return false
 */
bool getValidStmt(Stack<Token> &tokens)
{
	// Reminder: Vector of scopes has to keep moving whenever the list gets extended
	// so the pointers are no longer usable; occasionally everything gets corrupted
	if (currentScope == 0)
	{
		addScope();
	}
	bool status = false;
	Token t = tokens.next();
	// cout << t;
	switch (t.token)
	{
	case IMPORT:
		return import(tokens);
	case FOR:
		return forStmt(tokens);
	case IF:
		return ifStmt(tokens);
	case SWITCH:
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
		return genericStmt(tokens);
	case SEMICOL:
		return true;
	case OPENCURL:
	{
		return addScope();
	}
	case CLOSECURL:
		if (currentScope->getParentPointer() == 0)
		{
			cout << "unbalanced brackets" << endl;
			tokens.go_back(1);
			return false;
		}
		currentScope = currentScope->getParentPointer();
		return true;
	case INT:
	case SHORT: // int i = 0; << We start at the int token, then have to move to the ident, so we go back 1 to undo that
	case POINTER:
	case LONG:
	case FLOAT:
	case DOUBLE:
	case STRING:
	case CHAR:
	case BOOL:
	case BYTE:
		tokens.go_back(1);
	case PUBLIC:
	case PRIVATE:
	case PROTECTED:
	case SINGULAR:
	case CONST:
		return declareOrFunction(tokens);
	default:
	{
		cout << "ERROR: Unknown token: \n"
			<< t << endl;
		tokens.go_back(1);
		return false;
	}
	}
	cout << "Returning false by error?" << endl;
	return false;
}

Stack<Token> loadTokens(string fileDir)
{
	vector<Token> tokens;

	ifstream file;
	file.open(fileDir);
	if (!file)
	{
		cout << "File '" << fileDir << "' does not exist" << endl;
		Stack<Token> s;
		return s;
	}
	int ln = 1;
	while (!file.eof())
	{
		Token t = getNextToken(file, ln);
		t.ln = ln;
		tokens.push_back(t);
	}
	Stack<Token> s(tokens);
	return s;
}

bool analyzeFile(string fileDir)
{
	bool b;
	Stack<Token> s = loadTokens(fileDir);
	while (!s.eof() && (b = getValidStmt(s)))
	{
		// if(s.eof() == false)cout << s.eof();
	}
	if (!b || !currentScope->hasParent())
	{
		// if(currentScope->getParentPointer() != 0) cout << currentScope->getCascadingVars() <<endl;
		Token err = s.peek(); // cout << openbrackets <<endl ;
		cout << "Syntax error located at token '" << err.lex << "' on line " << err.ln << " in file: " << fileDir << "\n";
		Token e2;
		while (err.ln != -1 && (e2 = s.next()).ln == err.ln && !s.eof())
		{
			cout << e2.lex << " ";
		}
		cout << endl;
		return false;
	}
	return true;
}