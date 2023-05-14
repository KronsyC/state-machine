#include "regex-backend/presets.h"
#include "regex-backend/builder.h"
#include "gtest/gtest.h"
#include "malloc.h"
TEST(presets, numbers) {

  auto& match_int = regex_backend::presets::integer;

  ASSERT_TRUE(match_int.matches("1")) << "Matches single digit integer";
  ASSERT_TRUE(match_int.matches("12")) << "Matches double digit integers";
  ASSERT_TRUE(match_int.matches("1234567901223456778000")) << "Matches large integers";

  ASSERT_TRUE(match_int.matches("0")) << "Matches zero";
  ASSERT_FALSE(match_int.matches("00")) << "Doesnt match double-zero";
  ASSERT_FALSE(match_int.matches("0123456")) << "Does not match any zero-prefixed numbers";
}

TEST(presets, simple_identifiers) {
  auto& match_id = regex_backend::presets::simple_identifier;

  ASSERT_TRUE(match_id.matches("foo")) << "Matches a simple variable name";
  ASSERT_TRUE(match_id.matches("foo_bar")) << "Matches a snake_case variable name";
  ASSERT_TRUE(match_id.matches("foobar12")) << "Allows digits";

  ASSERT_FALSE(match_id.matches("1foo_bar")) << "Disallows names beginning with numbers";
}

TEST(presets, c_like_comments) {
  auto& match_comment = regex_backend::presets::c_like_comment;
  ASSERT_TRUE(match_comment.matches("// Hello, World!\n")) << "Matches basic comment";
  ASSERT_TRUE(match_comment.matches<true>("// Hello, World")) << "Matches EOF comments";
  ASSERT_TRUE(match_comment.matches("//\n")) << "Matches blank comments";

  ASSERT_FALSE(match_comment.matches("// Hello, World")) << "Does not match unterminated comments";
}

int main() {
  mallopt(M_PERTURB, 103);
  testing::InitGoogleTest();
  return RUN_ALL_TESTS();
}
