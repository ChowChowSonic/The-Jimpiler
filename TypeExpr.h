#pragma once
#ifndef typeExpr
#define typeExpr
#include "globals.cpp"
namespace jimpilier
{

	class TypeExpr
	{
	public:
		virtual ~TypeExpr(){};
		virtual llvm::Type *codegen(bool testforval = false) = 0;
		virtual std::unique_ptr<TypeExpr> clone() = 0;
		virtual std::string getName() = 0; 
		virtual bool isReference()
		{
			return false;
		}
	};

	class DoubleTypeExpr : public TypeExpr
	{
	public:
		DoubleTypeExpr() {}
		llvm::Type *codegen(bool testforval = false);
		std::string getName() {
			return "double"; 
		}; 
		std::unique_ptr<TypeExpr> clone();
	};

	class FloatTypeExpr : public TypeExpr
	{
	public:
		FloatTypeExpr() {}
		llvm::Type *codegen(bool testforval = false);
		std::string getName() { return "float"; }
		std::unique_ptr<TypeExpr> clone();
	};

	class LongTypeExpr : public TypeExpr
	{
	public:
		LongTypeExpr() {}
		llvm::Type *codegen(bool testforval = false);
		std::string getName() { return "long"; }
		std::unique_ptr<TypeExpr> clone();
	};

	class IntTypeExpr : public TypeExpr
	{
	public:
		IntTypeExpr() {}
		llvm::Type *codegen(bool testforval = false);
		std::string getName() { return "int"; }
		std::unique_ptr<TypeExpr> clone();
	};

	class ShortTypeExpr : public TypeExpr
	{
	public:
		ShortTypeExpr() {}
		llvm::Type *codegen(bool testforval = false);
		std::string getName() { return "short"; }
		std::unique_ptr<TypeExpr> clone();
	};

	class ByteTypeExpr : public TypeExpr
	{
	public:
		ByteTypeExpr() {}
		llvm::Type *codegen(bool testforval = false);
		std::string getName() { return "byte"; }
		std::unique_ptr<TypeExpr> clone();
	};

	class BoolTypeExpr : public TypeExpr
	{
	public:
		BoolTypeExpr() {}
		llvm::Type *codegen(bool testforval = false);
		std::string getName() { return "bool"; }
		std::unique_ptr<TypeExpr> clone();
	};

	class VoidTypeExpr : public TypeExpr
	{
	public:
		VoidTypeExpr() {}
		llvm::Type *codegen(bool testforval = false);
		std::string getName() { return "void"; }
		std::unique_ptr<TypeExpr> clone();
	};

/**
 * @brief Represents a fully defined object that has filled all templates associated with it
 * 
 * @example vector<int> - valid
 * @example map<string, int> - valid
 * @example vector<T> - not valid so long as 'T' is undefined/not an object
 * 
 */
	class TemplateObjectExpr : public TypeExpr
	{
		std::string name;
		std::vector<std::unique_ptr<TypeExpr>> types;
	public:
		TemplateObjectExpr(const std::string name, std::vector<std::unique_ptr<TypeExpr>> &templateTypes) : name(name), types(std::move(templateTypes)){};
		llvm::Type *codegen(bool testforval = false);
		std::string getName(); 
		std::unique_ptr<TypeExpr> clone();
	};

	class StructTypeExpr : public TypeExpr
	{
		std::string name;

	public:
		StructTypeExpr(const std::string &structname);
		llvm::Type *codegen(bool testforval = false);
		std::string getName(); 
		std::unique_ptr<TypeExpr> clone();
	};

	class PointerToTypeExpr : public TypeExpr
	{
		std::unique_ptr<TypeExpr> ty;

	public:
		PointerToTypeExpr(std::unique_ptr<TypeExpr> &type) : ty(std::move(type)) {}
		llvm::Type *codegen(bool testforval = false);
		std::string getName() { return ty->getName()+'*'; }
		std::unique_ptr<TypeExpr> clone();
	};

	class ReferenceToTypeExpr : public TypeExpr
	{
		std::unique_ptr<TypeExpr> ty;

	public:
		ReferenceToTypeExpr(std::unique_ptr<TypeExpr> &type) : ty(std::move(type)) {}
		llvm::Type *codegen(bool testforval = false);
		std::string getName() { return ty->getName()+'&'; }
		std::unique_ptr<TypeExpr> clone();

		bool isReference()
		{
			return true;
		}
	};

	class Variable
	{
	public:
		std::string name;
		std::unique_ptr<TypeExpr> ty;
		Variable(const std::string &ident, std::unique_ptr<TypeExpr> &type);
		Variable(std::pair<std::string, std::unique_ptr<TypeExpr>> &both) : name(both.first), ty(std::move(both.second)) {};
		Variable();
		std::string toString();
		/**
		 * @brief Returns this object, converted into a std::pair; makes a copy of the internal unique_ptr<TypeExpr> for later reuse 
		 * 
		 * @return std::pair<std::string, std::unique_ptr<TypeExpr>> 
		 */
		std::pair<std::string, std::unique_ptr<TypeExpr>> toPair(){
			return std::pair<std::string, std::unique_ptr<TypeExpr>>(name, ty->clone()); 
		}
	};

}; 
#endif