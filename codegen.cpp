#include "codegen.hpp"

llvm::LLVMContext &context(llvm::getGlobalContext());
llvm::IRBuilder<> builder(context);
llvm::Module *mod;

void Codegen::run(AST_vec ast) {
  Type ty;
  mod = new llvm::Module("QCC", context);
  for(auto st : ast) statement(st, &ty);
  mod->dump();
  std::string EC;
  llvm::raw_fd_ostream out("a.bc", EC, llvm::sys::fs::OpenFlags::F_RW);
  llvm::WriteBitcodeToFile(mod, out);
}

llvm::AllocaInst *Codegen::create_entry_alloca(llvm::Function *TheFunction, std::string &VarName, llvm::Type *type) {
  llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
      TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(type == nullptr ? llvm::Type::getInt32Ty(context) : type, 0, VarName.c_str());
}

llvm::Type *Codegen::to_llvm_type(Type *type) {
  switch(type->get().type) {
    case TY_VOID:
      return builder.getVoidTy();
    case TY_INT:
      return builder.getInt32Ty();
    case TY_CHAR:
      return builder.getInt8Ty();
    case TY_PTR:
      return to_llvm_type(type->next)->getPointerTo();
  }
  return nullptr;
}

llvm::Value *Codegen::statement(AST *st, Type *ret_type) {
  switch(st->get_type()) {
    case AST_FUNCTION_DEF:
      return statement((FunctionDefAST *)st, ret_type);
    case AST_FUNCTION_PROTO:
      return statement((FunctionProtoAST *)st, ret_type);
    case AST_FUNCTION_CALL:
      return statement((FunctionCallAST *)st, ret_type);
    case AST_IF:
      return statement((IfAST *)st, ret_type);
    case AST_RETURN:
      return statement((ReturnAST *)st, ret_type);
    case AST_VAR_DECLARATION:
      return statement((VarDeclarationAST *)st, ret_type);
    case AST_VARIABLE:
      return statement((VariableAST *)st, ret_type);
    case AST_ASGMT:
      return statement((AsgmtAST *)st, ret_type);
    case AST_BINARY:
      return statement((BinaryAST *)st, ret_type);
    case AST_STRING:
      return statement((StringAST *)st, ret_type);
    case AST_NUMBER:
      return statement((NumberAST *)st, ret_type);
  }
  return nullptr;
}

llvm::Value *Codegen::statement(FunctionProtoAST *st, Type *ret_type) {
  func_t func;
  func.name = st->name;
  func.ret_type = st->ret_type;
  std::vector<llvm::Type *> llvm_args_type;
  std::vector<Type *>       args_type;
  std::vector<std::string>  args_name;
  bool has_vararg = false;
  for(auto arg : st->args) {
    if(arg->eql(TY_VARARG)) {
      has_vararg = true;
    } else {
      func.args_type.push_back(arg);
      func.llvm_args_type.push_back(to_llvm_type(arg));
    }
  }
  this->func_list.add(func);
  func_t *function = this->func_list.get(func.name);
  
  llvm::FunctionType *llvm_func_type = 
    llvm::FunctionType::get(to_llvm_type(func.ret_type), func.llvm_args_type, has_vararg);
  llvm::Function *llvm_func = 
    llvm::Function::Create(llvm_func_type, llvm::Function::ExternalLinkage, func.name, mod);
  function->llvm_function = llvm_func;

  return nullptr;
}

llvm::Value *Codegen::statement(FunctionDefAST *st, Type *ret_type) {
  func_t func;
  func.name = st->name;
  func.ret_type = st->ret_type;
  std::vector<llvm::Type *> llvm_args_type;
  std::vector<Type *>       args_type;
  std::vector<std::string>  args_name;
  for(auto arg : st->args) {
    func.var_list.add(var_t(arg->name, arg->type));
    func.args_type.push_back(arg->type);
    func.args_name.push_back(arg->name);
    func.llvm_args_type.push_back(to_llvm_type(arg->type));
  }
  this->func_list.add(func);
  func_t *function = this->func_list.get(func.name);

  llvm::FunctionType *llvm_func_type = 
    llvm::FunctionType::get(to_llvm_type(func.ret_type), func.llvm_args_type, false);
  llvm::Function *llvm_func = 
    llvm::Function::Create(llvm_func_type, llvm::Function::ExternalLinkage, func.name, mod);
  function->llvm_function = llvm_func;

  { // create function body
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", function->llvm_function);
    builder.SetInsertPoint(entry);
    
    auto llvm_args_type_it = function->llvm_args_type.begin();
    auto      args_name_it = function->args_name.begin();
    for(auto arg_it = function->llvm_function->arg_begin(); arg_it != function->llvm_function->arg_end(); ++arg_it) {
      arg_it->setName(*args_name_it);
      llvm::AllocaInst *ainst = create_entry_alloca(function->llvm_function, *args_name_it, *llvm_args_type_it);
      builder.CreateStore(arg_it, ainst);
      var_t *v = function->var_list.get(*args_name_it);
      if(v) v->val = ainst;
      llvm_args_type_it++; args_name_it++;
    }

    Type return_type;
    cur_func = function;
    for(auto it : st->body) statement(it, &return_type);
  }

  return nullptr;
}

llvm::Value *Codegen::statement(FunctionCallAST *st, Type *ret_type) {
  func_t *func = this->func_list.get(st->name);
  if(func == nullptr) error("error: not found the function \'%s\'", st->name.c_str());
  std::vector<llvm::Value *> caller_args;
  for(auto a : st->args) 
    caller_args.push_back(statement(a, ret_type));
  auto callee = func->llvm_function;
  ret_type->change(func->ret_type);
  return builder.CreateCall(callee, caller_args, "call");
}

llvm::Value *Codegen::statement(VarDeclarationAST *st, Type *ret_type) {
  for(auto v : st->decls) {
    cur_func->var_list.add(var_t(v->name, v->type));
    auto cur_var = cur_func->var_list.get(v->name);
    llvm::Type *decl_type = to_llvm_type(cur_var->type);
    cur_var->val = builder.CreateAlloca(decl_type);
  }
  return nullptr;
}

llvm::Value *Codegen::statement(IfAST *st, Type *ret_type) {
  llvm::Value *cond_val = statement(st->cond, ret_type);
  cond_val = builder.CreateICmpNE(cond_val, llvm::ConstantInt::get(builder.getInt1Ty(), 0, true));

  auto *func = builder.GetInsertBlock()->getParent();

  llvm::BasicBlock *bb_then = llvm::BasicBlock::Create(context, "then", func);
  llvm::BasicBlock *bb_else = llvm::BasicBlock::Create(context, "else");
  llvm::BasicBlock *bb_merge= llvm::BasicBlock::Create(context, "merge");

  builder.CreateCondBr(cond_val, bb_then, bb_else);
  builder.SetInsertPoint(bb_then);

  bool necessary_merge = false;
  cur_func->br_list.push(false);
  statement(st->b_then, ret_type);
  if(cur_func->br_list.top())
    cur_func->br_list.pop();
  else builder.CreateBr(bb_merge), necessary_merge = true;
  bb_then = builder.GetInsertBlock();

  func->getBasicBlockList().push_back(bb_else);
  builder.SetInsertPoint(bb_else);

  cur_func->br_list.push(false);
  statement(st->b_else, ret_type);
  if(cur_func->br_list.top())
    cur_func->br_list.pop();
  else builder.CreateBr(bb_merge), necessary_merge = true;
  bb_else = builder.GetInsertBlock();

  if(necessary_merge) {
    func->getBasicBlockList().push_back(bb_merge);
    builder.SetInsertPoint(bb_merge);
  }
  
  return nullptr;
}

llvm::Value *Codegen::statement(ReturnAST *st, Type *ret_type) {
  if(!cur_func->br_list.empty()) cur_func->br_list.top() = true;
  return builder.CreateRet(statement(st->expr, ret_type));
}

llvm::Value *Codegen::statement(VariableAST *st, Type *ret_type) {
  auto var = this->cur_func->var_list.get(st->name);
  if(var) return builder.CreateLoad(var->val);
  error("error: not found variable '%s'", var->name.c_str());
  return nullptr;
}

llvm::Value *Codegen::statement(AsgmtAST *st, Type *ret_type) {
  auto src = statement(st->src, ret_type);
  llvm::Value *dst = nullptr;
  if(st->dst->get_type() == AST_VARIABLE) {
    VariableAST *va = (VariableAST *)st->dst;
    auto cur_var = cur_func->var_list.get(va->name);
    dst = cur_var->val;
  }
  builder.CreateStore(src, dst);
  return builder.CreateLoad(dst);
}

llvm::Value *Codegen::statement(BinaryAST *st, Type *ret_type) {
  auto lhs = statement(st->lhs, ret_type),
       rhs = statement(st->rhs, ret_type);
  llvm::Value *ret = nullptr;
  if(st->op == "+") {
    ret = builder.CreateAdd(lhs, rhs);
  } else if(st->op == "-") {
    ret = builder.CreateSub(lhs, rhs);
  } else if(st->op == "*") {
    ret = builder.CreateMul(lhs, rhs);
  } else if(st->op == "/") {
    ret = builder.CreateSDiv(lhs, rhs);
  } else if(st->op == "%") {
    ret = builder.CreateSRem(lhs, rhs);
  } else if(st->op == "==") {
    ret = builder.CreateICmpEQ(lhs, rhs);
  } else if(st->op == "!=") {
    ret = builder.CreateICmpNE(lhs, rhs);
  } else if(st->op == "<=") {
    ret = builder.CreateICmpSLE(lhs, rhs);
  } else if(st->op == (">=")) {
    ret = builder.CreateICmpSGE(lhs, rhs);
  } else if(st->op == "<") {
    ret = builder.CreateICmpSLT(lhs, rhs);
  } else if(st->op == (">")) {
    ret = builder.CreateICmpSGT(lhs, rhs);
  } else if(st->op == "&" && st->op == "&&") {
    ret = builder.CreateAnd(lhs, rhs);
  } else if(st->op == "|" && st->op == "||") {
    ret = builder.CreateOr(lhs, rhs);
  } else if(st->op == "^") {
    ret = builder.CreateXor(lhs, rhs);
  }
  return ret;
}

llvm::Value *Codegen::statement(StringAST *st, Type *ret_type) {
  ret_type->change(new Type(TY_PTR, new Type(TY_CHAR)));
  return builder.CreateGlobalStringPtr(st->str);
}

llvm::Value *Codegen::statement(NumberAST *st, Type *ret_type) {
  ret_type->change(new Type(TY_PTR, new Type(TY_INT)));
  return llvm::ConstantInt::get(builder.getInt32Ty(), st->number, true);
}

void Codegen::error(const char *errs, ...) {
  va_list args;
  va_start(args, errs);
    vprintf(errs, args); puts("");
  va_end(args);
  exit(0);
}