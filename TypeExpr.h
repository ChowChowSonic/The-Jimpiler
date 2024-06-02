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
		std::unique_ptr<TypeExpr> clone();
	};

	class FloatTypeExpr : public TypeExpr
	{
	public:
		FloatTypeExpr() {}
		llvm::Type *codegen(bool testforval = false);
		std::unique_ptr<TypeExpr> clone();
	};

	class LongTypeExpr : public TypeExpr
	{
	public:
		LongTypeExpr() {}
		llvm::Type *codegen(bool testforval = false);
		std::unique_ptr<TypeExpr> clone();
	};

	class IntTypeExpr : public TypeExpr
	{
	public:
		IntTypeExpr() {}
		llvm::Type *codegen(bool testforval = false);
		std::unique_ptr<TypeExpr> clone();
	};

	class ShortTypeExpr : public TypeExpr
	{
	public:
		ShortTypeExpr() {}
		llvm::Type *codegen(bool testforval = false);
		std::unique_ptr<TypeExpr> clone();
	};

	class ByteTypeExpr : public TypeExpr
	{
	public:
		ByteTypeExpr() {}
		llvm::Type *codegen(bool testforval = false);
		std::unique_ptr<TypeExpr> clone();
	};

	class BoolTypeExpr : public TypeExpr
	{
	public:
		BoolTypeExpr() {}
		llvm::Type *codegen(bool testforval = false);
		std::unique_ptr<TypeExpr> clone();
	};

	class VoidTypeExpr : public TypeExpr
	{
	public:
		VoidTypeExpr() {}
		llvm::Type *codegen(bool testforval = false);
		std::unique_ptr<TypeExpr> clone();
	};

	class TemplateTypeExpr : public TypeExpr
	{
		std::string name;

	public:
		TemplateTypeExpr(const std::string name);
		llvm::Type *codegen(bool testforval = false);
		std::unique_ptr<TypeExpr> clone();
	};

	class StructTypeExpr : public TypeExpr
	{
		std::string name;

	public:
		StructTypeExpr(const std::string &structname);
		llvm::Type *codegen(bool testforval = false);
		std::unique_ptr<TypeExpr> clone();
	};

	class PointerToTypeExpr : public TypeExpr
	{
		std::unique_ptr<TypeExpr> ty;

	public:
		PointerToTypeExpr(std::unique_ptr<TypeExpr> &type) : ty(std::move(type)) {}
		llvm::Type *codegen(bool testforval = false);
		std::unique_ptr<TypeExpr> clone();
	};

	class ReferenceToTypeExpr : public TypeExpr
	{
		std::unique_ptr<TypeExpr> ty;

	public:
		ReferenceToTypeExpr(std::unique_ptr<TypeExpr> &type) : ty(std::move(type)) {}
		llvm::Type *codegen(bool testforval = false);
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
		Variable();
		std::string toString();
	};

}
#endif