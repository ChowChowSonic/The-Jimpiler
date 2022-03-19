#include <vector>

template <typename T>
class Stack{
    public:
    int index = 0, size = 0; 
    T * items; 
    Stack(vector<T> item){
        items = new T[item.size()]; 
        size = item.size(); 
        int x = 0;
        for(T i : item){
            items[x] = i; 
            x++;
        }
    };
    ~Stack(){
        delete[] items; 
        index = 0;
        size = 0; 
    }
    T next(){
        if(index >= size) index--; 
        T Q = items[index]; 
        index++; 
        return Q; 
    }
    /**
     * @brief If you've ever played Magic the Gathering, this is similar to the Scry mechanic, but only with one card, always putting it on top.
     * If you havent't played Magic the Gathering, you look at the object I items down, without removing it from the stream. 
     * Works in in O(1) time. 
     * 
     * @param i - How many item down to look
     * @return T - Either the last item in the list, or the item I down. Whichever is smaller
     */
    T scry(int i){
        if(index+i >= size) return items[size-1];
        return items[index+i];

    }
    /**
     * @brief If you've ever played Magic the Gathering, SCRY 1, always putting it on top.
     * If you havent't played Magic the Gathering, it is effectively Stream.peek(). 
     * Works in in O(1) time. 
     * 
     * @param i - How many item down to look
     * @return T - Either the last item in the list, or the item I down. Whichever is smaller
     */
    T scry(){
        if(index+1 >= size) return items[size-1];
        return items[index+1];

    }
    bool eof(){
        return (index >= size); 
    }
    void reset(){
        index = 0;
    }

};