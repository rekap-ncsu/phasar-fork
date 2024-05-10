/******************************************************************************
 * Copyright (c) 2020 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_POINTER_FILTEREDLLVMALIASSET_H
#define PHASAR_PHASARLLVM_POINTER_FILTEREDLLVMALIASSET_H

#include "phasar/Pointer/AliasAnalysisType.h"
#include "phasar/Pointer/AliasInfoTraits.h"
#include "phasar/Pointer/AliasResult.h"
#include "phasar/Pointer/AliasSetOwner.h"
#include "phasar/Utils/AnalysisProperties.h"
#include "phasar/Utils/MaybeUniquePtr.h"

#include "llvm/Support/ErrorHandling.h"

#include "nlohmann/json_fwd.hpp"

#include <type_traits>
#include <utility>

namespace llvm {
class Value;
class Instruction;
class Function;
} // namespace llvm

namespace psr {

class LLVMAliasSet;
class FilteredLLVMAliasSet;

template <>
struct AliasInfoTraits<FilteredLLVMAliasSet>
    : DefaultAATraits<const llvm::Value *, const llvm::Instruction *> {};

class FilteredLLVMAliasSet {
public:
  using traits_t = AliasInfoTraits<FilteredLLVMAliasSet>;
  using n_t = traits_t::n_t;
  using v_t = traits_t::v_t;
  using AliasSetTy = traits_t::AliasSetTy;
  using AliasSetPtrTy = traits_t::AliasSetPtrTy;
  using AllocationSiteSetPtrTy = traits_t::AllocationSiteSetPtrTy;

  FilteredLLVMAliasSet(LLVMAliasSet *AS) noexcept;

  FilteredLLVMAliasSet(const FilteredLLVMAliasSet &) = delete;
  FilteredLLVMAliasSet &operator=(const FilteredLLVMAliasSet &) = delete;
  FilteredLLVMAliasSet &operator=(FilteredLLVMAliasSet &&) = delete;

  FilteredLLVMAliasSet(FilteredLLVMAliasSet &&) noexcept = default;

  ~FilteredLLVMAliasSet();

  template <typename... ArgsT,
            typename = std::enable_if_t<
                std::is_constructible_v<LLVMAliasSet, ArgsT...>>>
  explicit FilteredLLVMAliasSet(ArgsT &&...Args)
      : FilteredLLVMAliasSet(std::forward<ArgsT>(Args)...) {}

  // --- API Functions:

  [[nodiscard]] inline bool isInterProcedural() const noexcept {
    return false;
  };

  [[nodiscard]] AliasAnalysisType getAliasAnalysisType() const noexcept;

  [[nodiscard]] AliasResult alias(const llvm::Value *V1, const llvm::Value *V2,
                                  const llvm::Instruction *I);
  [[nodiscard]] AliasResult alias(const llvm::Value *V1, const llvm::Value *V2,
                                  const llvm::Function *Fun);

  [[nodiscard]] AliasSetPtrTy getAliasSet(const llvm::Value *V,
                                          const llvm::Instruction *I);
  [[nodiscard]] AliasSetPtrTy getAliasSet(const llvm::Value *V,
                                          const llvm::Function *Fun);

  [[nodiscard]] AllocationSiteSetPtrTy
  getReachableAllocationSites(const llvm::Value *V, bool IntraProcOnly = false,
                              const llvm::Instruction *I = nullptr);

  // Checks if PotentialValue is in the reachable allocation sites of V.
  [[nodiscard]] bool isInReachableAllocationSites(
      const llvm::Value *V, const llvm::Value *PotentialValue,
      bool IntraProcOnly = false, const llvm::Instruction *I = nullptr);

  void mergeWith(const FilteredLLVMAliasSet & /*OtherPTI*/) {
    llvm::report_fatal_error("Not Supported");
  }

  void introduceAlias(const llvm::Value * /*V1*/, const llvm::Value * /*V2*/,
                      const llvm::Instruction * /*I*/ = nullptr,
                      AliasResult /*Kind*/ = AliasResult::MustAlias) {
    llvm::report_fatal_error("Not Supported");
  }

  void print(llvm::raw_ostream &OS = llvm::outs()) const;

  [[nodiscard]] nlohmann::json getAsJson() const;

  void printAsJson(llvm::raw_ostream &OS = llvm::outs()) const;

  [[nodiscard]] AnalysisProperties getAnalysisProperties() const noexcept {
    return AnalysisProperties::None;
  }

private:
  FilteredLLVMAliasSet(MaybeUniquePtr<LLVMAliasSet, true> AS) noexcept;

  MaybeUniquePtr<LLVMAliasSet, true> AS;
  AliasSetOwner<AliasSetTy> Owner;
  llvm::DenseMap<std::pair<const llvm::Function *, v_t>, AliasSetPtrTy>
      AliasSetMap;
};
} // namespace psr

#endif // PHASAR_PHASARLLVM_POINTER_FILTEREDLLVMALIASSET_H
