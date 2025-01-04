#pragma once
#include <iostream>
#include "llvm/Support/Casting.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/FileCheck/FileCheck.h"
#include "globals.cpp"
#include "TypeExpr.h"
namespace jimpilier
{

		llvm::Type *DoubleTypeExpr::codegen(bool testforval) { return llvm::Type::getDoubleTy(*ctxt); };
		std::unique_ptr<TypeExpr> DoubleTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new DoubleTypeExpr());
		}

		llvm::Type *FloatTypeExpr::codegen(bool testforval) { return llvm::Type::getFloatTy(*ctxt); };
		std::unique_ptr<TypeExpr> FloatTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new FloatTypeExpr());
		}

		llvm::Type *LongTypeExpr::codegen(bool testforval) { return llvm::Type::getInt64Ty(*ctxt); };
		std::unique_ptr<TypeExpr> LongTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new LongTypeExpr());
		}

		llvm::Type *IntTypeExpr::codegen(bool testforval) { return llvm::Type::getInt32Ty(*ctxt); };
		std::unique_ptr<TypeExpr> IntTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new IntTypeExpr());
		}

		llvm::Type *ShortTypeExpr::codegen(bool testforval) { return llvm::Type::getInt16Ty(*ctxt); };
		std::unique_ptr<TypeExpr> ShortTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new ShortTypeExpr());
		}
		llvm::Type *ByteTypeExpr::codegen(bool testforval) { return llvm::Type::getInt8Ty(*ctxt); };
		std::unique_ptr<TypeExpr> ByteTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new ByteTypeExpr());
		}

		llvm::Type *BoolTypeExpr::codegen(bool testforval) { return llvm::Type::getInt1Ty(*ctxt); };
		std::unique_ptr<TypeExpr> BoolTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new BoolTypeExpr());
		}

		llvm::Type *VoidTypeExpr::codegen(bool testforval) { return llvm::Type::getVoidTy(*ctxt); };
		std::unique_ptr<TypeExpr> VoidTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new VoidTypeExpr());
		}

		llvm::Type *TemplateObjectExpr::codegen(bool testforval){
			auto& templ = TemplateMgr.getTemplate(name, types);
			
			//Check that the object doesn't already exist; if it does, return it
			std::string typenames; 
			for(auto &x : types) typenames+=x->getName()+','; 
			typenames = name+'<'+typenames.substr(0,typenames.size()-1)+'>'; 
			if(AliasMgr(typenames)) return AliasMgr(typenames); 
			//Object doesn't already exist, create it.
			std::vector<llvm::Type*> generatedTypes; 
			//Manditory to generate types this way to avoid bugs with recursive template types
			for(auto & x : types) generatedTypes.push_back(x->codegen()); 
			for(int i = 0; i < templ.templates.size(); i++){
				auto & x = templ.templates[i]; 
				AliasMgr.objects.replaceObject(x->getName(), generatedTypes[i]); 
			} 
			
			std::vector<llvm::Type*> objectTypes; 
			std::vector<std::string> objectNames; 
			for(auto &x : templ.members){
				llvm::Type* currentMember = x.ty->codegen(); 
				objectTypes.push_back(currentMember); 
				objectNames.push_back(x.name); 
			}
			llvm::Type* ret = llvm::StructType::create(*ctxt, objectTypes, typenames,false); 
			AliasMgr.objects.addObject(typenames, ret); 
			AliasMgr.objects.addObjectMembers(typenames, objectTypes, objectNames); 

			for(auto &x : templ.functions){
				x->replaceTemplate(typenames); 
				x->codegen(); 
			}

			for(int i = 0; i < templ.templates.size(); i++){
				auto & x = templ.templates[i]; 
				AliasMgr.objects.removeObject(x->getName()); 
			}
			return ret; 
		}
		std::string TemplateObjectExpr::getName() { 
			std::string names; 
			for(auto& x : types) names+=x->getName()+','; 
			return name+'<'+names.substr(0, names.size()-1)+'>'; 
			}
		std::unique_ptr<TypeExpr> TemplateObjectExpr::clone(){
			std::vector<std::unique_ptr<TypeExpr>> cpy; 
			for(auto&x:types){
				cpy.push_back(std::move(x->clone())); 
			}
			return std::make_unique<TemplateObjectExpr>(name, cpy); 
		}
		StructTypeExpr::StructTypeExpr(const std::string &structname) : name(structname) {}
		llvm::Type *StructTypeExpr::codegen(bool testforval)
		{
			llvm::Type *ty = AliasMgr(name);
			if (!testforval && ty == NULL)
			{
				logError("Unknown object of name: " + name);
				return NULL;
			}
			return ty;
		}
		std::string StructTypeExpr::getName() { return name; }
		std::unique_ptr<TypeExpr> StructTypeExpr::clone()
		{
			return std::unique_ptr<TypeExpr>(new StructTypeExpr(name));
		}

		llvm::Type *PointerToTypeExpr::codegen(bool testforval)
		{
			llvm::Type *t = ty->codegen();
			return t == NULL ? NULL : t->getPointerTo();
		}

		llvm::Type *ArrayOfTypeExpr::codegen(bool testforval){
			std::vector<std::unique_ptr<TypeExpr>> tyarr;
			tyarr.push_back(std::move(ty));  
			std::string name(".array"); 
						auto& templ = TemplateMgr.getTemplate(name, types);
			
			//Check that the object doesn't already exist; if it does, return it
			std::string typenames; 
			for(auto &x : types) typenames+=x->getName()+','; 
			typenames = name+'<'+typenames.substr(0,typenames.size()-1)+'>'; 
			if(AliasMgr(typenames)) return AliasMgr(typenames); 
			//Object doesn't already exist, create it.
			std::vector<llvm::Type*> generatedTypes; 
			//Manditory to generate types this way to avoid bugs with recursive template types
			for(auto & x : types) generatedTypes.push_back(x->codegen()); 
			for(int i = 0; i < templ.templates.size(); i++){
				auto & x = templ.templates[i]; 
				AliasMgr.objects.replaceObject(x->getName(), generatedTypes[i]); 
			} 
			
			std::vector<llvm::Type*> objectTypes; 
			std::vector<std::string> objectNames; 
			for(auto &x : templ.members){
				llvm::Type* currentMember = x.ty->codegen(); 
				objectTypes.push_back(currentMember); 
				objectNames.push_back(x.name); 
			}
			llvm::Type* ret = llvm::StructType::create(*ctxt, objectTypes, typenames,false); 
			AliasMgr.objects.addObject(typenames, ret); 
			AliasMgr.objects.addObjectMembers(typenames, objectTypes, objectNames); 

			for(auto &x : templ.functions){
				x->replaceTemplate(typenames); 
				x->codegen(); 
			}

			for(int i = 0; i < templ.templates.size(); i++){
				auto & x = templ.templates[i]; 
				AliasMgr.objects.removeObject(x->getName()); 
			}
			return ret; 
		}
		std::unique_ptr<TypeExpr> PointerToTypeExpr::clone()
		{
			std::unique_ptr<TypeExpr> encasedType = std::move(ty->clone());
			return std::make_unique<PointerToTypeExpr>(encasedType);
		}

		std::unique_ptr<TypeExpr> ArrayOfTypeExpr::clone()
		{
			std::unique_ptr<TypeExpr> encasedType = std::move(ty->clone());
			return std::make_unique<PointerToTypeExpr>(encasedType);
		}

		llvm::Type *ReferenceToTypeExpr::codegen(bool testforval)
		{
			llvm::Type *t = ty->codegen();
			return t == NULL ? NULL : t->getPointerTo();
		}
		std::unique_ptr<TypeExpr> ReferenceToTypeExpr::clone()
		{
			std::unique_ptr<TypeExpr> encasedType = std::move(ty->clone());
			return std::make_unique<ReferenceToTypeExpr>(encasedType);
		}

		Variable::Variable(const std::string &ident, std::unique_ptr<TypeExpr> &type) : name(ident), ty(std::move(type)) {}
		Variable::Variable() : name("")
		{
			ty = NULL;
		}
		std::string Variable::toString()
		{
			return "{ " + name + ", " + AliasMgr.getTypeName(ty->codegen()) + " }";
		}
}