
template <typename T>
/**
 * @brief A read-only version of the stack that allows the user to go back any amount. 
 * Intended for functions that require stack like operations, but either require 
 * optimization or the ability to go backwards in the stack multiple times over 
 * 
 */
class Stack{
    public:
    int index = 0, size = 0; 
    vector<T> items; 
    Stack(){
        // items = 0; 
    }
    Stack(vector<T> item){
        size = item.size();
        //cout << item.size() << " " <<size; 
        int x = 0;
        //I know there are better ways to do this, but I'm doing it this way for debugging purposes
        items = vector<T>(item.begin(), item.end()); 
        size = items.size(); 
        //cout <<endl; 
    };
    ~Stack(){
    }
    T next(){
        //cout << index << " " << items[index] <<endl; 
        if(index >= size) return Token(ERR, "", -1); 
        T Q = items[index]; 
        index++; 
        //if(index >= size) index--; 
        return Q; 
    }
    /**
     * @brief If you've ever played Magic the Gathering, this is similar to the Scry mechanic, but only with one card, always putting it on top.
     * If you havent't played Magic the Gathering, you look at the object I items down, without removing it from the stream. 
     * Works in in O(1) time. 
     * 
     * @param i - How many item down to look
     * @return T - Either the last item in the list, or the item I down. Whichever is closer
     */
    T scry(int i){
        if(index+i >= size) return items[size-1];
        return items[index+i];

    }
    /**
     * @brief If you've ever played Magic the Gathering, SCRY 1, always putting the card on top.
     * If you havent't played Magic the Gathering, it is effectively Stream.peek(). 
     * Works in in O(1) time. 
     * 
     * @return T - Either the last item in the list, or the item I down. Whichever is closer
     */
    T scry(){
        if(size == 0) return Token(ERR, "-1", -1);
        if(index+1 >= size) return items[size-1];
        return items[index];

    }
    T currentToken(){
        if (index == 0)return items[0]; 
        if (size == 0) return Token(ERR, "-1", -1);
        return items[index-1]; 
    }
    T peek(){
        if(size == 0 || index >= size) return Token(ERR, "-1", -1);
        return items[index]; 
    }
    bool eof(){
        //cout << items[index]<< ": " <<index << " - "<<  sizeof(items)/sizeof(items[0]) <<endl; 
        return !(index < size); 
    }
    void reset(){
        index = 0;
    }
    /**
     * @brief Adds "i" items back onto the stack. Items will be returned to the stack as if they were never removed to begin with, in the same, correct order. 
     * 
     * @param i - The number of items to add back onto the stack. Is set to 1 if no paramater is passed
     * @return void
     */
    void go_back(int i = 1){
        index -=i; 
        if(index < 0) index = 0; 
    }
};