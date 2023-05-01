/// Copyright (c) 2023 Samir Bioud
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in all
/// copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
/// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
/// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
/// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
/// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
/// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
/// OR OTHER DEALINGS IN THE SOFTWARE.
///

//
// examples/lexer.cc
//
// An example of how you could use the library to create a lexer for a simple
// c-like language
//
//


#include "../include/builder.h"


enum class TokenType {
  __EOF,

  t_int,
  t_float,
  t_char,
  t_void,

  plus,
  minus,
  star,
  slash,

  _for,
  _while,
  _goto,
  _break,
  _continue,


  l_integer,
  l_float,
  l_str,
  l_char,

  i_lbrace,
  i_rbrace,
  i_lparen,
  i_rparen,
  i_lbrack,
  i_rbrack,
};

enum class ErrorType {
  E_UNTERMINATEDSTR
};

struct TokenVariant {
  union {
    TokenType val_t;
    ErrorType err_t;
  };

  bool is_err;

  TokenVariant(TokenType t) : val_t(t), is_err(false){};
  TokenVariant(ErrorType e) : err_t(e), is_err(true){};

  operator std::string() {
    if (is_err) {
      return "Err# " + std::to_string((int)err_t);
    } else {
      return "Tok# " + std::to_string((int)val_t);
    }
  }

  bool operator==(TokenVariant other) const {
    if (is_err != other.is_err) {
      return false;
    }
    if (is_err) {
      return err_t == other.err_t;
    } else {
      return val_t == other.val_t;
    }
  }
};

int main() {

  regex_table::MutableStateMachine<TokenVariant> machine;

  // clang-format off
  // machine
  //   .match_sequence("int").commit(TokenType::t_int)
  //   .match_sequence("float").commit(TokenType::t_float)
  //   .match_sequence("char").commit(TokenType::t_char)
  //   .match_sequence("void").commit(TokenType::t_void)
  //   .match_sequence("for").commit(TokenType::_for)
  //   .match_sequence("while").commit(TokenType::_while)
  //   .match_sequence("goto").commit(TokenType::_goto)
  //   .match_sequence("break").commit(TokenType::_break)
  //   .match_sequence("continue").commit(TokenType::_continue)
  //   .match_sequence("+").commit(TokenType::plus)
  //   .match_sequence("-").commit(TokenType::minus)
  //   .match_sequence("*").commit(TokenType::star)
  //   .match_sequence("/").commit(TokenType::slash)
  //   .match_sequence("{").commit(TokenType::i_lbrace)
  //   .match_sequence("}").commit(TokenType::i_rbrace)
  //   .match_sequence("(").commit(TokenType::i_lparen)
  //   .match_sequence(")").commit(TokenType::i_rparen)
  //   .match_sequence("[").commit(TokenType::i_lbrack)
  //   .match_sequence("]").commit(TokenType::i_rbrack);

  // clang-format on

  //
  // complex lexing logic
  //

  // integers:

  regex_table::MutableRegex digit;
  digit.match_digit().terminal();

  regex_table::MutableRegex integer;
  integer
    .match_any_of("123456789")
    .match_many_optionally(digit)
    .terminal()
    .goback()
    .match_any_of("0")
    .terminal()
    .optimize();

  regex_table::MutableRegex floating;
  floating
    .match(integer)
    .match_sequence(".")
    .match_many_optionally(digit)
    .terminal()
    .optimize();
  // integer.print_dbg();
  machine.match(integer).commit(TokenType::l_integer);
  machine.match(floating).commit(TokenType::l_float);

  // machine.optimize();


  machine.print_dbg();
  machine.optimize();
  machine.print_dbg();
  return 0;
}
