#pragma once
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"
#ifndef exprAST
#define exprAST
#include "globals.cpp"
#include "TypeExpr.h"
using namespace std;
namespace jimpilier
{

	/// ExprAST - Base class for all expression nodes.
	class ExprAST
	{
	public:
		/**
		 * @brief A compile-time array of error types that can be thrown by this ExprAST; this list is only populated after a call to this->codegen()
		 */
		std::set<llvm::Type *> throwables;
		virtual ~ExprAST() {};
		virtual void replaceTemplate(std::string &name, std::unique_ptr<TypeExpr> *ty = NULL){}; 
		virtual llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL) = 0;
	};

	/// NumberExprAST - Expression class for numeric literals like "1.0".
	class NumberExprAST : public ExprAST
	{
		double Val;
		bool isInt = false;
		bool isBool = false;

	public:
		NumberExprAST(double Val) : Val(Val) {}
		NumberExprAST(int Val) : Val(Val) { isInt = true; }
		NumberExprAST(long Val) : Val(Val) { isInt = true; }
		NumberExprAST(bool Val) : Val(Val) { isBool = true; }
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class StringExprAST : public ExprAST
	{
		std::string Val;

	public:
		StringExprAST(std::string val) : Val(val) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	/// VariableExprAST - Expression class for referencing a variable, like "a".
	class VariableExprAST : public ExprAST
	{
		std::string Name;

	public:
		VariableExprAST(const std::string &Name) : Name(Name) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class DeclareExprAST : public ExprAST
	{
		const std::string name;
		std::unique_ptr<jimpilier::TypeExpr> type;
		int size;
		bool lateinit;

	public:
		DeclareExprAST(const std::string Name, std::unique_ptr<jimpilier::TypeExpr> type, bool isLateInit = false, int ArrSize = 1) : name(Name), type(std::move(type)), size(ArrSize), lateinit(isLateInit) {}

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class IncDecExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> val;
		bool prefix, decrement;

	public:
		IncDecExprAST(bool isPrefix, bool isDecrement, std::unique_ptr<ExprAST> &uniary) : prefix(isPrefix), decrement(isDecrement), val(std::move(uniary)) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class NotExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> val;

	public:
		NotExprAST(std::unique_ptr<ExprAST> Val) : val(std::move(Val)) {};
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class RefrenceExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> val;

	public:
		RefrenceExprAST(std::unique_ptr<ExprAST> &val) : val(std::move(val)) {};
		llvm::Value *codegen(bool autoDeref = false, llvm::Value *other = NULL);
	};

	class DeRefrenceExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> val;

	public:
		DeRefrenceExprAST(std::unique_ptr<ExprAST> &val) : val(std::move(val)) {};
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
	;};

	class IndexExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> bas, offs;

	public:
		IndexExprAST(std::unique_ptr<ExprAST> &base, std::unique_ptr<ExprAST> &offset) : bas(std::move(base)), offs(std::move(offset)) {};
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};
	class TypeCastExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> from;
		std::unique_ptr<TypeExpr> totype;

	public:
		TypeCastExprAST(std::unique_ptr<ExprAST> &fromval, std::unique_ptr<TypeExpr> &toVal) : from(std::move(fromval)), totype(std::move(toVal)) {};

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class AndOrStmtAST : public ExprAST
	{
		std::vector<std::unique_ptr<ExprAST>> items;
		std::vector<KeyToken> operations;
		bool sub;

	public:
		AndOrStmtAST(std::vector<std::unique_ptr<ExprAST>> &items, std::vector<KeyToken> &operations)
			: items(std::move(items)), operations(std::move(operations)) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class ComparisonStmtAST : public ExprAST
	{
		std::vector<std::vector<std::unique_ptr<ExprAST>>> items; // convert this to vector<vector<ExprAST>>
		std::vector<KeyToken> operations;
		bool sub;

	public:
		ComparisonStmtAST(std::vector<std::vector<std::unique_ptr<ExprAST>>> &items, std::vector<KeyToken> &operations)
			: items(std::move(items)), operations(std::move(operations)) {}
		// Create internal private function to retrieve items from array; Codegen them only once, cache the result and return that later
	private:
		std::map<int, std::map<int, llvm::Value *>> cache;
		llvm::Value *getCachedResult(const int argNo, const int subArgNo)
		{
			if (cache[argNo][subArgNo] == NULL)
			{
				cache[argNo][subArgNo] = items[argNo][subArgNo]->codegen();
			}
			return cache[argNo][subArgNo];
		}
	public:
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class IfExprAST : public ExprAST
	{
	public:
		std::vector<std::unique_ptr<ExprAST>> condition, body;
		IfExprAST(std::vector<std::unique_ptr<ExprAST>> cond, std::vector<std::unique_ptr<ExprAST>> bod) : condition(std::move(cond)), body(std::move(bod)) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class SwitchExprAST : public ExprAST
	{
	public:
		std::vector<std::pair<std::set<std::unique_ptr<ExprAST>>, std::unique_ptr<ExprAST>>> cases;
		std::unique_ptr<ExprAST> comp;
		bool autoBr;
		SwitchExprAST(std::unique_ptr<ExprAST> &comparator, std::vector<std::pair<std::set<std::unique_ptr<ExprAST>>, std::unique_ptr<ExprAST>>> &cases, bool autoBreak) : cases(std::move(cases)), comp(std::move(comparator)), autoBr(autoBreak) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	/**
	 * @brief represents a break statement with optional label.
	 * Simply creates a break to the most recent escapeblock, or returns a constantInt(0) if there is none
	 *
	 */
	class BreakExprAST : public ExprAST
	{

		std::string labelVal;

	public:
		BreakExprAST(std::string label = "") : labelVal(label) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};
	/**
	 * @brief represents a break statement with optional label.
	 * Simply creates a break to the most recent escapeblock, or returns a constantInt(0) if there is none
	 *
	 */
	class ContinueExprAST : public ExprAST
	{

		std::string labelVal;

	public:
		ContinueExprAST(std::string label = "") : labelVal(label) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		;
	};
	class SizeOfExprAST : public ExprAST
	{
		std::unique_ptr<TypeExpr> type = NULL;
		std::unique_ptr<ExprAST> target = NULL;

	public:
		SizeOfExprAST(std::unique_ptr<TypeExpr> &SizeOfTarget) : type(std::move(SizeOfTarget)) {}
		SizeOfExprAST(std::unique_ptr<ExprAST> &SizeOfTarget) : target(std::move(SizeOfTarget)) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};
	class HeapExprAST : public ExprAST
	{

	public:
		HeapExprAST() {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class DeleteExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> val;

	public:
		DeleteExprAST(std::unique_ptr<ExprAST> &deleteme) : val(std::move(deleteme)) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};
	/**
	 * @brief Represents a for loop in LLVM IR, currently does not support for each statements
	 *
	 */
	class ForExprAST : public ExprAST
	{
	public:
		std::vector<std::unique_ptr<ExprAST>> prefix, postfix;
		std::unique_ptr<ExprAST> condition, body;
		bool dowhile;

		ForExprAST(std::vector<std::unique_ptr<ExprAST>> &init,
				   std::unique_ptr<ExprAST> cond,
				   std::unique_ptr<ExprAST> bod,
				   std::vector<std::unique_ptr<ExprAST>> &edit, bool isdoWhile = false) : prefix(std::move(init)), condition(std::move(cond)), body(std::move(bod)), postfix(std::move(edit)), dowhile(isdoWhile) {}
		/**
		 * @brief Construct a ForExprAST, but with no bound variable or modifier; I.E. A while loop rather than a for loop
		 *
		 * @param cond
		 * @param bod
		 */
		ForExprAST(std::unique_ptr<ExprAST> cond, std::unique_ptr<ExprAST> bod, bool isDoWhile = false) : condition(std::move(cond)), body(std::move(bod)), dowhile(isDoWhile) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class RangeExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> start, end, step = NULL;

	public:
		RangeExprAST(std::unique_ptr<ExprAST> &st, std::unique_ptr<ExprAST> &fin) : start(std::move(st)), end(std::move(fin)) {}
		RangeExprAST(std::unique_ptr<ExprAST> &st, std::unique_ptr<ExprAST> &fin, std::unique_ptr<ExprAST> &step) : start(std::move(st)), end(std::move(fin)), step(std::move(step)) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	/**
	 * Represents a list of items in code, usually represented by a string such as "[1, 2, 3, 4, 5]" alongside an optional semicolon on the end
	 */
	class ListExprAST : public ExprAST
	{
	public:
		std::vector<std::unique_ptr<ExprAST>> Contents;
		ListExprAST(std::vector<std::unique_ptr<ExprAST>> &Args) : Contents(std::move(Args)) {}
		ListExprAST() {}

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL)
		;
	};

	class DebugPrintExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> val;
		int ln;

	public:
		DebugPrintExprAST(std::unique_ptr<ExprAST> printval, int line) : val(std::move(printval)), ln(line) {}

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	/**
	 * @brief Represents a return statement for a function
	 */
	class RetStmtAST : public ExprAST
	{
		std::unique_ptr<ExprAST> ret;

	public:
		RetStmtAST(std::unique_ptr<ExprAST> &val) : ret(std::move(val)) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class AssignStmtAST : public ExprAST
	{
		std::unique_ptr<ExprAST> lhs, rhs;

	public:
		AssignStmtAST(std::unique_ptr<ExprAST> &LHS, std::unique_ptr<ExprAST> &RHS)
			: lhs(std::move(LHS)), rhs(std::move(RHS))
		{}

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};
	/**
	 * Handles multiplication and division (* and /) statments
	 */
	class MultDivStmtAST : public ExprAST
	{
		std::unique_ptr<ExprAST> LHS, RHS;
		bool div;

	public:
		MultDivStmtAST(std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS, bool isDiv)
			: LHS(std::move(LHS)), RHS(std::move(RHS)), div(isDiv) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};
	/**
	 * Handles addition and subtraction (+ and -) statments
	 */
	class AddSubStmtAST : public ExprAST
	{
		std::unique_ptr<ExprAST> LHS, RHS;
		bool sub;

	public:
		AddSubStmtAST(std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS, bool isSub)
			: LHS(std::move(LHS)), RHS(std::move(RHS)), sub(isSub) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	/**
	 * Handles raised-to and modulo statements
	 */
	class PowModStmtAST : public ExprAST
	{
		std::unique_ptr<ExprAST> LHS, RHS;
		bool mod;

	public:
		PowModStmtAST(std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS, bool ismod)
			: LHS(std::move(LHS)), RHS(std::move(RHS)), mod(ismod) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};
	/** BinaryStmtAST - Expression class for a binary operator.
	 *
	 *
	 */
	class BinaryStmtAST : public ExprAST
	{
		std::string Op;
		std::unique_ptr<ExprAST> LHS, RHS;

	public:
		BinaryStmtAST(std::string op, /* llvm::Type* type,*/ std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
			: Op(op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class TryStmtAST : public ExprAST
	{
		std::unique_ptr<ExprAST> body;
		std::map<std::unique_ptr<TypeExpr>, std::pair<std::unique_ptr<ExprAST>, std::string>> catchStmts;

	public:
		TryStmtAST(std::unique_ptr<ExprAST> &body, std::map<std::unique_ptr<TypeExpr>, std::pair<std::unique_ptr<ExprAST>, std::string>> &catchStmts) : body(std::move(body)), catchStmts(std::move(catchStmts)) {};

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class ThrowStmtAST : public ExprAST
	{
		std::unique_ptr<ExprAST> ball;

	public:
		ThrowStmtAST(std::unique_ptr<ExprAST> &ball) : ball(std::move(ball)) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};
	/**
	   * Class representing a print statement in the Abstract Syntax Tree (AST)
	   * Inherits from ExprAST base class

	   */
	class PrintStmtAST : public ExprAST
	{
	public:
		std::vector<std::unique_ptr<ExprAST>> Contents;
		bool isLine = false;
		PrintStmtAST(std::vector<std::unique_ptr<ExprAST>> &Args, bool isLn)
			: Contents(std::move(Args)), isLine(isLn) {}

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class CodeBlockAST : public ExprAST
	{
	public:
		std::vector<std::unique_ptr<ExprAST>> Contents;
		CodeBlockAST(std::vector<std::unique_ptr<ExprAST>> &Args)
		{
			for (auto &x : Args)
			{
				Contents.push_back(std::move(x));
			}
		}

		CodeBlockAST() {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};
	class MemberAccessExprAST : public ExprAST
	{
		std::string member;
		std::unique_ptr<ExprAST> base;
		bool dereferenceParent;

	public:
		MemberAccessExprAST(std::unique_ptr<ExprAST> &base, std::string offset, bool deref = false) : base(std::move(base)), member(offset), dereferenceParent(deref) {}
		// TODO: FIX ME
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	/// CallExprAST - Expression class for function calls.
	class CallExprAST : public ExprAST
	{
		std::string Callee;
		std::vector<std::unique_ptr<ExprAST>> Args;

	public:
		CallExprAST(const std::string callee, std::vector<std::unique_ptr<ExprAST>> &Arg) : Callee(callee), Args(std::move(Arg))
		{
		}
		// : Callee(callee), Args(Arg) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	/**
	 * @brief ObjectFunctionCallExprAST - Expression class for function calls from objects. This acts just like regular function calls,
	 * but this adds an implicit pointer to the object refrencing the call, so it needs its own ExprAST to function properly
	 * @example vector<T> obj;
	 * obj.push_back(x); //"obj" is the object refrencing the call, so push_back() becomes: push_back(&obj, x)
	 */
	class ObjectFunctionCallExprAST : public ExprAST
	{
		std::string Callee;
		std::vector<std::unique_ptr<ExprAST>> Args;
		std::unique_ptr<ExprAST> parent;
		bool dereferenceParent;

	public:
		ObjectFunctionCallExprAST(const std::string callee, std::vector<std::unique_ptr<ExprAST>> &Arg, std::unique_ptr<ExprAST> &parent, bool deref) : dereferenceParent(deref), Callee(callee), Args(std::move(Arg)), parent(std::move(parent))
		{
		}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	/**
	 * @brief ObjectConstructorCallExprAST - Expression class for constructor calls from objects. This acts just like regular function calls,
	 * but this adds an implicit pointer to the object refrencing the call, so it needs its own ExprAST to function properly
	 * @example vector<T> obj;
	 * obj.push_back(x); //"obj" is the object refrencing the call, so push_back() becomes: push_back(&obj, x)
	 */
	class ObjectConstructorCallExprAST : public ExprAST
	{
		std::string Callee;
		std::vector<std::unique_ptr<ExprAST>> Args;
		std::unique_ptr<ExprAST> target = NULL;
		std::unique_ptr<TypeExpr> CalledTyConstructor;

	public:
		ObjectConstructorCallExprAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST>> &Arg) : Callee(callee), Args(std::move(Arg)) {}
		ObjectConstructorCallExprAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST>> &Arg, std::unique_ptr<ExprAST> &target) : target(std::move(target)), Callee(callee), Args(std::move(Arg)) {}
		ObjectConstructorCallExprAST(std::unique_ptr<TypeExpr> &callee, std::vector<std::unique_ptr<ExprAST>> &Arg) : CalledTyConstructor(std::move(callee)), Args(std::move(Arg)) {}
		ObjectConstructorCallExprAST(std::unique_ptr<TypeExpr> &callee, std::vector<std::unique_ptr<ExprAST>> &Arg, std::unique_ptr<ExprAST> &target) : target(std::move(target)), CalledTyConstructor(std::move(callee)), Args(std::move(Arg)) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	// ConstructorExprAST - A class representing the definition of an object constructor
	class ConstructorExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> bod;
		std::vector<Variable> argslist;
		std::string objName;

	public:
		ConstructorExprAST(
			std::vector<Variable> &argList,
			std::unique_ptr<ExprAST> &body,
			std::string objName) : argslist(std::move(argList)), bod(std::move(body)), objName(objName) {};
		void replaceTemplate(std::string &name, std::unique_ptr<TypeExpr> *ty = NULL){
			objName = name; 
		}; 
		llvm::Value *codegen(bool autoderef = false, llvm::Value *other = NULL); 
	};

	class ObjectHeaderExpr
	{
	public:
		std::string name;
		std::vector<std::unique_ptr<TypeExpr>> templates; 
		ObjectHeaderExpr(const std::string &nameval, std::vector<std::unique_ptr<TypeExpr>> &templatelist) : name(nameval), templates(std::move(templatelist)) {}
		llvm::StructType *codegen(bool autoderef = false, llvm::Value *other = NULL);
	};
	/**
	 * The class definition for an object (struct) in memory
	 */
	class ObjectExprAST : public ExprAST
	{
		ObjectHeaderExpr base;
		std::vector<Variable> vars;
		std::vector<std::unique_ptr<ExprAST>> functions, ops;
	public:
		ObjectExprAST(ObjectHeaderExpr &name, std::vector<Variable>&varArg,
					  std::vector<std::unique_ptr<ExprAST>> &funcList, std::vector<std::unique_ptr<ExprAST>> &oplist) : base(std::move(name)), vars(std::move(varArg)), functions(std::move(funcList)), ops(std::move(oplist)) {
					  };
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL); 
	};

	/// PrototypeAST - This class represents the "prototype" for a function,
	/// which captures its name, and its argument names (thus implicitly the number
	/// of arguments the function takes).
	class PrototypeAST
	{
	public:
		std::string Name, parent;
		std::unique_ptr<TypeExpr> retType;
		std::vector<Variable> Args;
		std::vector<std::unique_ptr<TypeExpr>> throwableTypes;

		PrototypeAST() {};

		PrototypeAST(const std::string &name,
					 std::vector<Variable> &args,
					 std::unique_ptr<TypeExpr> &ret, const std::string &parent = "")
			: Name(name), Args(std::move(args)), retType(std::move(ret)), parent(parent)
		{}
		PrototypeAST(const std::string &name,
					 std::vector<Variable> &args,
					 std::vector<std::unique_ptr<TypeExpr>> &throwables,
					 std::unique_ptr<TypeExpr> &ret, const std::string &parent = "")
			: Name(name), Args(std::move(args)), retType(std::move(ret)), parent(parent), throwableTypes(std::move(throwables))
		{}

		const std::string &getName() const { return Name; }
		std::vector<llvm::Type *> getArgTypes(); 
		llvm::Function *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};
	/// FunctionAST - This class represents a function definition, rather than a function call.
	class FunctionAST : public ExprAST
	{
		std::unique_ptr<PrototypeAST> Proto;
		std::unique_ptr<ExprAST> Body;

	public:
		FunctionAST(std::unique_ptr<PrototypeAST> Proto,
					std::unique_ptr<ExprAST> Body, std::string parentType = "")
			: Proto(std::move(Proto)), Body(std::move(Body)) {}
		void replaceTemplate(std::string &name, std::unique_ptr<TypeExpr> *ty = NULL){
			Proto->parent = name; 
		}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class OperatorOverloadAST : public ExprAST
	{
		PrototypeAST Proto;
		std::unique_ptr<ExprAST> Body;
		std::vector<Variable> args;
		std::unique_ptr<TypeExpr> retType;
		std::string name = "operator_", oper;

	public:
		OperatorOverloadAST(std::string &oper, std::unique_ptr<TypeExpr> &ret, std::vector<Variable> &args,
							std::unique_ptr<ExprAST> &Body)
			: oper(oper), Body(std::move(Body)), args(std::move(args)), retType(std::move(ret)) {}

		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class AsOperatorOverloadAST : public ExprAST
	{
		std::vector<Variable> arg1;
		std::unique_ptr<ExprAST> body;
		std::unique_ptr<TypeExpr> ty, ret;
		std::string name;

	public:
		AsOperatorOverloadAST(std::vector<Variable> &arg1, std::unique_ptr<TypeExpr> &castType, std::unique_ptr<TypeExpr> &retType, std::unique_ptr<ExprAST> &body) : arg1(std::move(arg1)), body(std::move(body)), ty(std::move(castType)), ret(std::move(retType)) {};
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class AssertionExprAST : public ExprAST
	{
		std::unique_ptr<ExprAST> msg = NULL, condition = NULL;
		int line;

	public:
		AssertionExprAST(std::unique_ptr<ExprAST> &message, std::unique_ptr<ExprAST> &condition, int line) : line(line), msg(std::move(message)), condition(std::move(condition)) {}
		AssertionExprAST(std::unique_ptr<ExprAST> &condition, int line) : line(line), condition(std::move(condition)) {}
		llvm::Value *codegen(bool autoDeref = true, llvm::Value *other = NULL);
	};

	class ASMExprAST : public ExprAST
	{
		std::string assembly;

	public:
		ASMExprAST(const std::string &assembly) : assembly(assembly) {}
		llvm::Value *codegen(bool autoderef = true, llvm::Value *other = NULL);
	};
}
#endif