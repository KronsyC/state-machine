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

#include "./node.h"
#include "mutils/assert.h"
#include "mutils/panic.h"
#include "mutils/stringify.h"
#include <algorithm>
#include <concepts>
#include <functional>
#include <iostream>
#include <map>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace regex_backend {
enum Conflict {
  Skip,
  Overwrite,
  Error
};

/**
 * \brief A Simple class for building your own custom state machines
 *
 * it exposes a simple builder pattern to construct these state machines
 *
 * the MutableStateMachine<void> specialization represents a raw Regular Expression state machine,
 * whereas any other specializations represent State machines with contained values
 */
template <typename Value_T> struct MutableStateMachine {


private:
  template <typename T> struct _M_source_range_t {
    char const* begin;
    char const* end;
    Value_T* value;
  };

  template <> struct _M_source_range_t<void> {
    char const* begin;
    char const* end;
  };

  template <typename T> struct _M_MatchResult_T {
    using type = Value_T*;
  };

  template <> struct _M_MatchResult_T<void> {
    using type = bool;
  };

  template <typename T> struct _M_LookupResult {
    char const* end;
    Value_T* value;
  };

  template <> struct _M_LookupResult<void> {
    char const* end;
  };

  using Node_T = StateMachineNode<Value_T>;

  std::vector<Node_T> _m_nodes;
  Conflict on_conflict = Conflict::Error;

  std::vector<size_t> m_cursors = {1};
  using Self                    = MutableStateMachine;
  using MutableRegex            = MutableStateMachine<void>;

  Node_T& get_node(size_t number) {
    MUTILS_ASSERT(number != 0, "Attempt to load a null transition");
    MUTILS_ASSERT(number <= _m_nodes.size(), "Attempt to access an out-of-range node");
    return _m_nodes[number - 1];
  }

public:
  MutableStateMachine() {
    // root node insertion
    _m_nodes.push_back(Node_T());
  }

  bool operator==(MutableStateMachine const& other) const {

    if (_m_nodes.size() != other._m_nodes.size()) {
      return false;
    }

    for (size_t i = 0; i < _m_nodes.size(); i++) {
      if (_m_nodes[i] != other._m_nodes[i]) {
        return false;
      }
    }
    return true;
  }

  /**
   * Get the root node of the state machine
   */
  Node_T& root() {
    return _m_nodes[0];
  }

  /**
   * Set the behavior of the state machine when
   * conflicting node values are written
   *
   */
  Self& conflict(Conflict c) {
    on_conflict = c;
    return *this;
  }

  /**
   * Optionally match the provided regular expression pattern
   *
   * roughly equivalent to the '?' operator in regex
   */
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

  /**
   * Match the provided regular expression pattern
   *
   * this is used for modularly building a state machine
   */
  Self& match(MutableRegex pattern) {
    merge_regex_into_machine(pattern);
    return *this;
  }

  /**
   * Match the given regular expression 1 or more times
   *
   * equivalent to the '+' regex operator
   */
  Self& match_many(MutableRegex& pattern) {
    // Simply match the pattern, then match_many_optionally
    return match(pattern).match_many_optionally(pattern);
    // return *this;
  }

  /**
   * Match the given regular expression 0 or more times
   *
   * equivalent to the '*' regex operator
   */
  Self& match_many_optionally(MutableRegex pattern) {

    auto cursors_before = m_cursors;

    // Merge the regex
    merge_regex_into_machine(pattern);

    auto regex_terminals = m_cursors;
    // Allow the end point to cycle back into the start.
    // we do this by copying the pure transitions of the root pattern node
    // to each of the cursors
    //
    // 1. Copy pattern into machine (excl. root)
    // 2. make nonambiguous transitions into the new part of machine for each current cursor
    //    (for each transition of the original pattern root)
    //    we do not use the new machine segment created by the regex cloning as that segment
    //    may be "corrupted" by currently existing transitions

    auto res           = copy_in_regex_except_root(pattern);
    auto& pattern_root = pattern.root();
    //
    // // Transform the newly written regex into a cycle
    for(auto c : res.terminals){
      int transition = 0;
      for(auto tzn : pattern_root.transitions){
        if(!tzn){
          transition++;
          continue;
        }
        make_nonambiguous_link(c, transition, res.mappings[tzn], {});
        transition++;
      }
    }


    m_cursors = regex_terminals;
    for (auto c : m_cursors) {
      int transition = 0;
      for (auto tzn : pattern_root.transitions) {
        if(!tzn){
          transition++;
          continue;
        }
        // std::cout << "Make transition " << c << " -> " << res.mappings[tzn] << " via " << stringify_char(transition) << "\n";
        make_nonambiguous_link(c, transition, res.mappings[tzn], {});
        transition++;
      }
    }
    //
    // // finally, we preserve the original cursors
    m_cursors = regex_terminals;
    for (auto c : cursors_before) {
      m_cursors.push_back(c);
    }
    for(auto c : res.terminals){
      m_cursors.push_back(c);
    }

    return *this;
  }

  /**
   * Create a new state-machine branch for the default cases
   *
   * this should be done last, as to prevent ambiguity
   */
  Self& match_default() {
    // Create a single node for all default cases to point to
    auto& default_node    = new_node();
    auto default_node_idx = node_index(default_node);
    for (auto c : m_cursors) {
      for (auto& t : get_node(c).transitions) {
        if (t == 0) {
          t = default_node_idx;
        }
      }
    }
    m_cursors = {default_node_idx};
    return *this;
  }

  /**
   * Write the given value at the current table position
   * then return back to the root
   */
  template <typename Val_T>
  Self& commit(Val_T value)
    requires((std::is_same_v<Value_T, Val_T> || std::is_convertible_v<Val_T, Value_T>) &&
             !std::is_same_v<Value_T, void>)
  {

    commit_continue(value);

    m_cursors = {1};

    return *this;
  }

  /**
   * Write the given value at the current table position,
   * and continue
   */
  template <typename Val_T>
  Self& commit_continue(Val_T value)
    requires((std::is_same_v<Value_T, Val_T> || std::is_convertible_v<Val_T, Value_T>) &&
             !std::is_same_v<Value_T, void>)
  {
    for (auto cur : m_cursors) {
      auto& node = get_node(cur);

      if (node.value) {
        switch (on_conflict) {
          case Conflict::Skip: {
            break;
          }
          case Conflict::Overwrite: {
            node.value = value;
            break;
          }
          case Conflict::Error: {

            mutils::PANIC("Failed to commit a value to node #" + std::to_string(cur) + " as the value: '" +
                          mutils::stringify(node.value.value()) +
                          "' already exists at this node\n"
                          "\tIf this is intentional behavior, change the collision action using the collison() method");
          }
        }
      } else {

        node.value = value;
      }
    }

    return *this;
  }

  /**
   * Match a sequence of characters exactly
   */
  Self& match_sequence(std::string seq) {

    for (char part : seq) {
      cursor_transition(part);
    }

    return *this;
  }

  /**
   * Match any character (including whitespace and control chars)
   */
  Self& match_any() {
    std::vector<size_t> new_cursors;
    auto initial_cursors = m_cursors;
    for (int i = 0; i <= 128; i++) {
      // transition, and update cursors
      cursor_transition(i);

      for (auto c : m_cursors) {
        new_cursors.push_back(c);
      }

      m_cursors = initial_cursors;
    }
    m_cursors = new_cursors;
    return *this;
  }

  /**
   * Match any of the characters provided in the 'choices' string
   */
  Self& match_any_of(char const* choices) {
    std::vector<size_t> new_cursors;
    auto initial_cursors = m_cursors;
    for (char const* c = choices; *c != 0; c++) {
      // transition, and update cursors
      cursor_transition(*c);

      for (auto c : m_cursors) {
        new_cursors.push_back(c);
      }

      m_cursors = initial_cursors;
    }
    m_cursors = new_cursors;
    return *this;
  }

  /**
   * Match any digit (0-9)
   */
  Self& match_digit() {
    return match_any_of("0123456789");
  }

  /**
   * Match an eof
   *
   * in the context of a stream, eof is the stream end
   * in strings, eof is the null terminator
   */
  Self& match_eof() {
    cursor_transition(128);

    return *this;
  }

  /**
   * Match any alphabetical character (a-z, A-Z)
   */
  Self& match_alpha() {
    return match_any_of("qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM");
  }

  /**
   * Match any lowercase alphabetical character (a-z)
   */
  Self& match_lowercase() {
    return match_any_of("qwertyuiopasdfghjklzxcvbnm");
  }

  /**
   * Match any uppercase alphabetical characters (A-Z)
   */
  Self& match_uppercase() {
    return match_any_of("QWERTYUIOPASDFGHJKLZXCVBNM");
  }

  /**
   * Matches visual whitespace characters
   * defined at https://en.wikipedia.org/wiki/Whitespace_character
   */
  Self& match_whitespace() {
    return match_any_of("\011\012\013\014\015\040");
  }

  /**
   * Matches any control characters
   * control characters are those outside of ascii range [ 33 - 127 ], which are not whitespace
   */
  Self& match_control() {
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

  /**
   * Sets the current cursor position to a terminal state
   * (only works for simple state machines)
   */
  Self& terminal()
    requires std::is_same_v<Value_T, void>
  {
    make_cursor_terminal();
    return *this;
  }

  /**
   * Reset the cursors back to the root node
   */
  Self& goback() {
    m_cursors = {1};
    return *this;
  }

  /**
   * Dump a textual representation of the state machine to
   * stdout
   */
  void print_dbg() {
    size_t idx = 0;


    std::string in = " |  ";
    for (Node_T& node : _m_nodes) {
      bool is_terminal = node.can_exit();
      bool is_cursor   = std::find(m_cursors.begin(), m_cursors.end(), node_index(node)) != m_cursors.end();

      std::string terminal_msg = "A";
      if (is_terminal) {
        if constexpr (std::is_same_v<Value_T, void>) {
          terminal_msg = "(terminal)";
        } else {
          terminal_msg = "(terminal val: '" + mutils::stringify(node.value.value()) + "' )";
        }
      }

      std::cout << "#" << idx + 1 << " " << (is_terminal ? terminal_msg : "") << " " << (is_cursor ? "[cursor] " : "")
                << ">>\n";

      int transition = 0;

      for (auto t : node.transitions) {
        if (t != 0) {
          std::cout << in << stringify_char(transition) << " -> #" << t << "\n";
        }

        transition++;
      }

      std::cout << "\n";
      idx++;
    }
  }

  /**
   * Minimize the size of the data structure as much as possible
   *
   * WARNING: This should not be called on incomplete machines, as optimizations
   *          assume no more extra data will be written
   *
   *          If you do write more transitions after optimization,
   *          the transitions are likely to encounter all sorts of strange
   *          behavior (u.b)
   */
  void optimize() {
    remove_duplicates();
    nullify_orphans();
    remove_blanks();

    // these passes invalidate cursors
    // thus, we do the safest thing and reset them
    m_cursors = {1};
  }

  /**
   * De-compactify the nodes of the tree
   *
   * This is a crazy slow operation, so call with caution
   */
  void expand() {
    std::vector<Node_T> new_nodes;
    m_expand(new_nodes);
    _m_nodes  = new_nodes;
    m_cursors = {1};
  }

  //
  // Lookup-oriented functions
  // performance matters
  //

  using matchresult = typename _M_MatchResult_T<Value_T>::type;

  /**
   * Test if the state machine successfully matches
   * the entire string
   */
  template <bool FILEMODE = false> matchresult matches(const std::string_view s) {
    size_t node = 1;

    if constexpr (FILEMODE) {
      for (size_t i = 0; i <= s.size(); i++) {
        int c    = i == s.size() ? 128 : s[i];
        auto tzn = get_node(node).transitions[c];
        if (tzn == 0) {
          if constexpr (std::is_same_v<void, Value_T>) {
            return false;
          } else {
            return nullptr;
          }
        }
        node = tzn;
      }
    } else {

      for (size_t i = 0; i < s.size(); i++) {
        int c    = s[i];
        auto tzn = get_node(node).transitions[c];
        if (tzn == 0) {
          if constexpr (std::is_same_v<void, Value_T>) {
            return false;
          } else {
            return nullptr;
          }
        }
        node = tzn;
      }
    }
    // if we ended on a terminal, all is good

    if constexpr (std::is_same_v<void, Value_T>) {
      return get_node(node).can_exit();
    } else {
      if (get_node(node).value.has_value()) {
        return &get_node(node).value.value();
      } else {
        return nullptr;
      }
    }
  }

  using lookup_result = _M_LookupResult<Value_T>;

  /**
   * Attempt to match the beginning of the string with
   * the expression as far as possible
   *
   * end will be nullptr if the match fails
   */
  lookup_result lookup(char const* s) {
    char const* c = s;
    size_t n      = 1;

    char const* last_val    = nullptr;
    Node_T* last_value_node = nullptr;

    while (*c != 0) {
      auto idx = get_node(n).transitions[*c];

      if (idx == 0) {
        break;
      }
      Node_T& next = get_node(idx);

      if (next.can_exit()) {
        last_val        = c;
        last_value_node = &next;
      }
      c++;
      n = idx;
    }

    if (last_val) {
      lookup_result lr;
      lr.end = last_val;

      if constexpr (!std::is_same_v<void, Value_T>) {
        lr.value = &last_value_node->value.value();
      }
      return lr;
    }
    lookup_result lr;
    lr.end = nullptr;
    return lr;
  }

  using source_range = _M_source_range_t<Value_T>;

  /**
   * Find the first sequence of characters that matches the expression
   * matches are greedy
   *
   * returns the range of characters matched, and a corresponding stored value (if applicable)
   * start and end will be null if no match has occurred
   *
   * NOTE: This function can be quite slow ( O(n^2) ), so please consider alternative methods before using this
   */
  source_range find_first(char const* s) {
    //
    // We consider a match to have occurred if the substring
    // causes a traversal through a value-node at any point
    //
    // We continue after finding a value node, as this function
    // is 'greedy'
    //
    // Iterate over each substring until we find one which

    char const* ss = s;

    while (*ss != 0) {
      size_t node                = 1;
      Node_T* last_value_node    = nullptr;
      char const* last_value_ptr = nullptr;
      for (auto c = ss; *c != 0; c++) {
        auto next = get_node(node).transitions[*c];

        if (next == 0) {
          // end of chain
          break;
        }

        auto& n = get_node(next);

        if (n.can_exit()) {
          last_value_node = &n;
          last_value_ptr  = c;
        }
        node = next;
      }

      if (last_value_node) {
        // We have found a value node
        // this node is the deepest one encountered on the first match
        source_range range;

        range.begin = ss;
        range.end   = last_value_ptr;
        if constexpr (!std::is_same_v<Value_T, void>) {
          range.value = &last_value_node->value.value();
        }
        return range;
      }

      ss++;
    }
    source_range r;
    r.begin = nullptr;
    r.end   = nullptr;
    return r;
  }

  /**
   * Find each sequence of characters that matches the expression
   * matches are greedy
   *
   * returns the each range of characters matched, and the corresponding stored values (if applicable)
   *
   * NOTE: This function can be quite slow, so please consider alternative methods before using this
   */

  std::vector<source_range> find_many(char const* s) {
    std::vector<source_range> return_value;

    char const* cur = s;

    while (*cur != 0) {
      // std::cout << "CUR: " << cur << "\n";
      auto result = find_first(cur);
      if (result.begin == nullptr) {
        // we are done, no more instances
        break;
      }
      return_value.push_back(result);
      // Update the cursor to be equal to end + 1
      cur = result.end;
    }

    return return_value;
  }

private:
  size_t m_expand(std::vector<Node_T>& storage, size_t node = 0, std::map<size_t, size_t> branch_mappings = {}) {
    // For this function, we walk the function and construct a new node for each node encountered
    // without taking duplicate encoutners into account
    //
    // Implemented via depth-first search
    //
    // when a loop is encountered, make sure it points to a node on the current branch


    auto root_idx = storage.size();

    storage.push_back(Node_T());

    branch_mappings[node] = root_idx;
    storage[root_idx]     = get_node(node);
    storage[root_idx].transitions.fill(0);
    int c = 0;
    for (auto t : get_node(node).transitions) {

      if (t == 0) {
        c++;
        continue;
      }

      if (branch_mappings.find(t) == branch_mappings.end()) {


        auto child_idx = m_expand(storage, t, branch_mappings);

        storage[root_idx].transitions[c] = child_idx;
      } else {
        storage[root_idx].transitions[c] = branch_mappings[t];
        // std::cout<< "Loop\n";
      }
      c++;
    }

    return root_idx;
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
      get_node(m_cursors[idx]).terminal = true;
    });
  }

  //
  // Makes the 'child' transition on the current cursors
  // if the transition already exists, we just update the cursor
  //
  //
  // NOTE: This function is not loop-aware
  //
  void cursor_transition(int child) {

    std::vector<size_t> cursors_without_specified_child;
    std::vector<size_t> cursors_with_specified_child;

    // Asess the actions required on each cursor
    for (size_t i = 0; i < m_cursors.size(); i++) {
      auto& node = get_node(m_cursors[i]);

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
        get_node(m_cursors[cur]).transitions[child] = goes_to_idx;
      }
    }

    // the remaining cursors are overwritten with the index of the already
    // existing child node
    for (auto cur : cursors_with_specified_child) {
      auto new_idx = get_node(m_cursors[cur]).transitions[child];

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

  void write_transition(char transition_on, size_t transition_target) {

    std::vector<size_t> sliding_window_transitions;

    for (auto c : m_cursors) {
      auto& node = get_node(c);
      if (node.transitions[transition_on] == 0) {
        // safely transition
        node.transitions[transition_on] = transition_target;
      } else {
        sliding_window_transitions.push_back(c);
      }
    }


    for (auto cursor : sliding_window_transitions) {

      // all we do is copy the contents of the next node to the pointed node
      auto& node      = get_node(cursor);
      auto& next_node = get_node(node.transitions[transition_on]);
    }

    if (sliding_window_transitions.size()) {
      mutils::PANIC("Sliding window cyclic transition resolution is not implemented");
    }
  }

  Node_T& new_node() {
    auto n = Node_T();
    n.transitions.fill(0);
    _m_nodes.push_back(n);
    return _m_nodes[_m_nodes.size() - 1];
  }

  std::size_t node_index(Node_T& node) {
    auto const addr      = &node;
    auto const base_addr = &root();
    auto const diff      = addr - base_addr;

    return diff + 1;
  }

  std::string stringify_char(int c) {
    if (c == -128 || c == 128) {
      return "<EOF>";
    }
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

  //
  // Makes an unambiguous transition
  //
  // this function will never modify the 'to' node, but instead make clones whenever necessary
  //
  // returns any nodes that were created as a replacement to any of the 'watch_nodes'
  //
  std::vector<size_t>
      make_nonambiguous_link(size_t from, int transition_char, size_t to, std::vector<size_t> watch_nodes) {

    MUTILS_ASSERT(to != 0, "Tried to link to a null node");
    MUTILS_ASSERT(from != 0, "Tried to link from a null node");
    // The pre-existing transitioned node
    auto tzn = get_node(from).transitions[transition_char];
    if (!tzn) {
      get_node(from).transitions[transition_char] = to;
      return {};
    }

    // Create a new node to replace the current transition
    // This node starts as an exact copy as the current transitioned node
    // we then begin merging in transitions from the target node as well
    // if any of these transitions collide, we run this function recursively
    //
    // exceptions:
    // If both transitions are found to be self-referring, we can skip it


    auto& node = new_node();
    auto nidx = node_index(node);
    node      = get_node(tzn);

    // fix self-references
    for (auto t : node.transitions) {
      if (t == tzn) {
        t = nidx;
      }
    }

    std::vector<size_t> tracked_nodes;


    // Update the tracked node list
    if (std::find(watch_nodes.begin(), watch_nodes.end(), to) != watch_nodes.end()) {
      tracked_nodes.push_back(nidx);
    } else if (std::find(watch_nodes.begin(), watch_nodes.end(), tzn) != watch_nodes.end()) {
      tracked_nodes.push_back(nidx);
    }


    Node_T& to_node = get_node(to);


    // Handle node value propogation
    if constexpr (std::is_same_v<Value_T, void>) {
      if (to_node.terminal) {
        node.terminal = true;
      }
    } else {
      if (to_node.value.has_value()) {
        if (node.value.has_value()) {
          switch (on_conflict) {
            case Conflict::Error: {
              mutils::PANIC("Conflicting values have been encountered while making nonambiguous transition: " +
                            std::to_string(from) + " -> " + std::to_string(to) + " (via " +
                            stringify_char(transition_char) + "\n");
              break;
            }
            case Conflict::Skip: {
              break;
            }
            case Conflict::Overwrite: {
              node.value = to_node.value;
              break;
            }
          }
        } else {
          node.value = to_node.value;
        }
      }
    }

    // copy in the target node
    int ch = 0;
    for (auto transition : get_node(to).transitions) {

#define NEXT                                                                                                           \
  ch++;                                                                                                                \
  continue
      // The base is circular and we are null,
      // ensure the base maintains purity by changing it from
      // a self-ref to an original-ref
      if (node.transitions[ch] == nidx && transition == 0) {
        node.transitions[ch] = tzn;
        NEXT;
      }

      // We are circular and base is null
      // we maintiain purity by writing our transition
      if (transition == to && node.transitions[ch] == 0) {
        node.transitions[ch] = tzn;
        NEXT;
      }

      // We are both circular
      // the node can just refer to itself
      if (transition == to && node.transitions[ch] == nidx) {
        // Already refers to self, so continue
        NEXT;
      }

      // skip null
      if (transition == 0) {
        NEXT;
      }


      auto res = make_nonambiguous_link(nidx, ch, transition, watch_nodes);

      for (auto n : res) {
        tracked_nodes.push_back(n);
      }


      NEXT;

#undef NEXT
    }

    // set the transition
    get_node(from).transitions[transition_char] = nidx;

    return tracked_nodes;
  }

  struct CopyResult {
    std::map<size_t, size_t> mappings;
    std::vector<size_t> terminals;
  };

  CopyResult copy_in_regex_except_root(MutableRegex regex) {
    std::map<size_t, size_t> mappings;

    std::vector<size_t> terminals;

    const size_t base_index = _m_nodes.size() - 1;
    for (auto node = regex._m_nodes.begin() + 1; node < regex._m_nodes.end(); node++) {
      auto idx = regex.node_index(*node);

      if (node->terminal) {
        terminals.push_back(idx + base_index);
      }


      Node_T n;
      n.transitions.fill(0);
      // Update indexes
      for (size_t i = 0; i < n.transitions.size(); i++) {
        if (node->transitions[i] == 0) {
          continue;
        }
        n.transitions[i] = node->transitions[i] + base_index;
      }

      n.consume_char = node->consume_char;

      _m_nodes.push_back(n);
      mappings[idx] = _m_nodes.size();
    }

    CopyResult r;
    r.mappings  = mappings;
    r.terminals = terminals;
    return r;
  }

  //
  // Merge a regex into the current state machine, while applying cursor transitions
  //
  // returns a mapping of regex indexes -> created indexes
  //
  void merge_regex_into_machine(MutableRegex regex) {

    //
    // PROCEDURE:
    //
    // 1. Copy all nodes from the regex into this machine (excluding the root)
    // 2. Clone the root to each cursor, then de-ambiguify
    //


    const size_t base_index = _m_nodes.size() - 1;
    auto result             = copy_in_regex_except_root(regex);
    auto terminals = result.terminals;

    std::array<size_t, 129> new_root_transitions;

    // Generate a list of the transitions that need
    // to be added to the cursors
    int c = 0;
    for (auto transition : regex.root().transitions) {
      if (transition != 0) {
        transition += base_index;
      }
      new_root_transitions[c] = transition;
      c++;
    }

    // de-ambigufying merge of the new_root_transitions with each cursor location

    for (auto cur : m_cursors) {

      int ch = 0;
      for (auto transition : new_root_transitions) {
        if (transition) {
          auto equivalent_terminals = make_nonambiguous_link(cur, ch, transition, terminals);

          for (auto n : equivalent_terminals) {
            terminals.push_back(n);
          }
        }

        ch++;
      }
    }

    m_cursors = terminals;
  }

  std::array<size_t, 129> get_cursor_common_transition() {
    std::array<size_t, 129> transitions = get_node(m_cursors[0]).transitions;

    for (size_t idx = 1; idx < m_cursors.size(); idx++) {
      auto c    = m_cursors[idx];
      auto tzns = get_node(c).transitions;

      // iterate over all transitions, remove ones
      // not common to main array
      for (size_t i = 0; i < 129; i++) {
        if (tzns[i] != transitions[i]) {
          transitions[i] = 0;
        }
      }
    }

    return transitions;
  }

  void cursor_overwrite_transition(int transition, size_t new_tgt) {
    for (auto c : m_cursors) {
      get_node(c).transitions[transition] = new_tgt;
    }
  }

  bool cursor_transition_is_free(int transition) {
    for (auto c : m_cursors) {
      if (get_node(c).transitions[transition]) {
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
    if (start == end) {
      return true;
    }
    if (chain_start == -1) {
      chain_start = start;
    }

    if (direct && chain_start == start) {
      for (auto tzn : get_node(end).transitions) {
        if (tzn != chain_start && path_is_cycle(tzn, chain_start, false)) {
          return false;
        }
      }
    }

    visited_nodes.push_back(start);

    for (auto transition : get_node(start).transitions) {

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

  bool remove_duplicates_once() {
    // We work backwards while removing duplicates, as that is the way in which
    // they are more likely to be positioned

    bool has_removed_dup = false;

    for (auto noderef = _m_nodes.rbegin(); noderef < _m_nodes.rend() - 1; noderef++) {
      if (noderef->is_null()) {
        continue;
      }

      auto& node    = *noderef;
      auto node_idx = node_index(node);
      // check if another node with the exact same data exists
      auto other_node = std::find_if(_m_nodes.begin(), noderef.base() - 1, [&node, this, node_idx](Node_T& other) {
        // We also consider nodes to be equal if transitions are self-referring


        if (node.consume_char != other.consume_char) {
          return false;
        }
        if constexpr (std::is_same_v<Value_T, void>) {
          if (node.terminal != other.terminal) {
            return false;
          }
        } else {
          if (node.value != other.value) {
            return false;
          }
        }
        auto other_idx = node_index(other);

        for (size_t i = 0; i < node.transitions.size(); i++) {
          auto ntzn = node.transitions[i];
          auto otzn = other.transitions[i];

          bool are_both_self_referring = otzn == other_idx && ntzn == node_idx;
          if (ntzn != otzn && !are_both_self_referring) {
            return false;
          }
          // if(ntzn != otzn) return false;
        }
        return true;
      });

      if (other_node != noderef.base() - 1 && !other_node->is_null()) {

        auto new_idx = node_index(node);
        auto old_idx = node_index(*other_node);

        for (auto& n : _m_nodes) {
          for (auto& tzn : n.transitions) {
            if (tzn == old_idx) {
              tzn = new_idx;
            }
          }
        }

        other_node->nullify();
        has_removed_dup = true;
      }
    }

    return has_removed_dup;
  }

  void nullify_orphans() {
    std::vector<bool> reachables(_m_nodes.size(), false);
    reachables[0] = true;

    while (true) {
      bool has_expanded = false;
      for (Node_T& n : _m_nodes) {
        if (reachables[node_index(n) - 1]) {
          for (auto t : n.transitions) {
            if (t == 0) {
              continue;
            }
            const size_t corrected_transition = t - 1;
            if (reachables[corrected_transition]) {
              continue;
            }
            reachables[corrected_transition] = true;
            has_expanded                     = true;
          }
        }
      }
      if (!has_expanded) {
        break;
      }
    }

    for (auto i = 0; i < _m_nodes.size(); i++) {
      if (reachables[i]) {
        // _m_nodes[i].nullify();
      }
    }
  }

  void remove_blanks() {
    // remove any nodes containing no data, and clear all references to them
    std::vector<Node_T> new_nodes;
    std::vector<size_t> mappings(_m_nodes.size(), 0);
    size_t idx = 1;
    for (auto& node : _m_nodes) {

      // do not bother if the node is null, but leave it if its the root
      if ((node.is_null()) && node_index(node) != 1) {
        continue;
      }
      new_nodes.push_back(node);

      mappings[node_index(node) - 1] = idx;
      idx++;
    }

    for (auto& n : new_nodes) {
      for (auto& t : n.transitions) {
        if (t == 0) {
          continue;
        }
        t = mappings[t - 1];
      }
    }
    _m_nodes = new_nodes;
  }
};

using MutableRegex = MutableStateMachine<void>;

}; // namespace regex_backend
