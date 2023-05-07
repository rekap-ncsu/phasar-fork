/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#ifndef PHASAR_UTILS_UTILITIES_H_
#define PHASAR_UTILS_UTILITIES_H_

#include "phasar/Utils/BitVectorSet.h"
#include "phasar/Utils/TypeTraits.h"

#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/SmallVector.h"

#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace llvm {
class Type;
} // namespace llvm

namespace psr {

std::string createTimeStamp();

bool isConstructor(llvm::StringRef MangledName);

const llvm::Type *stripPointer(const llvm::Type *Pointer);

/// \brief Computes the set-intersection of the potentially unordered sets
/// Dest and Src and stores the result back in Dest.
///
/// This function should work on all types of sets as long as they provide the
/// type value_type specifying, which type their elements have.
/// By-reference iteration is required, but the elements do not have any
/// requirements, although the performance is probably higher for small
/// elements that are trivially copyable.
template <typename ContainerTy, typename OtherContainerTy>
std::enable_if_t<!has_erase_iterator_v<ContainerTy>>
intersectWith(ContainerTy &Dest, const OtherContainerTy &Src) {
  static_assert(std::is_same_v<typename ContainerTy::value_type,
                               typename OtherContainerTy::value_type>,
                "The containers Src and Dest must be compatible");
  using ValueTy = typename ContainerTy::value_type;
  using ElementTy =
      std::conditional_t<std::is_trivially_copy_constructible_v<ValueTy> &&
                             sizeof(ValueTy) <= sizeof(void *),
                         ValueTy, ValueTy *>;

  auto removeFrom = [](auto &Dst, auto &&Elem) { // NOLINT
    if constexpr (std::is_same_v<ValueTy, ElementTy>) {
      Dst.erase(Elem);
    } else {
      Dst.erase(*Elem);
    }
  };

  /// This whole functionality is only for computing the set-intersection of
  /// Dest and Src storing the result in-place in Dest. It would be a big win,
  /// if the STL would provide us with in-place set operations and those that
  /// do not require the sets to be sorted...

  llvm::SmallVector<ElementTy, 16> Buffer;

  if constexpr (std::is_same_v<ValueTy, ElementTy>) {
    for (auto &&Elem : Dest) {
      if (!Src.count(Elem)) {
        Buffer.push_back(Elem);
      }
    }
  } else {
    for (auto &Elem : Dest) {
      if (!Src.count(Elem)) {
        Buffer.insert(&Elem);
      }
    }
  }

  for (auto &&Elem : Buffer) {
    removeFrom(Dest, Elem);
  }
}

template <typename ContainerTy, typename OtherContainerTy>
std::enable_if_t<has_erase_iterator_v<ContainerTy>>
intersectWith(ContainerTy &Dest, const OtherContainerTy &Src) {
  static_assert(std::is_same_v<typename ContainerTy::value_type,
                               typename OtherContainerTy::value_type>,
                "The containers Src and Dest must be compatible");

  for (auto It = Dest.begin(), End = Dest.end(); It != End;) {
    if (Src.count(*It)) {
      ++It;
    } else {
      It = Dest.erase(It);
    }
  }
}

template <typename T>
void intersectWith(BitVectorSet<T> &Dest, const BitVectorSet<T> &Src) {
  Dest.setIntersectWith(Src);
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                              const std::vector<bool> &Bits);

struct StringIDLess {
  bool operator()(const std::string &LHS, const std::string &RHS) const;
};

/// See "https://en.cppreference.com/w/cpp/experimental/scope_exit/scope_exit"
template <typename Fn> class scope_exit { // NOLINT
public:
  template <typename FFn, typename = decltype(std::declval<FFn>()())>
  scope_exit(FFn &&F) noexcept(std::is_nothrow_constructible_v<Fn, FFn &&>)
      : F(std::forward<FFn>(F)) {}

  ~scope_exit() noexcept { F(); }

  scope_exit(const scope_exit &) = delete;
  scope_exit(scope_exit &&) = delete;

  scope_exit &operator=(const scope_exit &) = delete;
  scope_exit &operator=(scope_exit &&) = delete;

private:
  Fn F;
};

template <typename Fn> scope_exit(Fn) -> scope_exit<Fn>;

// Copied from "https://en.cppreference.com/w/cpp/utility/variant/visit"
template <class... Ts> struct Overloaded : Ts... { using Ts::operator()...; };

// explicit deduction guide (not needed as of C++20)
template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

/// Based on the reference implementation of std::remove_if
/// "https://en.cppreference.com/w/cpp/algorithm/remove" and optimized for the
/// case that a sorted list of indices is given instead of an unary predicate
/// specifying the elements to be removed.
template <typename It, typename EndIt, typename IdxIt,
          typename IdxEndIt> // NOLINTNEXTLINE(readability-identifier-naming)
It remove_by_index(It First, EndIt Last, IdxIt FirstIndex, IdxEndIt LastIndex) {
  if (FirstIndex == LastIndex || First == Last) {
    return Last;
  }

  if constexpr (std::is_same_v<It, EndIt> &&
                std::is_same_v<
                    std::random_access_iterator_tag,
                    typename std::iterator_traits<It>::iterator_category>) {
    /// Random-access version inspired from
    /// "https://codereview.stackexchange.com/a/207056" and slightly changed

    auto Bounds = size_t(std::distance(First, Last));
    auto Out = std::next(First, *FirstIndex);
    auto In = std::next(Out);
    while (++FirstIndex != LastIndex) {
      auto CurrIdx = *FirstIndex;
      if (*std::prev(FirstIndex) + 1 == CurrIdx) {
        ++In;
        continue;
      }
      if (LLVM_UNLIKELY(CurrIdx >= Bounds)) {
        break;
      }

      auto Tar = std::next(First, CurrIdx);
      Out = std::move(In, Tar, Out);
      In = std::next(Tar);
    }
    return std::move(In, Last, Out);
  } else {
    auto CurrIdx = *FirstIndex;
    First = std::next(First, CurrIdx);
    if (First == Last) {
      return First;
    }
    for (auto I = First; I != Last; ++CurrIdx, ++I) {
      if (CurrIdx != *FirstIndex) {
        *First++ = std::move(*I);
        if (++FirstIndex == LastIndex) {
          return std::move(std::next(I), Last, First);
        }
      }
    }
    return First;
  }
}

template <typename Container, typename IdxIt,
          typename IdxEndIt> // NOLINTNEXTLINE(readability-identifier-naming)
auto remove_by_index(Container &Cont, IdxIt FirstIndex, IdxEndIt LastIndex) {
  using std::begin;
  using std::end;

  return remove_by_index(begin(Cont), end(Cont), std::move(FirstIndex),
                         std::move(LastIndex));
}

template <typename Container,
          typename Indices> // NOLINTNEXTLINE(readability-identifier-naming)
auto remove_by_index(Container &Cont, const Indices &Idx) {
  using std::begin;
  using std::end;

  return remove_by_index(begin(Cont), end(Cont), begin(Idx), end(Idx));
}

} // namespace psr

#endif
