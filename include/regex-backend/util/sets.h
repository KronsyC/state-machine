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

#include <algorithm>
#include <array>
#include <initializer_list>

namespace regex_backend::internal::charsets {

constexpr char const* ALPHABET_LOWER = "abcdefghijklmnopqrstuvwxyz";
constexpr char const* ALPHABET_UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr char const* ALPHABET_FULL  = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
constexpr char const* DIGITS         = "0123456789";
constexpr char const* CONTROL =
    "\001\002\003\004\005\006\007\010\016\017\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037\038\039"
    "\177";


}; // namespace regex_table::internal::charsets
