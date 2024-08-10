# How to use the Jimbo Programming Language (pre-alpha/early development build)
0) Install the required packages (if not already installed)
```
sudo apt update
sudo apt upgrade
sudo apt install llvm g++
```
1) Compile the Jimbo compiler. 

This is done thorugh the shell file `jimpilier.sh`. Simply run
```
./jimpilier.sh main.cpp
```
This will output a new executable file called `jmb`. In the current iteration, this file dumps LLVM-IR to stderr. 
This setup will remain until I have the foundations & syntax of the language complete; once that part is done, I'll begin working on the LLVM passes to optimize what it puts out. After that, i'll work on having this compile directly into asm

2) Run the Jimbo compiler

I have included is a test file, `test.txt` that demonstrates some of the features included in the Jimbo programming language. Feel free to test off of this, or create new files to run them in. 
Here is the shell command that I use to test any code I write:
```
./jmb test.txt &> jmb.ll ; lli jmb.ll
```
I plan to have Jimbo files use a `.jmb` file extension, but this is currently low on my priority list. 
# Special features of the Jimbo Programming Language
## Inline debug printing
This is a fairly straightforward operator -  
This prints the value & line of whatever is passed to it...
then returns the value of what it just printed
```
int temp = 5!
temp!
println temp
> Debug value (Line 1): 5
> Debug value (Line 2): 5
> 5
```
## Switch-AutoBreak statements 
Once again, this is fairly straightforward, it makes the following two blocks of code equivalent:
```
switch auto break variable{
	case 0{
		continue
	}
	case 1, 2{
		continue
	}
	default{
		println "default reached:", variable
		continue
	}
	case 3{
	}
	case 4{
		throw exception
	}
}
```
```
switch variable{
	case 0{
	}
	case 1, 2{
	}
	default{
		println "default reached:", variable
	}
	case 3{
		break
	}
	case 4{
		throw exception; 
	}
}
```
## Case-Range statements
This is a convenience notation, meant to handle large amounts of constant consecutive integers without having to use if statements. 

```
switch something {
	case 5 ... 10, 20 ... 30{
		println "This will print when (something >=5 && something < 10) || (something >= 20 && something < 30)"
	}
    case 0 ... 5:2{
		println "This will print when something is 0, 2, or 4"
	}
}
```
10 & 30 in the first case (& 5 in the second case) are exclusive. Don't worry about overlapping values as long as they're all part of the same case stmt.
Spaces around the range operator (the '...' is the range operator) is also important so the compiler doesn't think you're working with floats
## Range statements
When all the values you use are constants, the range operator creates a global constant array, and returns a pointer to it
```
(5 ... 1)! 
(1 ... 5)!
(5 ... 1 : 2)! 
(5 ... 1 : -2)!
(1 ... 5 : 2)!
(1 ... 5 : -2)!
(1.0 ... 5.0 : 1.5)!
> Debug value (Line 1): [5,4,3,2]
> Debug value (Line 2): [1,2,3,4]
> Debug value (Line 3): [5,3]
> Debug value (Line 4): [5,3]
> Debug value (Line 5): [1,3]
> Debug value (Line 6): [1,3]
> Debug value (Line 7): [1.0,2.5,4.0]
```
When you use non-constant values, it allocates space on the stack for the array, populates it via loop, then returns a pointer (Implementation to be changed soon, I just want to get a few more dependendant features working first)
```
int x = 5;
(1 ... x+1)!
> Debug value (Line 1): [1,2,3,4,5]
```

## N-way equivalency relations (EXPERIMENTAL)
```
if (x == y == z == 5 > 4) doSomething() //y,z,5 get evaluated twice; functions/operators will only be called once
//alternatively...
if (x == y && y == z && z == 5 && 5 > 4) doSomething() // y,z,5 evaluated twice, but functions will be called twice if not saved to a variable
```
## Implicit Main (EXPERIMENTAL)
I hope to have the language not require a main method, instead placing all code into a 'static_main' method that will never allocate variables on the stack. Once compilation is done, the compiler will check if a main method exists - if it doesn't, it renames the static method to 'main'. <br>
Any variables allocated in the implicit main method will be promoted to a global variable; this will ultimately hit performance if you go for the implicit main route, but is still very good for rapid prototyping. 
## Check out my progress
Right now my main priority is getting the language off the ground (When i'm not in class); `TypeExpr.cpp` contains the type & Variable objects, `globals.cpp` contains, well, globals; `ExprAST.cpp` contains the IR generating abstract syntax tree (& related) objects, while `jimpilier.h` contains the parsing code. 
I have various TODO markers spread out throughout `jimpilier.h` and `ExprAST.cpp` reminding me that I still have a lot of progress to make. Feel free to take a look through and see my progress! 