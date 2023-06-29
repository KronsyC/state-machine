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


#include <cmath>
#include <concepts>
#include <cstddef>
#include <vector>
#include "mutils/assert.h"



namespace regex_backend::internal {


constexpr size_t STORAGE_MAX_TRIVIAL_SIZE_BYTES = 4096;

///
/// If reasonable, we allocate as an array
/// otherwise, we allocate as a ConstSizeMap
///
template <typename K_T, typename V_T> constexpr bool reasonable_expanded_allocation() {
  auto const keyspace         = 1 << sizeof(K_T);
  auto const val_size         = sizeof(V_T);
  auto const total_size_bytes = keyspace * val_size;
  return total_size_bytes <= STORAGE_MAX_TRIVIAL_SIZE_BYTES;
}

template<typename Value_T, size_t SIZE>
struct storage_t{
  using type = std::array<Value_T, SIZE>;
};

template<typename Value_T>
struct storage_t<Value_T, 0>{
  using type = std::vector<Value_T>;
};

template <typename Value_T, size_t SIZE = 0> struct StateMachineNodeStore {
  constexpr static bool IS_DYNAMIC = SIZE == 0;
  using Storage_T = typename storage_t<Value_T, SIZE>::type;

  Storage_T store;

  size_t size() const{
    if constexpr(IS_DYNAMIC){
      return store.size();
    }
    return SIZE;
  }

  size_t indexof(Value_T& val) const{
    const auto idx = &val;
    const auto start = &store[0];


    return idx - start;
  }

  size_t push(Value_T val) requires IS_DYNAMIC{
    store.push_back(val);
    return store.size() - 1;
  };


  Value_T& operator[](size_t idx){
    MUTILS_ASSERT_LT(idx, size(), "Attempt to load node outside of storage");
    return store[idx];
  }
  const Value_T& operator[](size_t idx) const{
    MUTILS_ASSERT_LT(idx, size(), "Attempt to load node outside of storage");
    return store[idx];
  }

  auto begin(){
    return store.begin();
  }
  auto end(){
    return store.end();
  }

  auto rbegin(){
    return store.rbegin();
  }
  auto rend(){
    return store.rend();
  }
};

} // namespace regex_backend::internal
