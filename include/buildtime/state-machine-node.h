#pragma once

#include <array>
#include <optional>

namespace regex_table::build_time {

   struct StateMachineNodeBase {

      std::array<std::size_t, 128> transitions;

      //
      // Some nodes may not want to consume characters
      // This is useful for the '\' regex character
      //
      bool consume_char;
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
