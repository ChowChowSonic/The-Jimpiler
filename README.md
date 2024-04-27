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

## Check out my progress
Right now my main priority is getting the language off the ground (When i'm not in class); `TypeExpr.cpp` contains the type & Variable objects, `globals.cpp` contains, well, globals; `ExprAST.cpp` contains the IR generating abstract syntax tree (& related) objects, while `jimpilier.h` contains the parsing code. 
I have various TODO markers spread out throughout `jimpilier.h` and `ExprAST.cpp` reminding me that I still have a lot of progress to make. Feel free to take a look through and see my progress! 