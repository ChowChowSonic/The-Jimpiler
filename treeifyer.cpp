#include <vector>
#include "tokenizer.cpp"
using namespace std; 
//Syntax analyzer for the Compilier. 
//I'll work on this later
//map<KeyToken, Token> variables; 
class Node{
    private:
    Node *Left;
    Node *Right;  
    public:
    Node(Node Lchild, Node Rchild){
        Left = &Lchild; 
        Right = &Rchild; 
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
void make_tree(vector<Token> tokens){
    Stream<Token> stream(tokens); 
    //for token in tokens,
    // if token == parenthesis/bracket/divider
        //go up a level, and recursively restart the process for that bracket.
    //This should return a tree of code brackets that can be soon analyzed within their respective context
    //also for each token, try to create an array/table of each data type, and fill them out as you go
}

void analyze_level(){
    //recursive; 
}
void identify_tokens(){

}