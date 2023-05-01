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


#include "../include/builder.h"
using namespace regex_table;

int main() {

  //
  // This regex state machine reads integer literals
  //

  MutableRegex digit;
  digit.match_digit().terminal();

  // clang-format off

  MutableRegex integer;
  integer
      .match_any_of("123456789")
      .match_many_optionally(digit)
      .terminal()
      .goback()
      .match_any_of("0")
      .terminal()
      .optimize();

  // integer.print_dbg();
  // return 0;
  //
  // This regex state machine reads floating point literals
  //
  MutableRegex floatingpoint;
  floatingpoint
    .match(integer)
    .match_any_of(".")
    .match_many_optionally(digit)
    .terminal()
    .optimize();

  // floatingpoint.expand();
  floatingpoint.print_dbg();
  return 0;
}
