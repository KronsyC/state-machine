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

#include "mutils/assert.h"
#include "mutils/stringify.h"
#include "node_store.h"
#include <array>
#include <bit>
#include <functional>
#include <map>
#include <optional>
#include <variant>
#include <vector>

namespace regex_backend::internal {

///
/// Node value with user-defined type
///
template <typename T> struct Node_Value {
  T value;        // user-defined value
  size_t back_by; // The number of transitions to go back by before exiting, useful for match after-dependencies

  bool operator==(Node_Value const& other) const {
    return back_by == other.back_by && value == other.value;
  }

  bool operator!=(Node_Value const& other) const {
    return back_by != other.back_by && value != other.value;
  }
};

///
/// Node value with no contained type
///
template <> struct Node_Value<void> {
  size_t back_by;

  bool operator==(Node_Value const& other) const {
    return other.back_by == back_by;
  }

  bool operator!=(Node_Value const& other) const {
    return other.back_by != back_by;
  }
};

template <typename T, typename U, bool D> struct StateMachineNode;

///
/// Transition Key type to make the interface easier to work with
///
template <typename Key_T> class TransitionKey {

  // if no val is present, this key is an EOF
  std::optional<Key_T> val;

  enum {
    Val,
    Eof,
    Def,
  } kind;
  template <typename K, typename V, bool D> friend struct StateMachineNode;


public:
  static TransitionKey eof() {
    TransitionKey k;
    k.kind = Eof;
    return k;
  }

  static TransitionKey value(Key_T k) {
    TransitionKey key;
    key.val  = k;
    key.kind = Val;
    return key;
  }

  static TransitionKey def() {
    TransitionKey k;
    k.kind = Def;
    return k;
  }

  operator std::string() const {
    switch (kind) {
      case Eof: return "<EOF>";
      case Def: return "<Default>";
      case Val:
        if constexpr (std::is_same_v<Key_T, char32_t>) {
          auto v = val.value();
          if(v & 0b10000000){
            std::stringstream ss1;
            std::stringstream ss2;
            // re-introduce the drop bit and convert to hex
            ss1 << std::hex << (int)(v | 0b01000000);
            ss2 << std::hex << (int)(v);
            return "\\0x" + ss1.str() + " (header) OR " + "\\0x"+ss2.str() + " (data) (compressed utf8 encoding)";
          }
          else{
            return mutils::stringify((char)v);
          }
        } else {
          return mutils::stringify(val.value());
        }
    }
  }

  Key_T key_val() const {
    MUTILS_ASSERT_EQ(kind, kind.Val, "Cannot get key_val of non-val transition");
  }
};

//////////////////////////////////////////////////////////////
/// State Machine Node and specializations
//////////////////////////////////////////////////////////////


template <typename Stored_T, typename Transition_T, bool DYNAMIC> struct StateMachineNode {

  using TransitionMap_T = std::map<Transition_T, size_t>;
  using Value_T         = Node_Value<Stored_T>;
  using Key_T           = TransitionKey<Transition_T>;

private:
  TransitionMap_T transitions;
  size_t eof_transition = 0; // we hold a special transition state for the end-of-input which may be matched against
  size_t default_transition =
      0; // We also maintain a default transition, without this, the transition map would be huge
public:
  std::optional<Value_T> value;

  void nullify()
    requires DYNAMIC
  {
    value = {};
    transitions.clear();
    eof_transition     = 0;
    default_transition = 0;
  }

  bool is_null() const {
    return !value.has_value() && transitions.empty() && eof_transition == 0 && default_transition == 0;
  }

  ///
  /// Iterate over every currently existing transition
  ///
  void each_transition(std::function<void(Key_T key, size_t& ref)> callback)
    requires DYNAMIC
  {
    for (auto& [k, v] : transitions) {
      if (v != 0) {
        callback(Key_T::value(k), v);
      }
    }
    if (eof_transition != 0) {
      callback(Key_T::eof(), eof_transition);
    }

    if (default_transition != 0) {
      callback(Key_T::def(), default_transition);
    }
  }

  struct TransitionInfo {
    TransitionKey<Transition_T> key;
    size_t to;
  };

  ///
  /// Get all transitions
  /// Note: You may mutate the node store
  ///
  std::vector<TransitionInfo> get_transitions() {
    std::vector<TransitionInfo> ret;
    if (eof_transition != 0) {
      TransitionInfo ti;
      ti.to  = eof_transition;
      ti.key = Key_T::eof();
      ret.push_back(ti);
    }

    if (default_transition != 0) {
      TransitionInfo ti;
      ti.to  = default_transition;
      ti.key = Key_T::def();
      ret.push_back(ti);
    }
    for (auto [k, v] : transitions) {
      if (v != 0) {
        TransitionInfo ti;
        ti.to  = v;
        ti.key = Key_T::value(v);
        ret.push_back(ti);
      }
    }
    return ret;
  }

  size_t& eof()
    requires DYNAMIC
  {
    return eof_transition;
  }

  size_t get_eof() const {
    return eof_transition;
  }

  size_t& def()
    requires DYNAMIC
  {
    return default_transition;
  }

  ///
  /// Function for trivially accessing transitions
  /// NOTE: Not really suitable for runtime purposes
  ///
  size_t& transition(TransitionKey<Transition_T> key)
    requires DYNAMIC
  {
    switch (key.kind) {
      case key.Eof: return eof_transition;
      case key.Def: return default_transition;
      case key.Val: return transitions[key.val.value()];
    }
  }
};

///
/// Node specialization for chars and utf8chars
///
template <typename Stored_T, typename Transition_T, bool DYNAMIC>
  requires std::is_same_v<Transition_T, char> || std::is_same_v<Transition_T, char32_t>
struct StateMachineNode<Stored_T, Transition_T, DYNAMIC> {

private:
  constexpr static bool UTF8                = std::is_same_v<Transition_T, char32_t>;
  constexpr static size_t eof_idx           = UTF8 ? 196 : 128;
  constexpr static size_t def_idx           = UTF8 ? 197 : 129;
  constexpr static size_t EXTRA_STATE_COUNT = DYNAMIC ? 2 : 1;
  constexpr static size_t KEYSPACE_SIZE     = UTF8 ? 196 : 128;
  constexpr static size_t KEY_MASK          = 0b10111111;


public:
  using TransitionMap_T =
      std::array<size_t, KEYSPACE_SIZE + EXTRA_STATE_COUNT>; // base ascii set, eof and default when dynamic
  using Value_T = Node_Value<Stored_T>;
  using Key_T   = TransitionKey<Transition_T>;

private:
  TransitionMap_T transitions;

public:
  StateMachineNode() {
    transitions.fill(0);
  }

  std::optional<Value_T> value;

  void nullify()
    requires DYNAMIC
  {
    value = {};
    transitions.fill(0);
  }

  bool is_null() const {
    bool empty = true;
    for (auto t : transitions) {
      if (t) {
        empty = false;
        break;
      }
    }
    return !value.has_value() && empty;
  }

  struct TransitionInfo {
    TransitionKey<Transition_T> key;
    size_t to;
  };

  ///
  /// Get all transitions
  /// Note: You may mutate the node store
  ///
  std::vector<TransitionInfo> get_transitions() {
    std::vector<TransitionInfo> ret;

    for (size_t i = 0; i < KEYSPACE_SIZE; i++) {
      if (transitions[i] != 0) {
        TransitionInfo ti;
        ti.to  = transitions[i];
        ti.key = TransitionKey<Transition_T>::value(i);
        ret.push_back(ti);
      }
    }

    if (eof()) {
      TransitionInfo ti;
      ti.to  = eof();
      ti.key = TransitionKey<Transition_T>::eof();
      ret.push_back(ti);
    }
    if (def()) {
      TransitionInfo ti;
      ti.to  = def();
      ti.key = TransitionKey<Transition_T>::def();
      ret.push_back(ti);
    }

    return ret;
  }

  ///
  /// Iterate over every currently existing transition
  /// useful for transition tranformations
  ///
  /// NOTE: Please do not mutate the node store while iterating
  ///
  void each_transition(std::function<void(Key_T key, size_t& ref)> callback)

  {
    for (size_t c = 0; c < KEYSPACE_SIZE; c++) {
      if (transitions[c] != 0) {
        callback(Key_T::value(c), transitions[c]);
      }
    }
    if (eof() != 0) {
      callback(Key_T::eof(), eof());
    }

    if (def() != 0) {
      callback(Key_T::def(), def());
    }
  }

  size_t& eof()
    requires DYNAMIC
  {
    return transitions[eof_idx];
  }

  size_t get_eof() const {
    return transitions[eof_idx];
  }

  ///
  /// We only support bytewise transition fetching
  ///
  size_t get_transition(unsigned char c) const {

    ///
    /// We can ignore the second bit, which makes the keyspace
    /// 25% smaller.
    ///
    return transitions[c & KEY_MASK];
  }

  size_t& def()
    requires DYNAMIC
  {
    return transitions[def_idx];
  }

  ///
  /// Function for trivially accessing transitions
  /// NOTE: Not really suitable for runtime purposes
  ///
  size_t& transition(TransitionKey<Transition_T> key)
    requires DYNAMIC
  {
    switch (key.kind) {
      case key.Eof: return eof();
      case key.Def: return def();
      case key.Val:
        MUTILS_ASSERT_LT(key.val.value(), KEYSPACE_SIZE, "Out of range transition key");
        return transitions[key.val.value()];
    }
  }

private:
  size_t& get_utf8_transition(char32_t key, StateMachineNodeStore<StateMachineNode, 0>& store) {
    // our key is essentially just an array
    // where each element is a transition key
    constexpr auto SET_4 = 0xFF << 24;
    constexpr auto SET_3 = 0xFF << 16;
    constexpr auto SET_2 = 0xFF << 8;


    auto const idx = store.indexof(*this);

    if (key & SET_4) {
      // Top 4 bytes are set / 4-byte utf8 seq
      auto _goto = ((key & SET_4) >> 24) & KEY_MASK;
      auto tzn   = transitions[_goto];
      if (tzn == 0) {
        StateMachineNode n;
        store.push(n);
        store[idx].transitions[_goto] = store.size();
      }

      return store[store[idx].transitions[_goto] - 1].get_utf8_transition(key & ~SET_4, store);

    } else if (key & SET_3) {
      // top 3 bytes are set / 3-byte utf8 seq
      auto const _goto = ((key & SET_3) >> 16) & KEY_MASK;
      auto tzn         = transitions[_goto];

      if (tzn == 0) {
        StateMachineNode n;
        store.push(n);
        store[idx].transitions[_goto] = store.size();
      }

      return store[store[idx].transitions[_goto] - 1].get_utf8_transition(key & ~SET_3, store);
    } else if (key & SET_2) {
      // top 2 bytes are set / 2-byte utf8 seq
      auto const _goto = ((key & SET_2) >> 8) & KEY_MASK;
      auto tzn         = transitions[_goto];

      if (tzn == 0) {
        StateMachineNode n;
        store.push(n);
        store[idx].transitions[_goto] = store.size();
      }

      return store[store[idx].transitions[_goto] - 1].get_utf8_transition(key & ~SET_2, store);
    } else {
      // Base case
      MUTILS_ASSERT_LT(key, KEYSPACE_SIZE, "Out of range transition access");
      return transitions[key & KEY_MASK];
    }
  };
};
}; // namespace regex_backend::internal

    ; // namespace regex_backend::internal
