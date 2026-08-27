// SPDX-FileCopyrightText: 2026 The ng-log contributors
// SPDX-License-Identifier: BSD-3-Clause
//
// Author: Sergiu Deitsch

#ifndef NGLOG_INTERNAL_CHECKED_ARITHMETIC_H_
#define NGLOG_INTERNAL_CHECKED_ARITHMETIC_H_

#include <limits>
#include <type_traits>

namespace nglog {
namespace internal {

// Returns whether adding left and right cannot be represented by T.
template <typename T>
constexpr auto DoesAdditionOverflow(const T left, const T right)
    -> std::enable_if_t<
        std::is_integral<T>::value && std::is_unsigned<T>::value, bool> {
  return right > std::numeric_limits<T>::max() - left;
}

// Returns whether adding left and right cannot be represented by T.
template <typename T>
constexpr auto DoesAdditionOverflow(const T left, const T right)
    -> std::enable_if_t<std::is_integral<T>::value && std::is_signed<T>::value,
                        bool> {
  return (right > 0 && left > std::numeric_limits<T>::max() - right) ||
         (right < 0 && left < std::numeric_limits<T>::min() - right);
}

// Adds left and right to result when their sum is representable by T.
template <typename T>
constexpr auto CheckedAdd(const T left, const T right, T& result)
    -> std::enable_if_t<std::is_integral<T>::value, bool> {
  if (DoesAdditionOverflow(left, right)) {
    return false;
  }
  result = left + right;
  return true;
}

// Returns whether multiplying left and right cannot be represented by T.
template <typename T>
constexpr auto DoesMultiplicationOverflow(const T left, const T right)
    -> std::enable_if_t<
        std::is_integral<T>::value && std::is_unsigned<T>::value, bool> {
  return left != 0 && right > std::numeric_limits<T>::max() / left;
}

// Returns whether multiplying left and right cannot be represented by T.
//
// Each sign combination needs its own bound: comparing against max() / left
// alone is only meaningful when both operands are positive, and for the
// remaining combinations it both rejects representable products and, worse,
// accepts ones that overflow.
template <typename T>
constexpr auto DoesMultiplicationOverflow(const T left, const T right)
    -> std::enable_if_t<std::is_integral<T>::value && std::is_signed<T>::value,
                        bool> {
  if (left == 0 || right == 0) {
    return false;
  }

  if (left > 0) {
    return right > 0 ? left > std::numeric_limits<T>::max() / right
                     : right < std::numeric_limits<T>::min() / left;
  }

  return right > 0 ? left < std::numeric_limits<T>::min() / right
                   : right < std::numeric_limits<T>::max() / left;
}

// Multiplies left and right if their product fits in T.
template <typename T>
constexpr auto CheckedMultiply(const T left, const T right, T& result)
    -> std::enable_if_t<std::is_integral<T>::value, bool> {
  if (DoesMultiplicationOverflow(left, right)) {
    return false;
  }
  result = left * right;
  return true;
}

}  // namespace internal
}  // namespace nglog

#endif  // NGLOG_INTERNAL_CHECKED_ARITHMETIC_H_
