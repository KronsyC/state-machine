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

#pragma once

#include "./builder.h"


/**
 *
 * this file provides a few preset regular expressions
 * which are commonly used
 *
 */
namespace regex_backend::presets {

/**
 * Match any singular digit
 */
inline MutableRegex digit = []() {
  MutableRegex rg;
  rg.match_digit().terminal().optimize();
  return rg;
}();

/**
 * Match any c-like integer
 *
 * leading zeroes are considered illegal
 */
inline MutableRegex integer = []() {


  MutableRegex rg;

  // clang-format off
  rg
    .match_any_of("123456789")
    .match_many_optionally(digit)
    .terminal()
    .goback()
    .match_any_of("0")
    .terminal()
    .optimize()
    ;
  // clang-format on
  return rg;
}();

/**
 * Match any c-like integer
 *
 * leading zeroes are allowed
 */
inline MutableRegex zeroprefixable_integer = []() {
  MutableRegex rg;

  rg.match_many(digit).terminal().optimize();
  return rg;
}();

/**
 * Match basic 'sane' identifiers
 *
 * these identifiers can contain letters, numbers
 * and underscores, but may not start with a number
 *
  */
inline MutableRegex simple_identifier = []() {
  MutableRegex rg;

  MutableRegex first_char;
  first_char.match_alpha().terminal().goback().match_any_of("_").terminal().optimize();
  //
  MutableRegex other_chars;
  // first_char.print_dbg();
  // clang-format off
  other_chars
    .match(first_char)
    .terminal()
    .goback()
    .match_digit()
    .terminal()
    .optimize();

  // other_chars.print_dbg();
  // clang-format on
  //
  first_char.optimize();
  //
  //
  rg.match(first_char);
  rg.match_many_optionally(other_chars);
  rg.terminal();
  rg.optimize();
  return rg;
}();

inline MutableRegex c_like_comment = [](){

  MutableRegex chr;
  chr.match_default().terminal().optimize();

  MutableRegex end;
  end.match_eof().terminal().goback().match_any_of("\n").terminal().optimize();

  MutableRegex rg;
  rg.match_sequence("//");
  rg.match_many_optionally(chr);
  rg.match(end);
  rg.terminal();
  return rg;
}();

}; // namespace regex_backend::presets
