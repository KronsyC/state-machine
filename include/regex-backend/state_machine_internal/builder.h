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


#pragma once

#include "./node.h"
#include "./node_store.h"
#include "mutils/assert.h"
#include "mutils/stringify.h"
#include <algorithm>
#include <concepts>
#include <type_traits>
#include <vector>

namespace regex_backend::internal {

enum ConflictAction {
  Skip,
  Overwrite,
  Error
};

///
/// Here we hold state exclusive to constructible / dynamically allocated state machines
///
template <bool IS_DYNAMIC> struct StateMachineConstructionState {
  ConflictAction on_conflict = ConflictAction::Error;
  std::vector<size_t> cursors;
};

template <> struct StateMachineConstructionState<false> {};

template <typename Value_T,                  // Type of values held at regex lookups
          typename Transition_T,             // Type which transitions are based on
          typename Self,                     // class inheriting the statemachine
          std::size_t STATIC_NODE_COUNT = 0, // The number of internal nodes when allocating statically, set to 0 for
                                             // dynamic allocation or to any integer for static allocation
          bool TRANSITION_NONTRIVIAL_EQUALITY = true // Set to true if the transition type cannot be compared bytewise
          >
class StateMachine {

  // static_assert(std::is_base_of_v<StateMachine, Self>, "Self arg must extend StateMachine");
  // using Concrete_Transition_T = std::conditional_t<std::is_same_v<Transition_T, utf8char>, char, Transition_T>;

  // Is there a value type defined?
  // If not, this class operates as a plain regex builder
  // otherwise, this class also allows object lookup functionality
  static constexpr bool HAS_VALUE       = !std::is_void_v<Value_T>;
  static constexpr bool IS_REGEX        = !HAS_VALUE;
  static constexpr bool IS_PREALLOCATED = STATIC_NODE_COUNT != 0;
  static constexpr bool IS_DYNAMIC      = !IS_PREALLOCATED;
  static constexpr bool IS_UTF8         = std::is_same_v<Transition_T, char32_t>;

  using Node_T = StateMachineNode<Value_T, Transition_T, IS_DYNAMIC>;
  // using Self   = StateMachine;

  StateMachineNodeStore<Node_T, STATIC_NODE_COUNT> m_nodes;

  StateMachineConstructionState<!IS_PREALLOCATED> construction_state;

public:
  using MutableRegex = StateMachine<void, Transition_T, Self, 0, TRANSITION_NONTRIVIAL_EQUALITY>;
  // using Self_T = Self;
  typedef Self Self_T;

  ///
  /// Construct a static state-machine from a pre-existing dynamic one
  ///
  /// Note: You must know the size of the dynamic state machine to construct this
  constexpr StateMachine(StateMachine<Value_T, Transition_T, Self, 0, TRANSITION_NONTRIVIAL_EQUALITY> from)
    requires(IS_PREALLOCATED)
  {
    MUTILS_ASSERT_EQ(
        from.m_nodes.size(),
        STATIC_NODE_COUNT,
        "You may only construct a static state machine from a dynamic one if they are both of equal lengths");
    // TODO: Trivial cloning code
  };

  ///
  /// Construct a dynamic state-machine
  ///
  constexpr StateMachine()
    requires IS_DYNAMIC
  {
    Node_T root;
    m_nodes.push(root);
    construction_state.cursors = {1};
  };

  Self& root()
    requires IS_DYNAMIC
  {
    construction_state.cursors = {1};
    return *(Self*)this;
  }

  Self& conflict(ConflictAction ca)
    requires IS_DYNAMIC
  {
    construction_state.on_conflict = ca;
    return *(Self*)this;
  }

  Self& match_default()
    requires IS_DYNAMIC
  {
    auto& default_node              = new_node();
    auto default_node_idx           = node_index(default_node);
    std::vector<size_t> new_cursors = {default_node_idx};
    std::vector<std::string> errors;
    for (auto cursor : construction_state.cursors) {
      auto& deflt = get_node(cursor).def();

      if (deflt == 0) {
        deflt = default_node_idx;
      } else {
        switch (construction_state.on_conflict) {
          case ConflictAction::Skip: new_cursors.push_back(deflt); continue;
          case ConflictAction::Overwrite: deflt = default_node_idx; break;
          case ConflictAction::Error:

            errors.push_back("In node #" + std::to_string(cursor) + ", the extisting default value of " +
                             std::to_string(deflt) + " was attempted to be replaced with " +
                             std::to_string(default_node_idx));
            break;
        }
      }
    }
    if (errors.size()) {
      std::string msg = "An error was encountered while generating an exit-point to a regex state machine\n";
      for (auto e : errors) {
        msg += e + "\n";
      }
      msg += "\nTo solve these errors, either make non-ambiguous state machines, or update the conflict behavior";
      mutils::PANIC(msg);
    }
    construction_state.cursors = new_cursors;
    return *(Self*)this;
  };

  Self& match_eof()
    requires IS_DYNAMIC
  {
    cursor_transition(Node_T::Key_T::eof());
    return *(Self*)this;
  };

  Self& match_sequence(std::vector<Transition_T> seq)
    requires IS_DYNAMIC
  {
    for (auto part : seq) {
      cursor_transition(Node_T::Key_T::value(part));
    }
    return *(Self*)this;
  }

  Self& match_any_of(std::vector<Transition_T> options)
    requires IS_DYNAMIC
  {
    auto& target = new_node();
    auto tidx    = node_index(target);
    std::vector<size_t> new_cursors;
    auto initial_cursors = construction_state.cursors;
    for (auto choice : options) {

      if constexpr (IS_UTF8) {

        constexpr char32_t byte = 0xFF;
        // constexpr char32_t byte_msb = 128;

        // constexpr char32_t byte_dropbit = byte_msb >> 1;
        constexpr char32_t drop_mask = 0b10111111;


        char32_t key = choice;
        // the key is treated as an array of 4 utf8 bytes

        if (key & (byte << 24)) {
          // 4-wide utf8 char
          std::cout << "QUAD\n";
          cursor_discreet_transition(Node_T::Key_T::value((key >> 24) & drop_mask));
          cursor_discreet_transition(Node_T::Key_T::value((key >> 16) & drop_mask));
          cursor_discreet_transition(Node_T::Key_T::value((key >> 8) & drop_mask));
          cursor_discreet_transition(Node_T::Key_T::value(key & drop_mask));

        } else if (key & (byte << 16)) {
          // 3-wide utf8 char
          std::cout << "TRIPLE\n";
          cursor_discreet_transition(Node_T::Key_T::value((key >> 16) & drop_mask));
          cursor_discreet_transition(Node_T::Key_T::value((key >> 8) & drop_mask));
          cursor_discreet_transition(Node_T::Key_T::value(key & drop_mask));
        } else if (key & (byte << 8)) {
          // 2-wide utf8 char
          std::cout << "DOUBLE\n";
          cursor_discreet_transition(Node_T::Key_T::value((key >> 8) & drop_mask));
          cursor_discreet_transition(Node_T::Key_T::value(key & drop_mask));
        } else {
          // 1-wide utf8 char  / treat as regular ascii character
          MUTILS_ASSERT((key & 128) == 0, "The MSB was found to be set in an ascii character");
          cursor_discreet_transition(Node_T::Key_T::value(key));
        }
      } else {

        cursor_discreet_transition(Node_T::Key_T::value(choice));
      }
      // gather all the new cursors
      std::for_each(construction_state.cursors.begin(), construction_state.cursors.end(), [&](auto c) {
        new_cursors.push_back(c);
      });
      construction_state.cursors = initial_cursors;
    }
    construction_state.cursors = new_cursors;
    // for (auto choice : options) {
    // transition, and update cursors
    // cursor_transition(Node_T::Key_T::value(choice));
    //
    // make_nonambiguous_link(, typename Node_T::Key_T transition, size_t to, std::vector<size_t> watch_nodes)
    // for (auto c : construction_state.cursors) {
    // new_cursors.push_back(c);
    // }
    return *(Self*)this;
  };

  Self& match(MutableRegex pattern)
    requires IS_DYNAMIC
  {
    merge_regex_into_machine(pattern);
    return *(Self*)this;
  };

  Self& match_many(MutableRegex& pattern) {
    return match(pattern).match_many_optionally(pattern);
  }

  Self& match_many_optionally(MutableRegex pattern)
    requires IS_DYNAMIC
  {
    auto cursors_before = construction_state.cursors;

    auto res             = consume_regex_except_root(pattern);
    Node_T& pattern_root = pattern.m_nodes[0];

    pattern_root.each_transition([&](auto key, auto& old_transition) {
      auto new_transition = res.mappings[old_transition];


      //
      // Transform the newly written regex into a cycle
      // this is done by treating all of the terminals as the original root
      // referring back into the graph
      for (auto terminal : res.terminals) {
        // std::cout << "create transition: " << terminal << " -> " << new_transition << "\n";
        make_nonambiguous_link(terminal, key, new_transition, {});
      }
    });

    pattern_root.each_transition([&](auto key, auto& old_transition) {
      auto new_transition = res.mappings[old_transition];
      //
      // write the transitions into the cycle to make it accessible
      //
      for (auto terminal : cursors_before) {
        make_nonambiguous_link(terminal, key, new_transition, {});
      }
    });


    // finally, we preserve the original cursors
    construction_state.cursors = cursors_before;
    for (auto c : res.terminals) {
      construction_state.cursors.push_back(c);
    }

    return *(Self*)this;
  }

  ///
  /// Set an exit point for a regex state machine
  ///
  /// you may optionally provide the 'back_by' parameter
  ///
  /// with this parameter, all non-fullmatching match methods will
  /// not consume the final n elements of the input
  ///
  /// this allows for things like conditional matching depending on tokens after the primary match,
  /// while keeping the context tokens for later matches
  ///
  Self& exit_point(size_t back_by = 0)
    requires(IS_REGEX && IS_DYNAMIC)
  {
    std::vector<std::string> errors;
    for (auto cur : construction_state.cursors) {
      Node_T& node = get_node(cur);

      if (node.value.has_value()) {
        auto& v = node.value.value();
        if (v.back_by != back_by) {
          // Collision
          switch (construction_state.on_conflict) {
            case ConflictAction::Skip: continue;
            case ConflictAction::Overwrite: v.back_by = back_by; continue;
            case ConflictAction::Error: {
              errors.push_back("In node #" + std::to_string(cur) + ", the extisting back_by value of " +
                               std::to_string(v.back_by) + " was attempted to be replaced with " +
                               std::to_string(back_by));
              continue;
            }
          }
        }

      } else {
        typename Node_T::Value_T v;
        v.back_by  = back_by;
        node.value = v;
      }
    }


    // Error alerting
    if (errors.size()) {
      std::string msg = "An error was encountered while generating an exit-point to a regex state machine\n";
      for (auto e : errors) {
        msg += e + "\n";
      }
      msg += "\nTo solve these errors, either make non-ambiguous state machines, or update the conflict behavior";
      mutils::PANIC(msg);
    }

    return *(Self*)this;
  };

  /**
   * Dump a textual representation of the state machine to
   * stdout
   */
  void print_dbg()
    requires IS_DYNAMIC
  {
    size_t idx = 0;

    std::string in = " |  ";
    for (Node_T& node : m_nodes) {
      bool is_terminal = node.value.has_value();
      bool is_cursor =
          std::find(construction_state.cursors.begin(), construction_state.cursors.end(), node_index(node)) !=
          construction_state.cursors.end();

      std::string terminal_msg = "A";
      if (is_terminal) {
        if constexpr (IS_REGEX) {
          terminal_msg = "(terminal)";
        } else {
          terminal_msg = "(terminal val: '" + mutils::stringify(node.value.value()) + "' )";
        }
      }

      std::cout << "#" << idx + 1 << " " << (is_terminal ? terminal_msg : "") << " " << (is_cursor ? "[cursor] " : "")
                << (node.is_null() ? "NULL " : "") << ">>\n";


      node.each_transition([&](auto key, auto& v) {
        std::cout << in << "'" << mutils::stringify(key) << "' -> #" << v << "\n";
      });
      std::cout << "\n";
      idx++;
    }
  }

  Self_T& optimize()
    requires IS_DYNAMIC
  {
    nullify_nullrefs();
    remove_duplicates();
    nullify_nullrefs();
    remove_duplicates();
    nullify_orphans();
    remove_blanks();
    // construction_state.cursors = {1};

    return *(Self_T*)this;
  }


protected:
  ///
  /// Traverses the entire node chain and converts any transitions to null nodes into
  /// null transitions, this nullification bubbles up
  /// all the way to the root
  ///
  void nullify_nullrefs() {
    std::vector<bool> nulls(m_nodes.size(), false);

    size_t i = 0;
    for (Node_T& n : m_nodes) {
      if (is_deletable_node(i + 1)) {
        nulls[i] = true;
      }
      i++;
    }


    while (true) {
      bool has_nulled = false;

      for (Node_T& n : m_nodes) {
        if (nulls[node_index(n) - 1]) {
          continue;
        }
        n.each_transition([&](auto k, auto& v) {
          if (nulls[v - 1]) {
            v = 0;
          }
        });

        if (is_deletable_node(node_index(n))) {
          has_nulled               = true;
          nulls[node_index(n) - 1] = true;
        }
      }

      if (!has_nulled) {
        break;
      }
    }
  }

  void remove_duplicates() {
    // This action has to be applied multiple times as nodes have the tendency
    // to form chains which are easily simplifiable
    while (remove_duplicates_once()) {}
  }

  bool remove_duplicates_once() {
    bool has_removed_dup = false;

    std::vector<bool> cursors(m_nodes.size(), false);

    for (auto c : construction_state.cursors) {
      cursors[c - 1] = true;
    }
    // reverse iterate over every node excluding the root
    for (auto noderef = m_nodes.rbegin(); noderef < m_nodes.rend() - 1; noderef++) {

      Node_T& node  = *noderef;
      auto node_idx = node_index(node);
      // skip pure nulls (nulls without cursors on them)
      if (node.is_null() && !cursors[node_idx - 1]) {
        continue;
      }


      std::vector<size_t> matchers;
      // check every other node
      std::for_each(m_nodes.begin() + 1, noderef.base() - 1, [&](Node_T& other) {
        auto other_idx = node_index(other);

        if (other.is_null() && !cursors[other_idx - 1]) {
          return;
        }

        if (cursors[other_idx - 1] != cursors[node_idx - 1]) {
          // both dont have the same cursor state
          return;
        }
        if (node.value != other.value) {
          return;
        }
        // We also consider nodes to be equal if transitions are self-referring
        bool equal = true;
        node.each_transition([&](auto k, auto& node_tzn) {
          if (!equal) {
            return;
          }
          auto other_tzn = other.transition(k);

          const bool node_tzn_is_self_referencing  = node_tzn == node_idx;
          const bool other_tzn_is_self_referencing = other_tzn == other_idx;
          bool both_are_self_referencing           = node_tzn_is_self_referencing && other_tzn_is_self_referencing;

          if (both_are_self_referencing) {
            // equality holds if they both refer to themselves
            return;
          } else if (node_tzn == other_tzn) {
            // equality still holds when they both refer to the same node
          } else {
            // they are not equal
            equal = false;
          }
        });
        if (equal) {
          matchers.push_back(other_idx);
        } else {
        }
      });

      // not equal to end iterator (i.e exists)
      if (matchers.size()) {

        has_removed_dup = true;

        for (auto old_idx : matchers) {
          auto new_idx = node_index(node);
          for (Node_T& n : m_nodes) {
            n.each_transition([&](auto k, auto& v) {
              if (v == old_idx) {
                v = new_idx;
              }
            });
          }
          get_node(old_idx).nullify();
          cursors[old_idx - 1] = false;
          // std::cout << "DELETING: " << old_idx << "\n";
        }
      }
    }

    construction_state.cursors = {};
    size_t i                   = 1;
    for (bool c : cursors) {
      if (c) {
        construction_state.cursors.push_back(i);
      }
      i++;
    }
    return has_removed_dup;
  }

  ///
  /// Traverses the tree and marks any unreachable nodes as null
  ///
  void nullify_orphans() {
    std::vector<bool> reachables(m_nodes.size(), false);
    reachables[0] = true; // root node is always reachable

    while (true) {
      bool has_expanded = false;
      for (Node_T& n : m_nodes) {
        if (reachables[node_index(n) - 1]) {
          n.each_transition([&](auto k, auto& t) {
            const size_t corrected_transition = t - 1;
            if (reachables[corrected_transition]) {
              return;
            }
            reachables[corrected_transition] = true;
            has_expanded                     = true;
          });
        }
      }
      if (!has_expanded) {
        break;
      }
    }

    for (auto& c : construction_state.cursors) {
      if (!reachables[c]) {
        c = 0;
      }
    }
    for (auto i = 0; i < m_nodes.size(); i++) {
      if (!reachables[i]) {
        m_nodes[i].nullify();
      }
    }

    std::vector<size_t> new_cursors;
    std::copy_if(construction_state.cursors.begin(),
                 construction_state.cursors.end(),
                 std::back_inserter(new_cursors),
                 [&](auto c) {
                   return c != 0;
                 });
    construction_state.cursors = new_cursors;
  }

  void remove_blanks()
    requires IS_DYNAMIC
  {
    // remove any nodes containing no data, and clear all references to them
    StateMachineNodeStore<Node_T, 0> new_nodes;
    std::vector<size_t> mappings(m_nodes.size(), 0);
    mappings[0] = 1;
    size_t idx  = 1;
    for (auto& node : m_nodes) {

      // If the node is null. we do not keep it, the exception is the root node and any nodes with cursors
      if (node.is_null() && node_index(node) != 1 && !has_cursor(idx)) {
        continue;
      }
      new_nodes.push(node);

      mappings[node_index(node) - 1] = idx;
      idx++;
    }

    for (Node_T& n : new_nodes) {

      // remap transitions to reflect the new cursors
      n.each_transition([&](auto key, auto& t) {
        if (t != 0) {
          t = mappings[t - 1];
        }
      });
    }

    std::vector<size_t> new_cursors;
    for (auto c : construction_state.cursors) {
      auto mapped = mappings[c - 1];
      new_cursors.push_back(mapped);
    }
    construction_state.cursors = new_cursors;
    m_nodes                    = new_nodes;
  }

  //
  // Makes the 'child' transition on the current cursors
  // if the transition already exists, we just update the cursor
  //
  //
  // NOTE: This function is not loop-aware
  //
  void cursor_transition(typename Node_T::Key_T child)
    requires IS_DYNAMIC
  {
    std::vector<size_t> cursors_without_specified_child;
    std::vector<size_t> cursors_with_specified_child;

    // Assess the actions required on each cursor
    for (auto cur : construction_state.cursors) {
      auto& node = get_node(cur);

      if (node.transition(child) == 0) {
        cursors_without_specified_child.push_back(cur);
      } else {
        cursors_with_specified_child.push_back(cur);
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
        get_node(cur).transition(child) = goes_to_idx;
      }
    }

    // the remaining cursors are overwritten with the index of the already
    // existing child node
    for (auto cur : cursors_with_specified_child) {
      auto new_idx = get_node(cur).transition(child);
      new_cursors.push_back(new_idx);
    }

    construction_state.cursors = new_cursors;
  }

  Node_T& new_node()
    requires IS_DYNAMIC
  {
    auto n = Node_T();
    m_nodes.push(n);
    return m_nodes[m_nodes.size() - 1];
  }

  std::size_t node_index(Node_T& node) const {
    auto const addr      = &node;
    auto const base_addr = &m_nodes[0];
    auto const diff      = addr - base_addr;

    return diff + 1;
  }

  Node_T& get_node(size_t index) {
    MUTILS_ASSERT_LTE(index, m_nodes.size(), "Attempt to get_node outside of node storage");
    MUTILS_ASSERT_NEQ(index, 0, "Attempt to get_node of a null reference");
    return m_nodes[index - 1];
  }

  bool has_cursor(size_t index) const {
    for (auto c : construction_state.cursors) {
      if (c == index) {
        return true;
      }
    }
    return false;
  }

  bool is_deletable_node(size_t index) {
    if (index == 1) {
      return false;
    }
    auto& n = get_node(index);
    if (!n.is_null()) {
      return false;
    }
    if (has_cursor(index)) {
      return false;
    }
    return true;
  }

  struct ConsumeResult {
    std::map<size_t, size_t> mappings;
    std::vector<size_t> terminals;
  };

  ConsumeResult consume_regex_except_root(MutableRegex regex) {
    std::map<size_t, size_t> mappings;

    std::vector<size_t> terminals;

    size_t const base_index = m_nodes.size() - 1;
    for (auto node = regex.m_nodes.begin() + 1; node < regex.m_nodes.end(); node++) {
      auto idx = regex.node_index(*node);

      if (node->value.has_value()) {
        terminals.push_back(idx + base_index);
      }


      Node_T n;
      // Update indexes for the new indexing system
      node->each_transition([&](auto k, auto& v) {
        n.transition(k) = v + base_index;
      });


      m_nodes.push(n);
      mappings[idx] = m_nodes.size();
    }

    ConsumeResult r;
    r.mappings  = mappings;
    r.terminals = terminals;
    return r;
  }

  void merge_regex_into_machine(MutableRegex regex)
    requires IS_DYNAMIC
  {
    auto const base_idx = m_nodes.size() - 1;
    auto result         = consume_regex_except_root(regex);
    auto terminals      = result.terminals;

    Node_T new_root_transitions;

    // Get all the regex's root transtions, normalized to the new indexes
    regex.m_nodes[0].each_transition([&](auto transition, auto& dest) {
      new_root_transitions.transition(transition) = dest + base_idx;
    });

    // merge the pseudo-node into each of the current cursors
    for (auto cursor : construction_state.cursors) {
      new_root_transitions.each_transition([&](auto key, auto& dest) {
        auto new_terminals = make_nonambiguous_link(cursor, key, dest, terminals);

        // add the new terminals to the terminals list
        std::for_each(new_terminals.begin(), new_terminals.end(), [&](auto t) {
          terminals.push_back(t);
        });
      });
    }

    // finally, we can update the insertion points (cursors) to be the terminals
    construction_state.cursors = terminals;
  }

  // Makes an unambiguous transition, this is where the brunt of regex combination logic lives
  // this function will never modify the 'to' node, but instead make clones whenever necessary
  // returns any nodes that were created as a replacement to any of the 'watch_nodes'
  //
  std::vector<size_t> make_nonambiguous_link(size_t from,
                                             typename Node_T::Key_T transition,
                                             size_t to,

                                             std::vector<size_t> watch_nodes) {

    MUTILS_ASSERT_NEQ(to, 0, "Tried to link to a null node");
    MUTILS_ASSERT_NEQ(from, 0, "Tried to link from a null node");

    // The pre-existing transitioned node
    auto current_target = get_node(from).transition(transition);


    // simplest case
    if (!current_target) {
      get_node(from).transition(transition) = to;
      return {};
    }

    // transition is already in place
    if (current_target == to) {
      return {};
    }

    // Create a new node to replace the current transitioned node
    // This node starts as an exact copy as the current transitioned node
    // we then begin merging in transitions from the target node as well
    // if any of these transitions collide, we run this function recursively
    // which should resolve all ambiguity
    //
    // exceptions:
    // If both transitions are found to be self-referring, we can skip it


    Node_T& node = new_node();
    auto nidx    = node_index(node);


    std::vector<size_t> tracked_nodes;

    // clone the current target to node
    node = get_node(current_target);

    // fix self-references
    node.each_transition([&](auto k, auto& v) {
      if (v == current_target) {
        v = nidx;
      }
    });


    // Update the tracked node list
    if (std::find(watch_nodes.begin(), watch_nodes.end(), to) != watch_nodes.end()) {
      tracked_nodes.push_back(nidx);
    } else if (std::find(watch_nodes.begin(), watch_nodes.end(), current_target) != watch_nodes.end()) {
      tracked_nodes.push_back(nidx);
    }


    Node_T& to_node = get_node(to);


    // Handle node value propogation
    if (to_node.value.has_value()) {
      if (node.value.has_value()) {
        switch (construction_state.on_conflict) {
          case ConflictAction::Error: {
            mutils::PANIC("Conflicting values have been encountered while making nonambiguous transition: " +
                          std::to_string(from) + " -> " + std::to_string(to) + " (via " +
                          mutils::stringify(transition) + "\n");
            break;
          }
          case ConflictAction::Skip: {
            break;
          }
          case ConflictAction::Overwrite: {
            node.value = to_node.value;
            break;
          }
        }
      } else {
        node.value = to_node.value;
      }
    }

    // copy in the target node transitions to the newly created intermediary node
    // purity means that a nodes transition intents will remain after further writes
    // without it, we may accidentally introduce new transitions and thus unexpected machine state paths
    for (auto [key, reference] : get_node(to).get_transitions()) {
      // The base is circular and we are null,
      // ensure the base maintains purity by changing it from
      // a self-ref to an original-ref
      auto& node_transition = get_node(nidx).transition(key);
      if (node_transition == nidx && reference == 0) {
        node_transition = current_target;
      }

      // We are circular and base is null
      // we maintiain purity by writing our transition
      else if (reference == to && node_transition == 0) {
        node_transition = current_target;
      }

      // We are both circular
      // the node can just refer to itself
      else if (reference == to && node_transition == nidx) {
        // Already refers to self, so continue
      }

      // skip null
      else if (reference == 0) {
      } else {
        auto res = make_nonambiguous_link(nidx, key, reference, watch_nodes);
        //
        for (auto n : res) {
          tracked_nodes.push_back(n);
        }
      }
    }

    // set the transition
    get_node(from).transition(transition) = nidx;

    return tracked_nodes;
  }

  ///
  /// Similar to cursor_transition, but ensures the creation of a new path
  ///
  /// a lot of behavior derived from make_nonambiguous_link
  ///
  void cursor_discreet_transition(typename Node_T::Key_T transition) {
    std::cout << "cursor_discreet_transition ::: " << std::string(transition) << "\n";
    std::vector<size_t> cursors_with_child;
    std::vector<size_t> cursors_without_child;
    std::vector<size_t> new_cursors;

    for (auto cursor : construction_state.cursors) {
      auto& current_target = get_node(cursor).transition(transition);

      // if non-existant, we can go directly to def
      if (!current_target) {
        cursors_without_child.push_back(cursor);
      } else {
        cursors_with_child.push_back(cursor);
      }
    }

    if (cursors_without_child.size()) {
      auto default_idx = node_index(new_node());
      new_cursors.push_back(default_idx);
      for (auto cur : cursors_without_child) {

        get_node(cur).transition(transition) = default_idx;
      }
    }
    if (cursors_with_child.size()) {
      for (auto cursor : cursors_with_child) {

        auto old_target = get_node(cursor).transition(transition);

        // create an intermediary, cloned from the old value
        auto& intermediary = new_node();
        intermediary       = get_node(old_target);
        auto inter_idx     = node_index(intermediary);


        //
        // If the old transition used to refer immediately to itself
        // we have to update the corresponding transition in the clone to refer to itself
        // if this did not happen, when the transition is met twice in a row, we are brought out
        // of the matched state, requiring another transition to go back in.
        // With this fix, making the same transition immediately after getting out of a loop will
        // bring you back to the same place where you can transition again
        //
        if (old_target == cursor) {
          intermediary.transition(transition) = inter_idx;
        }

        // finally, rereference the cursor transition to point to the newly created intermediary
        get_node(cursor).transition(transition) = inter_idx;
        new_cursors.push_back(inter_idx);
      }
    }
    construction_state.cursors = new_cursors;
  }
};
}; // namespace regex_backend::internal
