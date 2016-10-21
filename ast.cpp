#include "ast.hpp"

FunctionProtoAST::FunctionProtoAST(std::string _name, Type *ret, Type_vec _args):
  name(_name), ret_type(ret), args(_args) {
}

FunctionDefAST::FunctionDefAST(std::string _name, Type *ret, std::vector<argument_t *> _args, AST_vec _body):
  name(_name), ret_type(ret), args(_args), body(_body) {
}

FunctionCallAST::FunctionCallAST(std::string _name, AST_vec _args):
  name(_name), args(_args) {
}

BinaryAST::BinaryAST(std::string _op, AST *_lhs, AST *_rhs):
  op(_op), lhs(_lhs), rhs(_rhs) {
}

VarDeclarationAST::VarDeclarationAST(std::vector<declarator_t *> _decls):
  decls(_decls) {
}

VariableAST::VariableAST(std::string _name):
  name(_name) {
}

IfAST::IfAST(AST *_cond, AST *_then, AST *_else):
  cond(_cond), b_then(_then), b_else(_else) {
}

AsgmtAST::AsgmtAST(AST *_dst, AST *_src):
  dst(_dst), src(_src) {
}

ReturnAST::ReturnAST(AST *_expr):
  expr(_expr) {
}

NumberAST::NumberAST(int num):
  number(num) {
}

StringAST::StringAST(std::string _str):
  str(_str) {
}
