#pragma once

#include "state-machine-node.h"
#include <algorithm>
#include <concepts>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace regex_table::build_time {

   //
   // Build a mutable state machine
   //
   // this state machine is quite slow, and memory intensive
   // it is recommended to build this machine at compile time and
   // use the optimize()'d version at runtime
   //
   template <typename Value_T> struct MutableStateMachine {

      template <class U> friend struct MutableStateMachine;

      using Node_T = StateMachineNode<Value_T>;
      using Self   = MutableStateMachine;

      using MutableRegex = MutableStateMachine<void>;

      std::vector<Node_T> m_nodes;

      MutableStateMachine() {
         // root node insertion
         m_nodes.push_back(Node_T());
      }

      std::vector<size_t> m_cursors = {0};

      Node_T& root() {
         return m_nodes[0];
      }

      Self& match_optionally(MutableRegex pattern) {
         // Merge the regex into the current machine,
         // then append all the current cursors to the new
         // cursor list

         auto cursors_before = m_cursors;

         merge_regex_into_machine(pattern);

         for (auto c : cursors_before) {
            m_cursors.push_back(c);
         }

         return *this;
      }

      //
      // Match the given subexpression 1 or more times
      //
      Self& match_many(MutableRegex pattern) {
         // Simply match the pattern, then match_many_optionally

         merge_regex_into_machine(pattern);
         match_many_optionally(pattern);

         return *this;
      }

      //
      // Match the given subexpression 0 or more times
      //
      Self& match_many_optionally(MutableRegex pattern) {

         auto cursors_before = m_cursors;

         // Merge the regex
         merge_regex_into_machine(pattern);

         // Copy the transitions of the first node of the pattern to the last node
         cursors_merge(cursors_before);

         // allow insertion from the old cursors as well

         for (auto c : cursors_before) {
            m_cursors.push_back(c);
         }

         return *this;
      }

      //
      // Write the given value at the current table position
      // then return back to the root
      //
      template <typename Val_T>
      Self& commit(Val_T value)
         requires(std::is_same_v<Value_T, Val_T> && !std::is_same_v<Value_T, void>)
      {

         commit_continue(value);

         m_cursors = {0};

         return *this;
      }

      //
      // Write the given value at the current table position,
      // and continue
      //
      template <typename Val_T>
      Self& commit_continue(Val_T value)
         requires(std::is_same_v<Value_T, Val_T> && !std::is_same_v<Value_T, void>)
      {

         for (auto cur : m_cursors) {
            auto& node = m_nodes[cur];
            node.value = value;
         }

         return *this;
      }

      //
      // Match a sequence of characters exactly
      //
      Self& match_sequence(std::string seq) {

         for (char part : seq) {
            cursor_transition(part);
         }

         return *this;
      }

      //
      // Sets the current cursor position to a terminal state
      //
      Self& terminal()
         requires std::is_same_v<Value_T, void>
      {
         make_cursor_terminal();
         return *this;
      }

      void print_dbg() {

         size_t idx = 0;

         std::string in = " |  ";
         for (Node_T& node : m_nodes) {
            bool is_terminal = node.can_exit();
            bool is_cursor   = std::find(m_cursors.begin(), m_cursors.end(), node_index(node)) != m_cursors.end();

            std::string terminal_msg = "A";
            if (is_terminal) {
               if (std::is_same_v<Value_T, void>) {
                  terminal_msg = "(terminal)";
               } else {
                  terminal_msg = "(terminal val: '" + stringify(node.value.value()) + "' )";
               }
            }

            // bool is_cursor = false;
            // bool is_cursor = node_index(node) == m_cursor_idx;
            std::cout << "#" << idx << " " << (is_terminal ? terminal_msg : "") << " " << (is_cursor ? "[cursor] " : "")
                      << ">>\n";

            char transition = 0;

            for (auto t : node.transitions) {
               if (t != 0) {
                  std::cout << in << stringify_char(transition) << " -> #" << t << "\n";
               }

               transition++;
            }

            std::cout << "\n";
         }
         idx++;
      }

      void optimize() {
         this->compress_gotos();
         this->kill_orphans();
      }

    protected:
      // TODO: Move all of the stringify-related stuff to the mutils library
      //       where it can be properly re-used
      template <typename S, typename T> struct is_streamable {
         template <typename SS, typename TT>
         static auto test(int) -> decltype(std::declval<SS&>() << std::declval<TT>(), std::true_type());

         template <typename, typename> static auto test(...) -> std::false_type;

       public:
         static bool const value = decltype(test<S, T>(0))::value;
      };

      template <typename T>
      static std::string stringify(T val)
         requires std::is_convertible_v<T, std::string>
      {
         return std::string(val);
      }

      template <typename T>
      static std::string stringify(T val)
         requires is_streamable<std::stringstream, T>::value
      {
         std::stringstream ss;
         ss << val;
         return ss.str();
      }

      //
      // Cursor Manipulation
      //

      void for_each_cursor(std::function<void(size_t cursor_idx)> callback) {

         for (size_t i = 0; i < m_cursors.size(); i++) {
            callback(i);
         }
      }

      void make_cursor_terminal()
         requires std::is_same_v<Value_T, void>
      {
         for_each_cursor([this](size_t idx) {
            m_nodes[m_cursors[idx]].terminal = true;
         });
      }

      //
      // Makes the 'child' transition on the current cursors
      // if the transition already exists, we just update the cursor
      //
      //
      // NOTE: This function is not loop-aware
      //
      void cursor_transition(char child) {

         std::vector<size_t> cursors_without_specified_child;
         std::vector<size_t> cursors_with_specified_child;

         for (size_t i = 0; i < m_cursors.size(); i++) {
            auto& node = m_nodes[m_cursors[i]];

            if (node.transitions[child] == 0) {
               cursors_without_specified_child.push_back(i);
            } else {
               cursors_with_specified_child.push_back(i);
            }
         }

         std::vector<size_t> new_cursors;

         if (cursors_without_specified_child.size()) {
            auto& goes_to    = new_node();
            auto goes_to_idx = node_index(goes_to);

            new_cursors.push_back(goes_to_idx);
            // all cursors without the child can safely point to the same node
            // there are no pre-existing nodes to worry about
            for (auto cur : cursors_without_specified_child) {
               m_nodes[m_cursors[cur]].transitions[child] = goes_to_idx;
            }
         }

         // the remaining cursors are overwritten with the index of the already
         // existing child node
         for (auto cur : cursors_with_specified_child) {
            auto new_idx = m_nodes[m_cursors[cur]].transitions[child];

            new_cursors.push_back(new_idx);
         }

         this->m_cursors = new_cursors;
      }

      void cursors_merge(std::vector<size_t> merge) {
         // This function builds what is functionally a clone of another existing
         // node This is accomplished reliably by recursively traversing the
         // transition tree of the referred node and copying it over to the current
         // cursor until we reach a non-ambiguous point
         //
         // warn: This function has the possibility of running infinitely, so ensure
         // that it returns
         //       some sort of error if we copy a node from which we started
         //       (ambiguous regexes) i.e hellohello and (hello)*
         //
         // todo: possibly implement rules for dealing with ambiguity (precedence)
         //
         //

         auto current_cursors   = m_cursors;
         m_cursors              = merge;
         auto merge_transitions = get_cursor_common_transition();

         m_cursors = current_cursors;

         std::vector<char> unresolved_transitions;
         char trans_char = 0;
         for (auto t : merge_transitions) {

            if (cursor_transition_is_free(trans_char)) {
               // Simply copy over the transition, no consequences
               cursor_overwrite_transition(trans_char, t);
            } else {
               // Use the sliding window technique

               // new cursorlist consisting of the transition target
               std::vector<size_t> new_cursors;
               new_cursors.push_back(t);

               cursor_transition(trans_char);

               cursors_merge(new_cursors);
            }

            trans_char++;
         }
      }

      Node_T& new_node() {
         auto n = Node_T();
         m_nodes.push_back(n);
         return m_nodes[m_nodes.size() - 1];
      }

      std::size_t node_index(Node_T& node) {
         auto const addr      = &node;
         auto const base_addr = &root();
         auto const diff      = addr - base_addr;

         return diff;
      }

      std::string stringify_char(char c) {
         if (c <= 31 || c == 127) {
            return "\\" + std::to_string((int)c);
         } else {
            std::string ret;
            ret += "'";
            ret += c;
            ret += "'";
            return ret;
         }
      }

      void merge_regex_into_machine(MutableRegex regex) {

         struct RegexMergeState {
            MutableRegex::Node_T& base_node;
            std::vector<size_t> cursors;

            RegexMergeState(MutableRegex::Node_T& base, std::vector<size_t> cursors) :
                base_node(base), cursors(cursors){};
         };

         std::vector<RegexMergeState> merge_operation_queue;
         std::vector<bool> queued_nodes(regex.m_nodes.size(), false);
         size_t current_op = 0;

         RegexMergeState root_merge(regex.root(), m_cursors);

         merge_operation_queue.push_back(root_merge);
         queued_nodes[0] = true;
         std::vector<size_t> terminal_regex_nodes;

         //
         // Keep going until every reachable node has been processed
         //
         while (current_op != merge_operation_queue.size()) {

            auto current_job = merge_operation_queue[current_op];

            m_cursors = current_job.cursors;

            MutableRegex::Node_T& node = current_job.base_node;

            // if the node is a terminal, all of the currently existing cursors become
            // terminal nodes at the end of this function, the state machine cursors
            // are set to the regex terminals

            if (node.terminal) {
               for (auto c : m_cursors) {
                  terminal_regex_nodes.push_back(c);
               }
            }

            for (size_t idx = 0; idx < node.transitions.size(); idx++) {
               char transition       = idx;
               size_t transitions_to = node.transitions[idx];

               if (transitions_to == 0) {
                  continue;
               }
               queued_nodes.reserve(transitions_to + 1);
               bool has_already_queued_transition = queued_nodes[transitions_to];

               // make sure the transition is not already queued

               if (!has_already_queued_transition) {

                  // Handle looping cursors separately (if any)
                  // The cursor is looped if we can walk from its target
                  // back to the cursor
                  std::vector<size_t> looped_cursors;
                  std::vector<size_t> safe_cursors;
                  for (size_t i = 0; i < m_cursors.size(); i++) {
                     auto cursor = m_cursors[i];

                     bool is_looped = transitions_to < m_nodes.size() && path_is_cycle(transitions_to, cursor, false);
                     bool is_preexisting_transition = m_nodes[cursor].transitions[transition];
                     // its only unsafe if the loop was not deliberate,
                     // i.e the transition already exists

                     if (is_looped && is_preexisting_transition) {
                        looped_cursors.push_back(cursor);
                     } else {
                        safe_cursors.push_back(cursor);
                     }
                  }
                  //
                  m_cursors = safe_cursors;

                  cursor_transition(transition);

                  for (auto cur : looped_cursors) {

                     // all we have to do is copy the cursor that we refer to, ahead of
                     // us these changes automatically propogate, and the end case is
                     // handled by the safe cursor case

                     auto& node                   = m_nodes[cur];
                     auto& copy                   = new_node();
                     copy                         = m_nodes[node.transitions[transition]];
                     node.transitions[transition] = node_index(copy);

                     m_cursors.push_back(node_index(copy));
                  }

                  RegexMergeState job(regex.m_nodes[transitions_to], m_cursors);

                  // add new transition to queue
                  merge_operation_queue.push_back(job);
                  queued_nodes[transitions_to] = true;
               }
            }
            current_op++;
         }

         m_cursors = terminal_regex_nodes;
         std::cout << "----- DONE\n";
      }

      std::array<size_t, 128> get_cursor_common_transition() {
         std::array<size_t, 128> transitions = m_nodes[m_cursors[0]].transitions;

         for (size_t idx = 1; idx < m_cursors.size(); idx++) {
            auto c    = m_cursors[idx];
            auto tzns = m_nodes[c].transitions;

            // iterate over all transitions, remove ones
            // not common to main array
            for (size_t i = 0; i < 128; i++) {
               if (tzns[i] != transitions[i]) {
                  transitions[i] = 0;
               }
            }
         }

         return transitions;
      }

      void cursor_overwrite_transition(char transition, size_t new_tgt) {
         for (auto c : m_cursors) {
            m_nodes[c].transitions[transition] = new_tgt;
         }
      }

      bool cursor_transition_is_free(char transition) {
         for (auto c : m_cursors) {
            if (m_nodes[c].transitions[transition]) {
               return false;
            }
         }
         return true;
      }

      //
      // Cycle detection
      //
      // NOTE: A CYCLE IS ONLY COUNTED WHEN NO CHILD NODES CYCLE BACK TO THE END
      // POINT (excl. the start point)
      //
      // in simpler words, only matches the 'link node' of the longest chain
      // containing said node
      //
      bool path_is_cycle(
          size_t start, size_t end, bool direct, std::vector<size_t> visited_nodes = {}, size_t chain_start = -1) {
         if (chain_start == -1) {
            chain_start = start;
         }

         if (direct && chain_start == start) {
            for (auto tzn : m_nodes[end].transitions) {
               if (tzn != chain_start && path_is_cycle(tzn, chain_start, false)) {
                  return false;
               }
            }
         }

         visited_nodes.push_back(start);

         for (auto transition : m_nodes[start].transitions) {

            // nul transitions can be ignored
            if (!transition) {
               continue;
            }

            // this node has already been visited, (FIXME: This is probably redundant)
            if (std::find(visited_nodes.begin(), visited_nodes.end(), transition) != visited_nodes.end()) {
               continue;
            }
            if (transition == end) {
               return true;
            }
            return path_is_cycle(transition, end, direct, visited_nodes, chain_start);
         }

         return false;
      }

      void remove_duplicates() {
         // This action has to be applied multiple times as nodes have the tendency
         // to form chains which are easily simplifiable
         while (remove_duplicates_once()) {}
      }

      void kill_orphans() {
         // Remove any nodes which have no direct parent
      }

      bool remove_duplicates_once() {
         // We work backwards while removing duplicates, as that is the way in which
         // they are more likely to be positioned

         return false;
      }
   };

   using MutableRegex = MutableStateMachine<void>;

}; // namespace regex_table::build_time
