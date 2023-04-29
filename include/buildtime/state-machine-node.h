#pragma once

#include <array>
#include <iostream>
#include <optional>

namespace regex_table::build_time {

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

  bool operator==(const StateMachineNode& other) const {
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

  void print(){
    std::cout << "transitions:\n";
    size_t idx = 0;
    for(auto t: transitions){
      if(t)std::cout << "'" << (char)idx << "' -> " << t << "\n";
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
};

using RegexNode = StateMachineNode<void>;

}; // namespace regex_table::build_time
