#ifndef tokenizer
#define tokenizer
#include <map>
#include <string> 
#include <iostream>
#include <iomanip>
//Tokenizer of the compilier
using namespace std; 

enum KeyToken{
    IDENT, ERR, IN, AND, OR, TRU, FALS, NOT, IMPORT, //Done
    EQUALCMP, EQUALS, NOTEQUAL, GREATER, GREATEREQUALS, LESS, LESSEQUALS, INSERTION, REMOVAL, INCREMENT, DECREMENT, //Done
    OPENCURL, CLOSECURL, OPENSQUARE, CLOSESQUARE, LPAREN, RPAREN, COMMA, //Done
    PLUS, MINUS, MULT, DIV, LEFTOVER, POWERTO, POINTERTO, REFRENCETO, AS, SIZEOF, HEAP, DEL, //Done
    IF, ELSE, FOR, DO, WHILE, CASE, SWITCH, BREAK, CONTINUE, DEFAULT, RET, PRINT, PRINTLN, ASSERT, TRY, CATCH, ASSEMBLY,//Done
    OBJECT, CONSTRUCTOR, DESTRUCTOR, SEMICOL, COLON, PERIOD,//Done
    CONST, SINGULAR, VOLATILE, PUBLIC, PRIVATE, PROTECTED, OPERATOR, THROW, THROWS,//Done
    SCONST, NUMCONST, //Done
    INT, SHORT, LONG, POINTER, FLOAT, DOUBLE, STRING, BOOL, CHAR, BYTE, VOID, AUTO //Done 
};

map<string, KeyToken> keywords = {
    {"", IDENT}, {"in", IN}, {"and",AND}, {"or",OR}, {"true", TRU}, {"false", FALS}, {"not", NOT},{"import", IMPORT},
    {"if", IF}, {"else", ELSE}, {"for", FOR}, {"do", DO},{"while", WHILE}, {"case", CASE}, {"switch", SWITCH}, {"break", BREAK}, {"continue", CONTINUE}, {"default", DEFAULT}, {"return", RET}, {"print",PRINT}, {"println", PRINTLN}, {"assert", ASSERT}, {"try", TRY}, {"catch", CATCH}, 
    {"object", OBJECT}, {"constructor", CONSTRUCTOR}, {"destructor", DESTRUCTOR}, {"as", AS}, {"sizeof", SIZEOF}, {"heap", HEAP}, {"delete", DEL}, 
    {"const", CONST}, {"singular", SINGULAR}, {"volatile", VOLATILE}, {"public", PUBLIC}, {"private", PRIVATE}, {"protected", PROTECTED}, {"operator", OPERATOR}, {"throw", THROW}, {"throws", THROWS}, {"assembly", ASSEMBLY},
    {"int", INT}, {"short", SHORT}, {"long", LONG}, {"pointer", POINTER}, {"ptr", POINTER}, {"float", FLOAT}, {"double", DOUBLE}, {"string", STRING}, {"bool", BOOL}, {"char", CHAR}, {"byte", BYTE}, {"void", VOID}, {"auto", AUTO}
};

string keytokens[] {
    "IDENT","ERR","IN","AND","OR","TRU","FALS", "NOT", "IMPORT",
    "EQUALCMP","EQUALS", "NOTEQUAL", "GREATER", "GREATEREQUALS", "LESS", "LESSEQUALS", "INSERTION","REMOVAL", "INCREMENT", "DECREMENT",
    "OPENCURL","CLOSECURL", "OPENSQUARE", "CLOSESQUARE", "LPAREN","RPAREN", "COMMA",
    "PLUS","MINUS","MULT","DIV","LEFTOVER","POWERTO","POINTERTO","REFRENCETO", "AS", "SIZEOF", "HEAP", "DELETE", 
    "IF","ELSE","FOR","DO","WHILE","CASE","SWITCH", "BREAK", "CONTINUE", "DEFAULT", "RET", "PRINT", "PRINTLN", "ASSERT", "TRY", "CATCH", 
    "OBJECT","CONSTRUCTOR","DESTRUCTOR","SEMICOL", "COLON", "PERIOD",
    "CONST","SINGULAR", "VOLATILE", "PUBLIC","PRIVATE","PROTECTED","OPERATOR", "THROW", "THROWS", 
    "SCONST", "NUMCONST",
    "INT","SHORT","LONG","POINTER","FLOAT","DOUBLE","STRING","BOOL","CHAR","BYTE","VOID", "AUTO"
}; 
enum LexState{START, INNUM, INSTRING, INCOMMENT, INLINECOMMENT, INIDENT};

class Token{
    public:
    KeyToken token = ERR;
    string lex; 
    int ln; 
    Token(){
        token = ERR; 
        ln = -1; 
    }
    Token(KeyToken k, string lexeme, int line){
        token = k; 
        lex = lexeme; 
        line = line; 
    }

    string toString(){
        string st; 
        st+=keytokens[token]; 
        st+=" '";
        st+=lex; 
        st+="' Line:";
        st+=to_string(ln); 
        return st;
    }

    friend ostream& operator<<(ostream& s, Token t){
        string q; 
        q+= keytokens[t.token]; 
        if(t.token == IDENT) q+="("+t.lex+")";
        cout <<std::setw(10)<<q;
        cout << "\tline: " << t.ln <<endl;
        return s; 
    }
    bool operator==(Token t){
        return (token == t.token); 
    }
    bool operator!=(Token t){
        return (token != t.token); 
    }
    bool operator==(KeyToken t){
        return (token == t); 
    }
    bool operator!=(KeyToken t){
        return (token != t); 
    }
};

bool iequals(const string& a, const string& b){
    unsigned int sz = a.size();
    if (b.size() != sz) return false;
    for (unsigned int i = 0; i < sz; ++i)
        if (tolower(a[i]) != tolower(b[i]))
            return false;
    return true;
}
string toUpper(string s){
    for(int i = 0; i < s.size(); i++){
        if(isalpha(s[i]) && !isupper(s[i])) s[i] = toupper(s[i]); 
    }
    return s; 
}
bool isValidInt(char ch){
    return (isdigit(ch) || ch == '.');
}
bool isValidIdent(char ch){
    return (isalnum(ch) || ch == '_') && ch != ';' && ch != '.';
}
/**
 * @brief Parses inline assembly bounded by a opening and closing curly brace
 * 
 *
 */
Token parseAsm(istream &s, int &line){
    int startingline = line, bracecount = 1; 
    std::string assembly = "";
    char currentchar; 
    while(currentchar != '{'){
        (s >> currentchar); 
    }
    do{
        s >> currentchar; 
        if(currentchar == '{') bracecount++; 
        else if(currentchar == '}') bracecount--; 
        if(currentchar == '\n') line++; 
        if(bracecount > 0 )
        assembly+= currentchar; 
    }while(bracecount > 0); 
    return Token(ASSEMBLY, assembly, startingline); 
}
/**
 * @brief Get the Next Token object in the stringstream provided. Also takes a 'line' variable to keep track of possible errors.
 * The line Variable automatically increments as the tokenizer reads off of the StringStream. 
 * 
 * @param s - The refrence to the stringtream to read off of. 
 * @param line - The refrence to the line counter. Represents the current line we're on. This automatically increments as it goes along, no need to track it manually.
 * @return Token - The next token in the stream, separated by whitespace. 
 */
Token getNextToken(istream & s, int & line){
    LexState state = START;
    char ch = 0; 
    string lexeme; 
    while(s.get(ch)){
        char nextchar = s.peek(); 
        if(ch == '\n') line++; 
        switch(state){
            case START:{
            bool isswapped = false; 
            if(isspace(ch)){
                continue;  
                }
            if(ch == '@') return Token(REFRENCETO, "@", line); 
            else if(ch == ';') return Token(SEMICOL, ";", line); 
            else if (ch == '{') return Token(OPENCURL, "{", line); 
            else if (ch == '}') return Token(CLOSECURL, "}", line); 
            else if (ch == '[') return Token(OPENSQUARE, "[", line); 
            else if (ch == ']') return Token(CLOSESQUARE, "]", line); 
            else if (ch == ')') return Token(RPAREN, ")", line); 
            else if (ch == '(') return Token(LPAREN, "(", line); 
            else if (ch == '*') return Token(MULT, "*", line); 
            else if (ch == '.') return Token(PERIOD, ".", line); 
            else if (ch == '^') return Token(POWERTO, "^", line); 
            else if (ch == '%') return Token(LEFTOVER, "%", line); 
            else if (ch == ',') return Token(COMMA, ",", line); 
            else if (ch == ':') return Token(COLON, ":", line); 
            else if (ch == '!') {
                if(nextchar == '=') {s.get(); return Token(NOTEQUAL, "!=", line); }
                return Token(NOT, "!", line);
            }else if (ch == '+' && nextchar == '+') {
                s.get(); 
                return Token(INCREMENT, "++", line); }
            else if (ch == '+') return Token(PLUS, "+", line); 
            //Removal operator vs. Less Than
            else if (ch == '<' && nextchar == '<') {
                s.get();
                return Token(INSERTION, "<<", line);
            }else if(ch == '<' && nextchar == '='){
                s.get();
                return Token(LESSEQUALS, "<=", line); 
            }else if (ch == '<') return Token(LESS, "<", line); 
            //Insertion operator vs. Greater than; 
            else if(ch == '>' && nextchar == '>'){
                s.get(); 
                return Token(REMOVAL, ">>", line); 
            }else if(ch == '>' && nextchar == '='){
                s.get();
                return Token(GREATEREQUALS, ">=", line); 
            }else if (ch == '>') return Token(GREATER, ">", line);
            //Inline comment vs. Division
            else if (ch == '/' && nextchar == '/') {
                state = INLINECOMMENT; 
                continue; 
            }else if (ch == '/') return Token(DIV, "/", line); 
            //PointerTo vs Minus
            else if (ch == '-' && nextchar == '>') {
                s.get();
                return Token(POINTERTO, "->", line); 
            }else if (ch == '-' && nextchar == '-') {
                s.get();
                return Token(DECREMENT, "--", line); 
            }else if (ch == '-') return Token(MINUS, "-", line); 
            //Equals compare vs Equals assign; 
            else if (ch == '=' && nextchar == '=') {
                s.get(); 
                return Token(EQUALCMP, "==", line);
            }else if (ch == '=')  return Token(EQUALS, "=", line);  
            //multi line and/or documentation comment
            else if (ch == '?' && nextchar == '?') {
                state = INCOMMENT; 
                continue;
            }else if (isalpha(ch)) {
                if(isValidIdent(nextchar)){
                state = INIDENT; 
                }else {
                    lexeme = ch; 
                    return Token(IDENT, lexeme, line); }
                }
            else if (isdigit(ch)) {
                state = INNUM; 
                lexeme = ch; 
                if(!isValidInt(nextchar)) return Token(NUMCONST, lexeme, line); 
                continue;
            }else if (ch == '"' || ch == '\'') {
                state = INSTRING; 
                lexeme="";
                continue;
            }
            if(!isswapped)lexeme+=ch;
            break;
            }
            case INIDENT:{
            bool isvalidchar = isValidIdent(ch);// && isValidIdent(nextchar);
            bool isnextvalid = isValidIdent(nextchar);
            if(isvalidchar && !isspace(ch))lexeme+=ch; 
            if(!isnextvalid) { 
                Token t(keywords[(lexeme)], lexeme, line);
                if(t.token == ASSEMBLY) return parseAsm(s, line); 
                return t; 
            } 
            break;
            }
            case INNUM:{
            if(isValidInt(ch))lexeme+=ch; 
            if(!isValidInt(nextchar)) return Token(NUMCONST, lexeme, line); 
            break;
            }
            case INSTRING:
            {if(ch == '"' || ch == '\''){
                return Token(SCONST, lexeme, line);
            }
            lexeme+=ch; 
            break;
            }
            case INCOMMENT:
            if(nextchar == '?' && ch == '?') {
                s.get(ch); 
                state = START; 
            }
            break; 
            case INLINECOMMENT:
            if(ch == '\n') state = START; 
        }
    }
    return Token(ERR, string("ERROR"), -1);
}
#endif