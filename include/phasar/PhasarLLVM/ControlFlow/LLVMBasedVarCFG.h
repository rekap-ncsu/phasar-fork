/******************************************************************************
 * Copyright (c) 2020 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel, Philipp Schubert and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_CONTROLFLOW_LLVMBASEDVARCFG_H_
#define PHASAR_PHASARLLVM_CONTROLFLOW_LLVMBASEDVARCFG_H_

#include <unordered_map>
#include <utility>
#include <vector>

#include <z3++.h>

#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCFG.h"
#include "phasar/PhasarLLVM/ControlFlow/VarCFG.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/VarStaticRenaming.h"
#include <llvm/ADT/StringMap.h>
#include <optional>

namespace llvm {
class Function;
class Instruction;
class BranchInst;
class BinaryOperator;
class LoadInst;
class CmpInst;
class Constant;
class Value;
class GlobalVariable;
} // namespace llvm

namespace psr {

class ProjectIRDB;

class LLVMBasedVarCFG
    : public virtual LLVMBasedCFG,
      public virtual VarCFG<const llvm::Instruction *, const llvm::Function *,
                            z3::expr> {
private:
  // TODO: check if those variables need to be mutable, i.e. the z3
  // related member functions need to be const.
  mutable z3::context CTX;
  llvm::StringMap<z3::expr> AvailablePPConditions;
  const stringstringmap_t *staticBackwardRenaming = nullptr;
  // mutable std::unordered_map<std::string, z3::expr> PPVariables;

  bool isPPBranchNode(const llvm::BranchInst *BI) const;

  bool isPPBranchNode(const llvm::BranchInst *BI, z3::expr &Cond) const;

  // z3::expr inferCondition(const llvm::CmpInst *Cmp) const;

  // z3::expr createExpression(const llvm::Value *V) const;

  // z3::expr createBinOp(const llvm::BinaryOperator *OP) const;

  // z3::expr createVariableOrGlobal(const llvm::LoadInst *LI) const;

  // z3::expr createConstant(const llvm::Constant *C) const;

  // z3::expr compareBoolAndInt(z3::expr XBool, z3::expr XInt,
  //                            bool ForEquality) const;

  // bool isPPVariable(const llvm::GlobalVariable *G, std::string &Name) const;

  // don't pass by reference, as we need to take ownership of the Name
  // z3::expr createBoolConst(std::string Name) const;
  // don't pass by reference, as we need to take ownership of the Name
  // z3::expr createIntConst(std::string Name) const;

  std::optional<z3::expr>
  getConditionIfIsPPVariable(const llvm::GlobalVariable *G) const;

  z3::expr deserializePPCondition(llvm::StringRef Cond) const;

public:
  LLVMBasedVarCFG(const ProjectIRDB &IRDB,
                  const stringstringmap_t *StaticBackwardRenaming = nullptr);

  ~LLVMBasedVarCFG() override = default;

  std::vector<std::pair<const llvm::Instruction *, z3::expr>>
  getSuccsOfWithPPConstraints(const llvm::Instruction *Stmt) const override;

  bool isPPBranchTarget(const llvm::Instruction *Stmt,
                        const llvm::Instruction *Succ) const override;

  z3::expr getPPConstraintOrTrue(const llvm::Instruction *Stmt,
                                 const llvm::Instruction *Succ) const override;

  z3::expr getTrueConstraint() const override;

  z3::context &getContext() const;

  [[nodiscard]] std::string
  getDemangledFunctionName(const llvm::Function *Fun) const override;
};

} // namespace psr

#endif