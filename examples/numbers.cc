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

void print_range(const char* begin, const char* end){
  for(auto c = begin; c <= end; c++){
    std::cout << *c;
  }
  std::cout << "\n";
}

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

  floatingpoint.print_dbg();
  auto result = floatingpoint.find_many("hello world this is some random aaah text avnaobnfaw 123456 << these number should not be detected but these next ones should 1234.567 by the way have you heard of this new game called 'Raid, Shadow Legends' it has a rating of 3.7 stars on google play");

  for(auto r : result){
    std::cout << "Extracted float : ";
    print_range(r.begin, r.end);
  }

  const char* lookup = "123.456abcde123.4";
  auto r = floatingpoint.lookup(lookup);

  std::cout << "Lookup found: ";
  print_range(lookup, r.end);



  return 0;
}
