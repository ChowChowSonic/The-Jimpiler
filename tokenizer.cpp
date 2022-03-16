//Tokenizer of the compilier
#include <stdio.h>
#include <map>
#include <string>
#include <istream>
using namespace std; 

enum KeyToken{
    IDENT, ERR, IN, AND, OR, TRU, FALS, NOT, IMPORT, //Done
    EQUALCMP, EQUALS, GREATER, LESS, INSERTION, REMOVAL, //Done
    OPENCURL, CLOSECURL, LPAREN, RPAREN, COMMA, //Done
    PLUS, MINUS, MULT, DIV, LEFTOVER, POWERTO, POINTERTO, REFRENCETO, //Done
    IF, ELSE, FOR, WHILE, CASE, SWITCH, //Done
    OBJECT, CONSTRUCTOR, DESTRUCTOR, SEMICOL, //Done
    CONST, SINGULAR, PUBLIC, PRIVATE, PROTECTED, //Done
    SCONST, NUMCONST, //Done
    INT, SHORT, LONG, POINTER, FLOAT, DOUBLE, STRING, BOOL, CHAR, BYTE, //Done 
};

map<string, KeyToken> keywords {
    {"", IDENT}, {"IN", IN}, {"AND",AND}, {"OR",OR}, {"TRUE", TRU}, {"FALSE", FALS}, {"IMPORT", IMPORT},
    {"IF", IF}, {"ELSE", ELSE}, {"FOR", FOR}, {"WHILE", WHILE}, {"CASE", CASE}, {"SWITCH", SWITCH},
    {"OBJECT", OBJECT}, {"CONSTRUCTOR", CONSTRUCTOR}, {"DESTRUCTOR", DESTRUCTOR},
    {"CONST", CONST}, {"SINGULAR", SINGULAR}, {"PUBLIC", PUBLIC}, {"PRIVATE", PRIVATE}, {"PROTECTED", PROTECTED},
    {"INT", INT}, {"SHORT", SHORT}, {"LONG", LONG}, {"POINTER", POINTER}, {"FLOAT", FLOAT}, {"DOUBLE", DOUBLE}, {"STRING", STRING}, {"BOOL", BOOL}, {"CHAR", CHAR}, {"BYTE", BYTE},
    };

string keytokens[] {
    "IDENT","ERR","IN","AND","OR","TRU","FALS", "NOT", "IMPORT",
    "EQUALCMP","EQUALS","GREATER", "LESS", "INSERTION","REMOVAL",
    "OPENCURL","CLOSECURL","LPAREN","RPAREN", "COMMA",
    "PLUS","MINUS","MULT","DIV","LEFTOVER","POWERTO","POINTERTO","REFRENCETO",
    "IF","ELSE","FOR","WHILE","CASE","SWITCH",
    "OBJECT","CONSTRUCTOR","DESTRUCTOR","SEMICOL",
    "CONST","SINGULAR","PUBLIC","PRIVATE","PROTECTED",
    "SCONST", "NUMCONST",
    "INT","SHORT","LONG","POINTER","FLOAT","DOUBLE","STRING","BOOL","CHAR","BYTE"
}; 
enum LexState{START, INNUM, INSTRING, INCOMMENT, INLINECOMMENT, INIDENT};

class Token{
    public:
    KeyToken token;
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
};

bool iequals(const string& a, const string& b)
{
    unsigned int sz = a.size();
    if (b.size() != sz)
        return false;
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
    return (isalnum(ch) || ch == '_') && (ch != ';');
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
            else if (ch == ')') return Token(RPAREN, ")", line); 
            else if (ch == '(') return Token(LPAREN, "(", line); 
            else if (ch == '+') return Token(PLUS, "+", line); 
            else if (ch == '*') return Token(MULT, "*", line); 
            else if (ch == '^') return Token(POWERTO, "^", line); 
            else if (ch == '%') return Token(LEFTOVER, "%", line); 
            else if (ch == '!') return Token(NOT, "!", line); 
            else if (ch == ',') return Token(COMMA, ",", line); 
            //Removal operator vs. Less Than
            else if (ch == '<' && nextchar == '<') {
                s.get();
                return Token(INSERTION, "<<", line);
            }else if (ch == '<') return Token(LESS, "<", line); 
            //Insertion operator vs. Greater than; 
            else if(ch == '>' && nextchar == '>'){
                s.get(); 
                return Token(REMOVAL, ">>", line); 
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
                if(isalpha(nextchar)){
                state = INIDENT; 
                }else {
                    lexeme = ch; 
                    return Token(IDENT, lexeme, line); }
                }
            else if (isdigit(ch)) {
                if(isdigit(nextchar)){
                state = INNUM; 
                }else {
                    lexeme = ch; 
                    return Token(NUMCONST,lexeme, line); }
            }else if (ch == '"') {
                state = INSTRING; 
            }
            if(!isswapped)lexeme+=ch;
            break;
            }
            case INIDENT:{
            bool isvalidchar = isValidIdent(ch) && isValidIdent(nextchar);
            if(!isspace(ch))lexeme+=ch; 
            if(!isvalidchar) return Token(keywords[toUpper(lexeme)], lexeme, line); 
            break;
            }
            case INNUM:{
            //cout << ch << nextchar; 
            bool isvalidchar = isValidInt(ch) && isValidInt(nextchar);
            if(isValidInt(ch))lexeme+=ch; 
            if(!isvalidchar) return Token(NUMCONST, lexeme, line); 
            break;
            }
            case INSTRING:
            {if(ch == '"'){
                lexeme+=ch; 
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
    return Token(ERR, string(""), -1);
}