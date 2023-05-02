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


#include <sstream>
#include <string>
#include <type_traits>

namespace mutils {

template <typename T>
concept IsJavaStyleStringCastable = requires(std::string& tgt, T val) { tgt = val.toString(); };

template<typename T>
concept IsSTLToStringCompatible = requires(T val){ std::to_string(val); };

template<typename T>
concept IsStreamable = requires(T val, std::stringstream ss){ ss << val; };

template<typename T>
concept IsStringCastable = std::is_convertible_v<T, std::string>;
//
// Cast the 'val' to a string using the `operator std::string` function
//
template <typename T>
std::string stringify(T val)
  requires IsStringCastable<T>
{
  // Implicit/Explicit castability
  return std::string(val);
}

//
// Cast the 'val' to a string using the `std::to_string` function
//

template<typename T>
std::string stringify(T val) requires IsSTLToStringCompatible<T> && (!IsStringCastable<T>)
{
  return std::to_string(val);
}

//
// Cast the 'val' to a string using the `toString` function
//

template <typename T>
std::string stringify(T val)
  requires IsJavaStyleStringCastable<T> && (!IsSTLToStringCompatible<T> && !IsStringCastable<T>)
{
  // many people implement the toString function,
  // coming from java
  return val.toString();
}
//
// Cast the 'val' to a string using the `operator<<(std::stringstream&)` function
//
template <typename T>
std::string stringify(T val)
  requires IsStreamable<T> && (!IsSTLToStringCompatible<T> && !IsStringCastable<T> && !IsJavaStyleStringCastable<T>)
{
  std::stringstream ss;
  ss << val;
  return ss.str();
}

}; // namespace mutils
