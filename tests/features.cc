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

#include "regex-backend/builder.h"
#include "regex-backend/state_machine.h"
#include <gtest/gtest.h>

using namespace regex_backend;

TEST(features, equivalence) {
  MutableRegex regex1;
  MutableRegex regex2;


  // clang-format off

  regex1
    .match_sequence("ABC").terminal().goback()
    .match_sequence("DEF").terminal().goback()
    .match_sequence("GHI").terminal().goback()
    .match_sequence("DEFABC").terminal().goback()
    .optimize();

  regex2
    .match_sequence("DEFABC").terminal().goback()
    .match_sequence("GHI").terminal().goback()
    .match_sequence("ABC").terminal().goback()
    .match_sequence("DEF").terminal().goback()
    .optimize();

  // clang-format on

  ASSERT_EQ(regex1, regex2) << "Two regexes with the same transitions declared in different orders are equivalent";
}

TEST(features, match_sequence) {
  MutableRegex regex;

  // Push multiple sequences and test some edge cases

  // clang-format off
  regex
    .match_sequence("foo").terminal().goback()
    .match_sequence("foobar").terminal().goback()
    .match_sequence("foobarbaz").terminal().goback()
    .match_sequence("foobaz").terminal().goback()
    .match_sequence("foobazbaz").terminal().goback()
    .match_sequence("barbaz").terminal().goback()
    .match_sequence("baz").terminal().goback()
    .match_sequence("").terminal().goback()
    .optimize();

  // clang-format on

  ASSERT_TRUE(regex.matches("foo")) << "correctly matches";
  ASSERT_TRUE(regex.matches("foobar")) << "correctly matches";
  ASSERT_TRUE(regex.matches("foobarbaz")) << "correctly matches";
  ASSERT_TRUE(regex.matches("foobaz")) << "correctly matches";
  ASSERT_TRUE(regex.matches("foobazbaz")) << "correctly matches";
  ASSERT_TRUE(regex.matches("barbaz")) << "correctly matches";
  ASSERT_TRUE(regex.matches("baz")) << "correctly matches";
  ASSERT_TRUE(regex.matches("")) << "correctly matches empty string";
}

TEST(features, match_optional) {
  MutableRegex regex;

  MutableRegex foobar;
  foobar.match_sequence("foobar").terminal();

  regex.match_sequence("ABCDEF").match_optionally(foobar).terminal();

  ASSERT_TRUE(regex.matches("ABCDEF")) << "matches the unbranched optional path";
  ASSERT_TRUE(regex.matches("ABCDEFfoobar")) << "matches the branched optional path";

  ASSERT_FALSE(regex.matches("")) << "does not match a null string";

  ASSERT_FALSE(regex.matches("ABCDEFG")) << "Does not match with an additional character";
  ASSERT_FALSE(regex.matches("ABCDEFfoo")) << "Does not match with a partial optional";
  ASSERT_FALSE(regex.matches("ABCD")) << "Does not match with a substring";
}

TEST(features, match_many_optional) {
  MutableRegex regex;

  regex.match_sequence("abc").terminal();

  MutableRegex test;
  test.match_sequence("alphabet.").match_many_optionally(regex).match_sequence(".done").terminal();


  ASSERT_TRUE(test.matches("alphabet.abc.done")) << "Matches once";
  ASSERT_TRUE(test.matches("alphabet.abcabc.done")) << "Matches twice";
  ASSERT_TRUE(test.matches("alphabet..done")) << "Matches none";

  ASSERT_FALSE(test.matches("alphabet.alphabet.done")) << "Does not match non-conforming string\n";

}

int main() {
  testing::InitGoogleTest();
  return RUN_ALL_TESTS();
}
