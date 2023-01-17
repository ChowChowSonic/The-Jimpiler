template <typename T>
class SLL{
	private:
	template <typename Q>
	class node{
		public:
		Q data; 
		node* next; 
		node(){
			data = Q(); 
		}
		node(Q _item){
			data = _item; 
			next = nullptr; 
		};

		~node(){
			if(next != nullptr)
				delete next; 
		}
	};
	node<T>* head;
	int len; 
	public:
	SLL(){
		head = nullptr; 
		len = 0;
	} 

	~SLL(){
		delete head; 
	}

	void emplace_back(T & item){
		node<T> *tmp = new node<T>(item); 
		tmp->next = head; 
		head = tmp; 
		len++; 
	}

	T* getHead(){
		return &head->data;
	}
	int size(){
		return len; 
	}
}; 