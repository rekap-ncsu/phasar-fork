/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * MonoSolverTest.cpp
 *
 *  Created on: 06.06.2017
 *      Author: philipp
 */

#include <algorithm>
#include <iostream>

#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

#include <phasar/PhasarLLVM/ControlFlow/LLVMBasedCFG.h>
#include <phasar/Utils/LLVMShorthands.h>

#include <phasar/PhasarLLVM/Mono/Problems/IntraMonoSolverTest.h>
using namespace std;
using namespace psr;

namespace psr {

IntraMonoSolverTest::IntraMonoSolverTest(LLVMBasedCFG &Cfg,
                                         const llvm::Function *F)
    : IntraMonoProblem<const llvm::Instruction *, const llvm::Value *,
                       const llvm::Function *, LLVMBasedCFG &>(Cfg, F) {}

BitVectorSet<const llvm::Value *>
IntraMonoSolverTest::join(const BitVectorSet<const llvm::Value *> &Lhs,
                          const BitVectorSet<const llvm::Value *> &Rhs) {
  cout << "IntraMonoSolverTest::join()\n";
  BitVectorSet<const llvm::Value *> Result;
  Lhs.setUnion(Rhs);
  return Result;
}

bool IntraMonoSolverTest::sqSubSetEqual(
    const BitVectorSet<const llvm::Value *> &Lhs,
    const BitVectorSet<const llvm::Value *> &Rhs) {
  cout << "IntraMonoSolverTest::sqSubSetEqual()\n";
  return Lhs.includes(Rhs);
}

BitVectorSet<const llvm::Value *>
IntraMonoSolverTest::normalFlow(const llvm::Instruction *S,
                                const BitVectorSet<const llvm::Value *> &In) {
  cout << "IntraMonoSolverTest::normalFlow()\n";
  BitVectorSet<const llvm::Value *> Result;
  Result.insert(In);
  if (const auto Store = llvm::dyn_cast<llvm::StoreInst>(S)) {
    Result.insert(Store);
  }
  return Result;
}

unordered_map<const llvm::Instruction *, BitVectorSet<const llvm::Value *>>
IntraMonoSolverTest::initialSeeds() {
  cout << "IntraMonoSolverTest::initialSeeds()\n";
  return {};
}

void IntraMonoSolverTest::printNode(ostream &os,
                                    const llvm::Instruction *n) const {
  os << llvmIRToString(n);
}

void IntraMonoSolverTest::printDataFlowFact(ostream &os,
                                            const llvm::Value *d) const {
  os << llvmIRToString(d);
}

void IntraMonoSolverTest::printMethod(ostream &os,
                                      const llvm::Function *m) const {
  os << m->getName().str();
}

} // namespace psr
