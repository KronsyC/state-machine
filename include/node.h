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

#include <array>
#include <iostream>
#include <optional>

namespace regex_table{

struct StateMachineNodeBase {

  std::array<std::size_t, 128> transitions;

  //
  // Some nodes may not want to consume characters
  // This is useful for the '\' regex character
  //
  bool consume_char = true;
};

//
// A state machine consists of a graph of nodes
//
template <typename T> struct StateMachineNode : StateMachineNodeBase {

  //
  // The optional value that the node may carry
  // this value is returned from the lookup functions
  //
  // this regex implementation is eager
  // therefore, we take the value as far into the graph as
  // possible
  //
  std::optional<T> value;

  bool can_exit() const {

    return value.has_value();
  }

  bool operator==(StateMachineNode const& other) const {
    return transitions == other.transitions && consume_char == other.consume_char && value == other.value;
  }

  void nullify() {
    value        = std::optional<T>{};
    consume_char = true;
    transitions.fill(0);
  }

  bool is_null() {
    if (value.has_value()) {
      return false;
    }
    if (!consume_char) {
      return false;
    }
    for (auto t : transitions) {
      if (t != 0) {
        return false;
      }
    }
    return true;
  }

  void print() {
    std::cout << "transitions:\n";
    size_t idx = 0;
    for (auto t : transitions) {
      if (t) {
        std::cout << "'" << (char)idx << "' -> " << t << "\n";
      }
      idx++;
    }
  }
};

//
// A pure regex node is equivalent to the specialization
// StateMachineNode<void>
//
template <> struct StateMachineNode<void> : StateMachineNodeBase {

  //
  // This value represents if the expression is allowed
  // to terminate on this node, or not
  //
  bool terminal = false;

  bool can_exit() const {
    return terminal;
  }


  bool is_null() const{
    if(terminal) return false;
    for(auto t : transitions){
      if(t) return false;
    }

    return true;
  }
  void nullify() {
    transitions.fill(0);
    terminal = false;
  }

  bool operator==(StateMachineNode<void> const other )const{
    return terminal == other.terminal && transitions == other.transitions;
  }
};

using RegexNode = StateMachineNode<void>;

}; // namespace regex_table::build_time
