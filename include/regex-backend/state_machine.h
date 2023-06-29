// Copyright (c) 2023 Samir Bioud
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
// OR OTHER DEALINGS IN THE SOFTWARE.
//

#include "./state_machine_internal/builder.h"
#include "./state_machine_internal/node.h"
#include "./util/sets.h"
#include <cstdint>

namespace regex_backend {

template <typename Value_T,
          typename Transition_T,
          std::size_t STATIC_NODE_COUNT       = 0,
          bool TRANSITION_NONTRIVIAL_EQUALITY = true>
class StateMachine :
    public internal::StateMachine<
        Value_T,
        Transition_T,
        StateMachine<Value_T, Transition_T, STATIC_NODE_COUNT, TRANSITION_NONTRIVIAL_EQUALITY>,
        STATIC_NODE_COUNT,
        TRANSITION_NONTRIVIAL_EQUALITY> {};

///
/// Specialization of the StateMachine for char- transitions, which provides many useful matches for various char ranges
///
template <typename Value_T, std::size_t STATIC_NODE_COUNT, bool TRANSITION_NONTRIVIAL_EQUALITY>
class StateMachine<Value_T, char, STATIC_NODE_COUNT, TRANSITION_NONTRIVIAL_EQUALITY> :
    public internal::StateMachine<Value_T,
                                  char,
                                  StateMachine<Value_T, char, STATIC_NODE_COUNT, TRANSITION_NONTRIVIAL_EQUALITY>,
                                  STATIC_NODE_COUNT,
                                  TRANSITION_NONTRIVIAL_EQUALITY> {
    using Parent = internal::StateMachine<Value_T,
                                  char,
                                  StateMachine<Value_T, char, STATIC_NODE_COUNT, TRANSITION_NONTRIVIAL_EQUALITY>,
                                  STATIC_NODE_COUNT,
                                  TRANSITION_NONTRIVIAL_EQUALITY>;
public:
  StateMachine& match_any_of(std::string const& options) {
    return match_any_of(options.c_str());
  }

  StateMachine& match_any_of(char const* options) {
    std::vector<char> v;
    for (char const* o = options; *o != 0; o++) {
      v.push_back(*o);
    }
      Parent::match_any_of(v);
    return *this;
  }

  ///
  /// Matches visual whitespace characters
  /// defined at https://en.wikipedia.org/wiki/Whitespace_character
  ///
  StateMachine& match_whitespace() {
    return match_any_of("\011\012\013\014\015\040");
  }

  ///
  /// Matches any control characters
  /// control characters are those outside of ascii range [ 33 - 127 ], which are not whitespace
  ///
  StateMachine& match_control() {
    return match_any_of(
        "\001"
        "\002"
        "\003"
        "\004"
        "\005"
        "\006"
        "\007"
        "\010"
        "\016"
        "\017"
        "\020"
        "\021"
        "\022"
        "\023"
        "\024"
        "\025"
        "\026"
        "\027"
        "\030"
        "\031"
        "\032"
        "\033"
        "\034"
        "\035"
        "\036"
        "\037"
        "\038"
        "\039"
        "\177");
  }

  ///
  /// Match any uppercase ASCII character [A-Z]
  ///
  StateMachine& match_uppercase() {
    return match_any_of(internal::charsets::ALPHABET_UPPER);
  }

  ///
  /// Match any lowercase ASCII character [A-Z]
  ///
  StateMachine& match_lowercase() {
    return match_any_of(internal::charsets::ALPHABET_LOWER);
  }

  ///
  /// Match any ASCII characters [A-Za-z]
  ///
  StateMachine& match_alpha() {
    return match_any_of(internal::charsets::ALPHABET_FULL);
  }

  ///
  /// Match any ASCII digits [0-9]
  ///
  StateMachine& match_digit() {
    return match_any_of(internal::charsets::DIGITS);
  }
};

///
/// A state machine that operates on utf8 based input
///
/// essentially encodes utf8 chars as a series of simple transitions, but also
/// introduces logic for dealing with illegal utf8, and stores it in a slightly more compact format
///
template <typename Value_T, std::size_t STATIC_NODE_COUNT, bool TRANSITION_NONTRIVIAL_EQUALITY>
class StateMachine<Value_T, char32_t, STATIC_NODE_COUNT, TRANSITION_NONTRIVIAL_EQUALITY> :
    public internal::StateMachine<Value_T,
                                  char32_t,
                                  StateMachine<Value_T, char32_t, STATIC_NODE_COUNT, TRANSITION_NONTRIVIAL_EQUALITY>,
                                  STATIC_NODE_COUNT,
                                  TRANSITION_NONTRIVIAL_EQUALITY> {
private:

    using Parent = internal::StateMachine<Value_T,
                                  char32_t,
                                  StateMachine<Value_T, char32_t, STATIC_NODE_COUNT, TRANSITION_NONTRIVIAL_EQUALITY>,
                                  STATIC_NODE_COUNT,
                                  TRANSITION_NONTRIVIAL_EQUALITY>;
  std::vector<char32_t> split_str_as_utf_points(std::string const& s) {
    std::vector<char32_t> data;

    uint32_t current = 0;
    uint8_t len      = 0;
    for (unsigned char c : s) {
      if (c & 0b10000000) {
        auto const ones = std::countl_one(c);
        switch (ones) {
          case 1: // segment
          {
            MUTILS_ASSERT_NEQ(len, 0, "An invalid utf8 code sequence was detected / dangling segment");
            auto const shift = (len - 1) * 8;

            current |= (c << shift);
            len--;
            if (len == 0) {
              // a full codepoint has been created
              data.push_back(current);
              current = 0;
              len     = 0;
            }
            break;
          }

          case 2: // double width
            len = 1;
            current |= (c << 8);
            break;
          case 3: // triple width
            len = 2;
            current |= (c << 16);
            break;
          case 4: // quad width
            len = 3;
            current |= (c << 24);
            break;
        }
      } else {
        MUTILS_ASSERT_EQ(len, 0, "An invalid utf8 code sequence was detected / unfinished segment");
        data.push_back(c);
      }
    }
    MUTILS_ASSERT_EQ(len, 0, "An invalid utf8 code sequence was detected / end before final segment(s)");

    return data;
  }

public:
  StateMachine& match_any_of(std::string const& options) {
    std::vector<char32_t> codepoints = split_str_as_utf_points(options);
      Parent::match_any_of(codepoints);
    return *this;
  }

  ///
  /// Matches visual whitespace characters
  /// defined at https://en.wikipedia.org/wiki/Whitespace_character
  ///
  StateMachine& match_whitespace() {
    return this->match_any_of("\011\012\013\014\015\040");
  }

  ///
  /// Matches any control characters
  /// control characters are those outside of ascii range [ 33 - 127 ], which are not whitespace
  ///
  StateMachine& match_control() {
    return this->match_any_of(
        "\001"
        "\002"
        "\003"
        "\004"
        "\005"
        "\006"
        "\007"
        "\010"
        "\016"
        "\017"
        "\020"
        "\021"
        "\022"
        "\023"
        "\024"
        "\025"
        "\026"
        "\027"
        "\030"
        "\031"
        "\032"
        "\033"
        "\034"
        "\035"
        "\036"
        "\037"
        "\038"
        "\039"
        "\177");
  }

  ///
  /// Match any uppercase ASCII character [A-Z]
  ///
  StateMachine& match_uppercase() {
    return this->match_any_of(internal::charsets::ALPHABET_UPPER);
  }

  ///
  /// Match any lowercase ASCII character [A-Z]
  ///
  StateMachine& match_lowercase() {
    return this->match_any_of(internal::charsets::ALPHABET_LOWER);
  }

  ///
  /// Match any ASCII characters [A-Za-z]
  ///
  StateMachine& match_alpha() {
    return this->match_any_of(internal::charsets::ALPHABET_FULL);
  }

  ///
  /// Match any ASCII digits [0-9]
  ///
  StateMachine& match_digit() {
    return this->match_any_of(internal::charsets::DIGITS);
  }
};
}; // namespace regex_backend
