#pragma once

#include "common.hpp"
#include "token.hpp"
#include "ast.hpp"
#include "struct.hpp"


class Parser {
  private:
    Token token;
  public:
    AST_vec run(Token);

    AST_vec eval();
    AST *statement_top();
    AST *statement();

    std::map<std::string, llvm::Type *> typedef_map;
    bool is_function_def();
    bool is_function_proto();
    bool is_type();
    AST *make_function();
    AST *make_function_proto();
    AST *make_var_declaration();
    AST *read_declaration();
    llvm::Type *read_declarator(std::string &, llvm::Type *);
    llvm::Type *read_declarator(std::string &, llvm::Type *, std::vector<argument_t *>);
    llvm::Type *read_declarator_func(llvm::Type *, std::vector<argument_t *>);
    llvm::Type *read_declarator_tail(llvm::Type *);
    llvm::Type *read_declarator_array(llvm::Type *);
    // Type_vec read_field();
    StructList struct_list;
    llvm::Type *make_struct_declaration();
    std::map<std::string, NumberAST *> enum_list;
    llvm::Type *make_enum_declaration();
    void read_typedef();
    AST *make_block();
    AST *make_break();
    AST *make_continue();
    AST *make_if();
    AST *make_while();
    AST *make_for();
    AST *make_return();

    llvm::Type *to_llvm_type(std::string);
    llvm::Type *skip_type_spec();
    llvm::Type *read_primitive_type();
    llvm::Type *read_struct_union_type();
    llvm::Type *read_enum_type();
    llvm::Type *read_type_declarator();
    int skip_pointer();
    std::vector<int> skip_array(); // .size() = number of [], elem = ary size

    std::map<std::string, int> op_prec;
    int get_op_prec(std::string);
    AST *expr_entry();
    AST *expr_asgmt();
    AST *expr_unary();
    AST *expr_unary_postfix(AST *);
    AST *expr_ternary(AST *);
    AST *expr_dot();
    AST *expr_index();
    AST *expr_primary();
    AST *expr_array();
    AST *expr_rhs(int, AST *);
};
