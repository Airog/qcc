#include "pp.hpp"
#include "lexer.hpp"
#include "parse.hpp"

Token Preprocessor::run(Token tok) {
  token = tok;

  while(token.get().type != TOK_TYPE_END) {
    if(token.skip("#")) {
           if(token.skip("include"))             read_include();
      else if(token.skip("define"))              read_define();
      else if(token.skip("undef"))               read_undef();
      else if(token.skip("if"))                  read_if();
      else if(token.skip("elif"))                read_elif();
      else if(token.skip("else"))                read_else();
      else if(token.skip("ifdef"))               read_ifdef();
      else if(token.skip("ifndef"))              read_ifndef();
      else if(token.skip("endif"))               skip_this_line();
      else if(token.skip("error"))               skip_this_line();
      else error("error in preprocessor(%d): #%s", token.get().line, token.get().val.c_str());
    } else if(is_defined(token.get().val)) { 
      auto m = replace_macro(token);
      std::copy(m.begin(), m.end(), std::back_inserter(new_token.token));
    } else if(token.get().type != TOK_TYPE_NEWLINE) {
      new_token.token.push_back(token.next());
    } else token.skip();
  }
  token.pos = 0;
  return new_token;
}

void Preprocessor::read_define() {
  std::string macro_name; bool funclike = false;
  std::vector<std::string> args_name;
  if(token.get().type == TOK_TYPE_IDENT)
    macro_name = token.next().val;
  if(token.is("(") && !token.get().space) {
    token.skip();
    funclike = true;
    while(1) {
      args_name.push_back(token.next().val);
      if(token.skip(")")) break;
      token.expect_skip(",");
    }
  }
  std::vector<token_t> content;
  while(token.get().type != TOK_TYPE_NEWLINE) {
    content.push_back(token.next());
  }
  if(funclike) add_define_funclike_macro(macro_name, args_name, content);
  else add_define_macro(macro_name, content);
}

void Preprocessor::read_undef() {
  std::string macro;
  if(token.get().type == TOK_TYPE_IDENT)
    macro = token.next().val;
  auto it = define_map.find(macro);
  if(it != define_map.end()) // if macro was declared
    define_map.erase(it);
}

token_t Preprocessor::read_defined_op() {
  std::string macro_name;
  if(token.skip("(")) {
    macro_name = token.next().val;
    token.expect_skip(")");
  } else {
    macro_name = token.next().val;
  }
  std::cout << "MACRO " << macro_name << "DEFINED? " << this->define_map.count(macro_name) << std::endl;;
  return is_defined(macro_name) ? 
    token_t(TOK_TYPE_NUMBER, "1", token.get().line) : 
    token_t(TOK_TYPE_NUMBER, "0", token.get().line);
}

Token Preprocessor::read_expr_line() {
  Token expr_line;
  for(;;) {
    if(token.get().type == TOK_TYPE_NEWLINE) break;
    if(token.skip("defined")) {
      expr_line.token.push_back(read_defined_op());
    } else {
      auto tok = token.get();
      if(is_defined(tok.val)) {
        auto q = replace_macro(token);
        for(auto a : q)
          expr_line.token.push_back(a);
      } else if(tok.type == TOK_TYPE_IDENT) {
        // INQCC: identifiers are replaced by 0
        token.skip();
        expr_line.token.push_back(token_t(TOK_TYPE_NUMBER, "0"));
      } else {
        token.skip();
        expr_line.token.push_back(tok);
      }
    }
  }
  return expr_line;
}

bool Preprocessor::read_constexpr() {
  // TODO: implement processing of const expression
  auto tok = read_expr_line();
  tok.add_symbol_tok(";", 0);
  tok.add_end_tok();
  tok.show();
  tok.pos = 0;
  Parser parser;
  auto expr = parser.run(tok, true)[0];
  bool cond = eval_constexpr(expr);
  return cond;
}

void Preprocessor::do_read_if(bool m) {
  cond_stack.push(m);
  // std::cout << "if cond = " << m << std::endl;
  if(!m) skip_cond_include();
}

void Preprocessor::read_if() {
  do_read_if(read_constexpr());
}

void Preprocessor::read_elif() {
  if(cond_stack.top() || !read_constexpr())
    skip_cond_include();
  else
    cond_stack.top() = true;
}

void Preprocessor::read_ifdef() {
  if(token.get().type != TOK_TYPE_IDENT) error("error: in pp");
  std::string macro = token.next().val;
  do_read_if( is_defined(macro) );
}

void Preprocessor::read_ifndef() {
  if(token.get().type != TOK_TYPE_IDENT) error("error: in pp");
  std::string macro = token.next().val;
  do_read_if( !is_defined(macro) );
}

void Preprocessor::read_else() {
  if(cond_stack.empty()) error("error: in pp");
  if(cond_stack.top()) // if immediately before cond is true
    skip_cond_include();
}

void Preprocessor::skip_cond_include() {
  int nest = 0;
  for(;;) {
    if(!token.skip("#")) { token.skip(); continue; }
    if(!nest && (token.skip("else") || token.skip("elif") || token.skip("endif"))) {
      token.prev(); // else elif endif
      token.prev(); // #
      return;
    }
    if(token.skip("if") || token.skip("ifdef") || token.skip("ifndef"))
      nest++;
    else if(nest && token.skip("endif"))
      nest--;
  }
}

void Preprocessor::read_include() {
  std::string file_name;
  if(token.skip("<")) {
    while(!token.skip(">")) file_name += token.next().val;
  } else if(token.get().type == TOK_TYPE_STRING) {
    file_name = token.next().val;
  }
  std::function<std::string(int)> include_content = [&](int incl_n) -> std::string {
    if(incl_n == default_include_path.size()) { puts("NOT FOUND INCLUDE FILE "); exit(0); }
    std::ifstream ifs_src(default_include_path[incl_n] + file_name);
    // std::cout << "NOW INCLUDING " << default_include_path[incl_n] + file_name << std::endl;
    if(!ifs_src) { return include_content(incl_n+1); }
    std::istreambuf_iterator<char> it(ifs_src), last;
    std::string src_all(it, last);
    return src_all;
  };
  Lexer lex; Token include_tok = lex.run(include_content(0));
  Preprocessor pp; 
  for(auto incl_def : define_map)
    pp.define_map[incl_def.first] = incl_def.second;
  include_tok = pp.run(include_tok);
  for(auto incl_def : pp.define_map) 
    define_map[incl_def.first] = incl_def.second;
  for(auto t : include_tok.token)
    new_token.token.push_back(t);
}

bool Preprocessor::is_defined(std::string name) {
  return define_map.count(name);
}

std::vector<token_t> &Preprocessor::replace_macro(Token &token, std::vector<token_t> &dst) {
  auto m = replace_macro(token);
  std::copy(m.begin(), m.end(), std::back_inserter(dst));
  return dst;
}

std::vector<token_t> Preprocessor::replace_macro_obj(Token &token, define_t &macro) {
  std::vector<token_t> body;

  auto tok = macro.rep;
  while(tok.pos < tok.token.size()) {
    if(is_defined(tok.get().val)) {
      replace_macro(tok, body);
    } else body.push_back(tok.next());
  }
  token.skip(); // ident
  return body;
}
std::vector<token_t> Preprocessor::replace_macro_func(Token &token, define_t &macro) {
  std::vector<token_t> body;
  // funclike macro
  token.skip(); // IDENT
  token.expect_skip("(");
  std::vector< std::vector<token_t> > args;
  int nest = 1;
  while(token.pos < token.token.size()) {
    std::vector<token_t> arg;
    while(1) {
      if(token.is("(")) nest++; if(token.is(")")) nest--;
      if((token.is(")") && !nest) || token.is(",")) break;
      if(is_defined(token.get().val)) {
        replace_macro(token, arg);
      } else 
        arg.push_back(token.next());
    }
    args.push_back(arg);
    if(token.skip(")") && !nest) break;
    token.expect_skip(",");
  }

  macro.rep.pos = 0;
  Token tok = macro.rep;
    
  for(; tok.pos < tok.token.size();) {
    bool expand = false;
    bool stringize = false;
    stringize = tok.skip("#");
    for(int i = 0; i < macro.args.size(); i++) {
      if(tok.skip(macro.args[i])) { //it->token.val == macro.args[i]) {
        if(stringize) {
          std::string str;
          for(auto s : args[i]) 
            str += (s.space && !str.empty() ? " " : "") + 
              (s.type == TOK_TYPE_STRING ? 
               "\"" + s.val + "\"" : 
               s.type == TOK_TYPE_CHAR   ?
               "\'" + s.val + "\'" : s.val);
          body.push_back(token_t(TOK_TYPE_STRING, str));
        } else {
          for(auto a : args[i])
            body.push_back(a);
        }
        expand = true;
        break;
      } 
    }
    if(!expand) 
      body.push_back(tok.next());
  }
  tok.token = body;
  tok.pos = 0;
  body.clear();
  for(; tok.pos < tok.token.size();) {
    if(define_map.count(tok.get().val)) {
      auto m = replace_macro(tok, body);
    } else 
      body.push_back(tok.next());
  }

  return body;
}

std::vector<token_t> Preprocessor::replace_macro(Token &token) {
  auto macro = define_map[token.get().val];
  // normal macro
  if(macro.type == DEFINE_MACRO) {
    return replace_macro_obj(token, macro);
  } else if(macro.type == DEFINE_FUNCLIKE_MACRO) {
    return replace_macro_func(token, macro);
  }
  error("unknown");
  return std::vector<token_t>();
}

void Preprocessor::skip_this_line() {
  while(token.get().type != TOK_TYPE_NEWLINE) token.skip();
}

void Preprocessor::add_define_macro(std::string name, std::vector<token_t> rep) {
  define_t def;
  def.type = DEFINE_MACRO;
  def.rep = Token(rep);
  define_map[name] = def;
}
void Preprocessor::add_define_funclike_macro(std::string name, std::vector<std::string> args_name, std::vector<token_t> rep) {
  define_t def;
  def.type = DEFINE_FUNCLIKE_MACRO;
  def.args = args_name;
  def.rep = Token(rep);
  define_map[name] = def;
}
