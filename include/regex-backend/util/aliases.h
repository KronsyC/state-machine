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

//
// A collection of useful type aliases used to give semantic meaning to various types used
// throughout the library, and hopefully reduce overall mental overhead
//
//


#pragma once

#include <array>

namespace regex_backend::internal::aliases {

/**
 * A transition set represents the set of transitions which can be made by a node
 * Transitions 0 -> 127 represent their corresponding ascii characters
 * Transition 128 represents the EOF character, which is also aliased by NULL in the case of c-string matching
 */
template <typename Referred_T> using TransitionSet = std::array<Referred_T, 129>;


// WIP
// struct TransitionMetadata {
//   bool consume_char  : 1 = true;
//
//   /**
//   * Indicate to the state machine that it should begin eating characters into the next
//   * of 16 capture group variables, terminates the previous capture if applicable
//   */
//   bool start_capture : 1 = false;
//   bool end_capture   : 1 = false;
// };

}; // namespace regex_backend::internal::aliases
